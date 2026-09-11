#include "PluginProcessor.h"
#include "PluginEditor.h"

// ==============================================================================
// AutoLevelerAudioProcessor コンストラクタ
// ==============================================================================
AutoLevelerAudioProcessor::AutoLevelerAudioProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout()),
      visualFifoBuffer (static_cast<size_t>(fifoCapacity))
{
    inputGainParam       = apvts.getRawParameterValue (ParameterIDs::inputGain);
    targetLevelParam     = apvts.getRawParameterValue (ParameterIDs::targetLevel);
    rangeParam           = apvts.getRawParameterValue (ParameterIDs::range);
    speedParam           = apvts.getRawParameterValue (ParameterIDs::speed);
    outputGainParam      = apvts.getRawParameterValue (ParameterIDs::outputGain);
    lookaheadEnableParam = apvts.getRawParameterValue (ParameterIDs::lookaheadEnable);
    detectionModeParam   = apvts.getRawParameterValue (ParameterIDs::detectionMode);
    timingModeParam      = apvts.getRawParameterValue (ParameterIDs::timingMode);
    syncSpeedParam       = apvts.getRawParameterValue (ParameterIDs::syncSpeed);
    meterModeParam       = apvts.getRawParameterValue (ParameterIDs::meterMode);
    guiEnableParam       = apvts.getRawParameterValue (ParameterIDs::guiEnable);
    bypassParam          = apvts.getRawParameterValue (ParameterIDs::bypass);

    tameNoiseEnableParam = apvts.getRawParameterValue (ParameterIDs::tameNoiseEnable);
    tameNoiseListenParam = apvts.getRawParameterValue (ParameterIDs::tameNoiseListen);
    tameNoiseAmountParam = apvts.getRawParameterValue (ParameterIDs::tameNoiseAmount);
    scFilterEnableParam  = apvts.getRawParameterValue (ParameterIDs::scFilterEnable);

    // ファクトリープリセット (TameNoise & SC Filter 対応)
    presets.push_back ({ "Default", 0.0f, -12.0f, 6.0f, 50.0f, 0.0f, true, true, 30.0f, true, 0, 0, 1 });
    presets.push_back ({ "Synth Vocal", 0.0f, -14.0f, 6.0f, 75.0f, 0.0f, true, true, 45.0f, true, 0, 0, 1 });
    presets.push_back ({ "Gentle Vocal Ride", 0.0f, -14.0f, 4.0f, 35.0f, 0.0f, true, true, 25.0f, true, 0, 0, 1 });
    presets.push_back ({ "Aggressive Leveler", 0.0f, -10.0f, 10.0f, 75.0f, 0.0f, true, true, 50.0f, true, 0, 0, 1 });
    presets.push_back ({ "Podcast / Spoken", 0.0f, -16.0f, 8.0f, 60.0f, 0.0f, true, true, 40.0f, true, 0, 0, 1 });
    presets.push_back ({ "Ballad Dynamic", 0.0f, -18.0f, 8.0f, 30.0f, 0.0f, true, true, 20.0f, true, 0, 0, 1 });
    presets.push_back ({ "Peak Catching Leveler", 0.0f, -12.0f, 6.0f, 70.0f, 0.0f, true, true, 35.0f, true, 1, 0, 1 });
    presets.push_back ({ "Broadcast Rider", 0.0f, -14.0f, 5.0f, 45.0f, 0.0f, true, true, 30.0f, true, 0, 1, 1 });

    loadUserPresets();
}

AutoLevelerAudioProcessor::~AutoLevelerAudioProcessor()
{
}

const juce::String AutoLevelerAudioProcessor::getName() const
{
    return "Atafuta09Leveler";
}

bool AutoLevelerAudioProcessor::acceptsMidi() const { return false; }
bool AutoLevelerAudioProcessor::producesMidi() const { return false; }
bool AutoLevelerAudioProcessor::isMidiEffect() const { return false; }
double AutoLevelerAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int AutoLevelerAudioProcessor::getNumPrograms()
{
    return static_cast<int>(presets.size());
}

int AutoLevelerAudioProcessor::getCurrentProgram()
{
    return currentProgram;
}

void AutoLevelerAudioProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < static_cast<int>(presets.size()))
    {
        currentProgram = index;
        const auto& p = presets[static_cast<size_t>(index)];

        if (auto* param = apvts.getParameter (ParameterIDs::inputGain))
            param->setValueNotifyingHost (param->convertTo0to1 (p.inGain));
        if (auto* param = apvts.getParameter (ParameterIDs::targetLevel))
            param->setValueNotifyingHost (param->convertTo0to1 (p.targetLevel));
        if (auto* param = apvts.getParameter (ParameterIDs::range))
            param->setValueNotifyingHost (param->convertTo0to1 (p.range));
        if (auto* param = apvts.getParameter (ParameterIDs::speed))
            param->setValueNotifyingHost (param->convertTo0to1 (p.speed));
        if (auto* param = apvts.getParameter (ParameterIDs::outputGain))
            param->setValueNotifyingHost (param->convertTo0to1 (p.outGain));
        if (auto* param = apvts.getParameter (ParameterIDs::lookaheadEnable))
            param->setValueNotifyingHost (p.lookahead ? 1.0f : 0.0f);
        if (auto* param = apvts.getParameter (ParameterIDs::tameNoiseEnable))
            param->setValueNotifyingHost (p.tameNoise ? 1.0f : 0.0f);
        if (auto* param = apvts.getParameter (ParameterIDs::tameNoiseAmount))
            param->setValueNotifyingHost (param->convertTo0to1 (p.tameAmount));
        if (auto* param = apvts.getParameter (ParameterIDs::scFilterEnable))
            param->setValueNotifyingHost (p.scFilter ? 1.0f : 0.0f);
        if (auto* param = apvts.getParameter (ParameterIDs::detectionMode))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float>(p.detectionMode)));
        if (auto* param = apvts.getParameter (ParameterIDs::timingMode))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float>(p.timingMode)));
        if (auto* param = apvts.getParameter (ParameterIDs::syncSpeed))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float>(p.syncSpeed)));
    }
}

const juce::String AutoLevelerAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < static_cast<int>(presets.size()))
        return presets[static_cast<size_t>(index)].name;
    return {};
}

void AutoLevelerAudioProcessor::changeProgramName (int /*index*/, const juce::String& /*newName*/)
{
}

void AutoLevelerAudioProcessor::loadUserPresets()
{
    auto userDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("AtafutaAudio")
                       .getChildFile ("AutoLeveler");
    auto file = userDir.getChildFile ("UserPresets.xml");
    if (!file.existsAsFile()) return;

    auto xml = juce::XmlDocument::parse (file);
    if (xml != nullptr && xml->hasTagName ("AutoLevelerUserPresets"))
    {
        for (auto* child : xml->getChildIterator())
        {
            if (child->hasTagName ("Preset"))
            {
                Preset p;
                p.name          = child->getStringAttribute ("name", "Custom Preset");
                p.inGain        = static_cast<float>(child->getDoubleAttribute ("inGain", 0.0));
                p.targetLevel   = static_cast<float>(child->getDoubleAttribute ("target", -12.0));
                p.range         = static_cast<float>(child->getDoubleAttribute ("range", 6.0));
                p.speed         = static_cast<float>(child->getDoubleAttribute ("speed", 50.0));
                p.outGain       = static_cast<float>(child->getDoubleAttribute ("outGain", 0.0));
                p.lookahead     = child->getBoolAttribute ("lookahead", true);
                p.tameNoise     = child->getBoolAttribute ("tameNoise", true);
                p.tameAmount    = static_cast<float>(child->getDoubleAttribute ("tameAmount", 30.0));
                p.scFilter      = child->getBoolAttribute ("scFilter", true);
                p.detectionMode = child->getIntAttribute ("det", 0);
                p.timingMode    = child->getIntAttribute ("timing", 0);
                p.syncSpeed     = child->getIntAttribute ("syncSpeed", 1);

                presets.push_back (p);
            }
        }
    }
}

void AutoLevelerAudioProcessor::saveUserPresetsToFile()
{
    auto userDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                       .getChildFile ("AtafutaAudio")
                       .getChildFile ("AutoLeveler");
    userDir.createDirectory();
    auto file = userDir.getChildFile ("UserPresets.xml");

    auto rootXml = std::make_unique<juce::XmlElement> ("AutoLevelerUserPresets");
    for (size_t i = 8; i < presets.size(); ++i)
    {
        const auto& p = presets[i];
        auto* child = rootXml->createNewChildElement ("Preset");
        child->setAttribute ("name", p.name);
        child->setAttribute ("inGain", static_cast<double>(p.inGain));
        child->setAttribute ("target", static_cast<double>(p.targetLevel));
        child->setAttribute ("range", static_cast<double>(p.range));
        child->setAttribute ("speed", static_cast<double>(p.speed));
        child->setAttribute ("outGain", static_cast<double>(p.outGain));
        child->setAttribute ("lookahead", p.lookahead);
        child->setAttribute ("tameNoise", p.tameNoise);
        child->setAttribute ("tameAmount", static_cast<double>(p.tameAmount));
        child->setAttribute ("scFilter", p.scFilter);
        child->setAttribute ("det", p.detectionMode);
        child->setAttribute ("timing", p.timingMode);
        child->setAttribute ("syncSpeed", p.syncSpeed);
    }

    rootXml->writeTo (file, {});
}

bool AutoLevelerAudioProcessor::saveUserPreset (const juce::String& presetName)
{
    if (presetName.trim().isEmpty()) return false;

    Preset newPreset;
    newPreset.name          = presetName.trim();
    newPreset.inGain        = inputGainParam != nullptr ? inputGainParam->load() : 0.0f;
    newPreset.targetLevel   = targetLevelParam != nullptr ? targetLevelParam->load() : -12.0f;
    newPreset.range         = rangeParam != nullptr ? rangeParam->load() : 6.0f;
    newPreset.speed         = speedParam != nullptr ? speedParam->load() : 50.0f;
    newPreset.outGain       = outputGainParam != nullptr ? outputGainParam->load() : 0.0f;
    newPreset.lookahead     = lookaheadEnableParam != nullptr ? (lookaheadEnableParam->load() > 0.5f) : true;
    newPreset.tameNoise     = tameNoiseEnableParam != nullptr ? (tameNoiseEnableParam->load() > 0.5f) : true;
    newPreset.tameAmount    = tameNoiseAmountParam != nullptr ? tameNoiseAmountParam->load() : 30.0f;
    newPreset.scFilter      = scFilterEnableParam  != nullptr ? (scFilterEnableParam->load() > 0.5f) : true;
    newPreset.detectionMode = detectionModeParam != nullptr ? juce::roundToInt (detectionModeParam->load()) : 0;
    newPreset.timingMode    = timingModeParam != nullptr ? juce::roundToInt (timingModeParam->load()) : 0;
    newPreset.syncSpeed     = syncSpeedParam != nullptr ? juce::roundToInt (syncSpeedParam->load()) : 1;

    presets.push_back (newPreset);
    currentProgram = static_cast<int>(presets.size()) - 1;

    saveUserPresetsToFile();
    return true;
}

// ==============================================================================
// パラメータレイアウトの作成
// ==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AutoLevelerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::inputGain, 1 },
        "Input Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::targetLevel, 1 },
        "Target Level",
        juce::NormalisableRange<float> (-36.0f, 0.0f, 0.1f),
        -12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::range, 1 },
        "Range",
        juce::NormalisableRange<float> (0.0f, 13.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::speed, 1 },
        "Speed",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
    ));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputGain, 1 },
        "Output Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::lookaheadEnable, 1 },
        "Lookahead",
        true
    ));

    // --- TameNoise & 音質変化0 サイドチェインフィルター ---
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::tameNoiseEnable, 1 },
        "Tame Noise",
        true
    ));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::tameNoiseListen, 1 },
        "Tame Listen",
        false
    ));

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::tameNoiseAmount, 1 },
        "Tame Amount",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f),
        30.0f, // ユーザー指定デフォルト 30%!
        juce::AudioParameterFloatAttributes().withLabel ("%")
    ));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::scFilterEnable, 1 },
        "SC Filter",
        true
    ));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::detectionMode, 1 },
        "Detection Mode",
        juce::StringArray { "RMS", "Peak" },
        0
    ));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::timingMode, 1 },
        "Timing Mode",
        juce::StringArray { "Free (ms)", "BPM Sync" },
        0
    ));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::syncSpeed, 1 },
        "Sync Speed",
        juce::StringArray { "Fast (1/64)", "Mid (1/32)", "Slow (1/16)" },
        1
    ));

    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::meterMode, 1 },
        "Meter Mode",
        juce::StringArray { "Peak", "RMS", "VU" },
        0
    ));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::guiEnable, 1 },
        "GUI Animation",
        true
    ));

    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 },
        "Bypass",
        false
    ));

    return { params.begin(), params.end() };
}

// ==============================================================================
// 処理準備 (prepareToPlay)
// ==============================================================================
void AutoLevelerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    // Lookahead ディレイバッファの初期化 (原音スルー用)
    lookaheadSamples = juce::roundToInt (static_cast<float>(sampleRate) * (lookaheadMs * 0.001f));
    delayBufferSize = lookaheadSamples + samplesPerBlock + 128;
    delayBuffer.setSize (2, delayBufferSize);
    delayBuffer.clear();
    delayBufferWritePos = 0;

    smoothedInputGainDb.reset (currentSampleRate, 0.02);
    smoothedInputGainDb.setCurrentAndTargetValue (inputGainParam != nullptr ? inputGainParam->load() : 0.0f);

    smoothedOutputGainDb.reset (currentSampleRate, 0.02);
    smoothedOutputGainDb.setCurrentAndTargetValue (outputGainParam != nullptr ? outputGainParam->load() : 0.0f);

    // 音質変化ゼロのサイドチェイン検出専用 HPF (100Hz)
    sidechainDetectorHPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 100.0f);
    sidechainDetectorHPF.reset();

    // TameNoise 16kHz リサンプラー & エンジン初期化
    tameNoiseEngine.reset();
    tameRingBuffer16k.assign (TAME_BLOCK_SIZE * 4, 0.0f);
    tameRingWritePos = 0;
    tameSamplesSinceLastAnalysis = 0;
    tameResampleRatio = TAME_INTERNAL_SR / currentSampleRate;
    tameResamplePhase = 0.0;
    tameLastDawSample = 0.0f;

    tameAuditionGain.reset (currentSampleRate, 0.015);
    tameAuditionGain.setCurrentAndTargetValue (0.0f);

    fastEnvelopeRms = 0.0f;
    fastEnvelopePeak = 0.0f;
    currentSmoothedGainDb = 0.0f;
    smoothedRmsIn = 0.0f;
    smoothedRmsOut = 0.0f;
    smoothedVuIn = 0.0f;
    smoothedVuOut = 0.0f;

    downsampleInterval = juce::roundToInt (static_cast<float>(currentSampleRate) / 100.0f);
    if (downsampleInterval < 1) downsampleInterval = 1;
    downsampleCounter = 0;
    inputAccumSumSquares = 0.0f;
    outputAccumSumSquares = 0.0f;
    gainChangeAccumDb = 0.0f;

    visualFifo.reset();
    std::fill (visualFifoBuffer.begin(), visualFifoBuffer.end(), VisualDataPoint{});
}

void AutoLevelerAudioProcessor::releaseResources()
{
}

bool AutoLevelerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

// ==============================================================================
// オーディオ処理 (processBlock)
// ==============================================================================
void AutoLevelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // ホストBPMの取得
    if (auto* playHead = getPlayHead())
    {
        if (auto posOpt = playHead->getPosition())
        {
            if (posOpt->getBpm().hasValue())
                currentBpm.store (static_cast<float>(*posOpt->getBpm()), std::memory_order_relaxed);
        }
    }

    // パラメータ値の取得
    const float inGainDb          = inputGainParam       != nullptr ? inputGainParam->load (std::memory_order_relaxed)        : 0.0f;
    const float targetLevelDb     = targetLevelParam     != nullptr ? targetLevelParam->load (std::memory_order_relaxed)      : -12.0f;
    const float rangeDb           = rangeParam           != nullptr ? rangeParam->load (std::memory_order_relaxed)            : 6.0f;
    const float speedVal          = speedParam           != nullptr ? speedParam->load (std::memory_order_relaxed)            : 50.0f;
    const float outGainDb         = outputGainParam      != nullptr ? outputGainParam->load (std::memory_order_relaxed)       : 0.0f;
    const bool isLookaheadOn      = lookaheadEnableParam != nullptr && (lookaheadEnableParam->load (std::memory_order_relaxed) > 0.5f);
    const bool isPeakDetection    = detectionModeParam   != nullptr && (detectionModeParam->load (std::memory_order_relaxed)   > 0.5f);
    const bool isSyncMode         = timingModeParam      != nullptr && (timingModeParam->load (std::memory_order_relaxed)      > 0.5f);
    const int syncSpeedChoice     = syncSpeedParam       != nullptr ? juce::roundToInt (syncSpeedParam->load (std::memory_order_relaxed)) : 1;
    const bool isGuiEnabled       = guiEnableParam       != nullptr && (guiEnableParam->load (std::memory_order_relaxed)       > 0.5f);
    const bool isBypassed         = bypassParam          != nullptr && (bypassParam->load (std::memory_order_relaxed)          > 0.5f);

    const bool isTameNoiseOn      = tameNoiseEnableParam != nullptr && (tameNoiseEnableParam->load (std::memory_order_relaxed) > 0.5f);
    const bool isTameListenOn     = tameNoiseListenParam != nullptr && (tameNoiseListenParam->load (std::memory_order_relaxed) > 0.5f);
    const float tameAmountNorm    = tameNoiseAmountParam != nullptr ? (tameNoiseAmountParam->load (std::memory_order_relaxed) * 0.01f) : 0.30f;
    const bool isScFilterOn       = scFilterEnableParam  != nullptr && (scFilterEnableParam->load (std::memory_order_relaxed) > 0.5f);

    // DAWレイテンシー報告
    const int targetLatency = (isLookaheadOn && !isBypassed) ? lookaheadSamples : 0;
    if (getLatencySamples() != targetLatency)
        setLatencySamples (targetLatency);

    // バイパス時の高速処理
    if (isBypassed)
    {
        latestInputPeak.store (0.0f, std::memory_order_relaxed);
        latestOutputPeak.store (0.0f, std::memory_order_relaxed);
        latestInputRms.store (0.0f, std::memory_order_relaxed);
        latestOutputRms.store (0.0f, std::memory_order_relaxed);
        latestInputVu.store (0.0f, std::memory_order_relaxed);
        latestOutputVu.store (0.0f, std::memory_order_relaxed);
        return;
    }

    smoothedInputGainDb.setTargetValue (inGainDb);
    smoothedOutputGainDb.setTargetValue (outGainDb);

    // 時定数計算
    const auto timing = calculateTiming (speedVal, isSyncMode, syncSpeedChoice, currentBpm.load (std::memory_order_relaxed));
    const float attackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float>(currentSampleRate) * (timing.attackMs * 0.001f)));
    const float releaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float>(currentSampleRate) * (timing.releaseMs * 0.001f)));

    constexpr float rmsWindowMs = 25.0f;
    const float rmsEnvCoeff = 1.0f - std::exp (-1.0f / (static_cast<float>(currentSampleRate) * (rmsWindowMs * 0.001f)));
    constexpr float peakWindowMs = 3.0f;
    const float peakEnvCoeff = 1.0f - std::exp (-1.0f / (static_cast<float>(currentSampleRate) * (peakWindowMs * 0.001f)));

    float blockMaxInputPeak = 0.0f;
    float blockMaxOutputPeak = 0.0f;

    const float* inL = buffer.getReadPointer (0);
    const float* inR = (numChannels > 1) ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = (numChannels > 1) ? buffer.getWritePointer (1) : outL;

    const int ringSize = static_cast<int>(tameRingBuffer16k.size());

    for (int i = 0; i < numSamples; ++i)
    {
        // 1. 入力ゲイン
        const float inG = juce::Decibels::decibelsToGain (smoothedInputGainDb.getNextValue());
        const float sampleInL = inL[i] * inG;
        const float sampleInR = inR[i] * inG;
        const float monoDetectorInput = 0.5f * (sampleInL + sampleInR);

        // 2. Lookahead 遅延バッファへの書き込み (原音スルー用: 音質変化0!)
        delayBuffer.setSample (0, delayBufferWritePos, sampleInL);
        if (numChannels > 1)
            delayBuffer.setSample (1, delayBufferWritePos, sampleInR);

        // 3. TameNoise 16kHz リサンプリング & 解析 (87データ学習済みエンジン)
        tameResamplePhase += tameResampleRatio;
        while (tameResamplePhase >= 1.0)
        {
            tameResamplePhase -= 1.0;
            const float interp = tameLastDawSample + static_cast<float>(tameResamplePhase) * (monoDetectorInput - tameLastDawSample);
            tameRingBuffer16k[tameRingWritePos] = interp;
            tameRingWritePos = (tameRingWritePos + 1) % ringSize;
            tameSamplesSinceLastAnalysis++;

            if (tameSamplesSinceLastAnalysis >= TAME_HOP_SIZE)
            {
                tameSamplesSinceLastAnalysis = 0;
                std::array<float, TAME_BLOCK_SIZE> blockBuffer;
                int readPos = (tameRingWritePos - TAME_BLOCK_SIZE + ringSize) % ringSize;
                for (int b = 0; b < TAME_BLOCK_SIZE; ++b)
                {
                    blockBuffer[b] = tameRingBuffer16k[readPos];
                    readPos = (readPos + 1) % ringSize;
                }

                auto match = tameNoiseEngine.analyzeBlock (blockBuffer.data(), TAME_BLOCK_SIZE);
                tameNoiseSibilanceScore.store (match.sibilanceScore, std::memory_order_relaxed);
                tameNoiseBreathScore.store (match.breathScore, std::memory_order_relaxed);
                tameNoiseNormalScore.store (match.normalScore, std::memory_order_relaxed);

                const float maxNoise = std::max ({ match.sibilanceScore, match.breathScore, match.plosiveScore });
                tameNoiseTrigger.store (maxNoise > 0.40f, std::memory_order_relaxed);
            }
        }
        tameLastDawSample = monoDetectorInput;

        // 4. 音質変化ゼロのサイドチェイン検出信号
        // (検出器のみに100Hz HPFをかけ、低音吹かれ・ゴトつきによるポンピングを完全防止!)
        float filteredDetector = monoDetectorInput;
        if (isScFilterOn)
            filteredDetector = sidechainDetectorHPF.processSample (filteredDetector);

        // 5. 検出器レベル測定 (RMS / Peak)
        float detectedDb = -100.0f;
        if (!isPeakDetection)
        {
            fastEnvelopeRms += rmsEnvCoeff * ((filteredDetector * filteredDetector) - fastEnvelopeRms);
            const float linLevel = std::sqrt (std::max (0.0f, fastEnvelopeRms));
            detectedDb = juce::Decibels::gainToDecibels (linLevel + 1e-6f, -100.0f);
        }
        else
        {
            const float absIn = std::abs (filteredDetector);
            fastEnvelopePeak += peakEnvCoeff * (absIn - fastEnvelopePeak);
            detectedDb = juce::Decibels::gainToDecibels (fastEnvelopePeak + 1e-6f, -100.0f);
        }

        // 6. TameNoise 連動レベラーゲイン計算
        float targetGainDb = targetLevelDb - detectedDb;
        targetGainDb = juce::jlimit (-rangeDb, +rangeDb, targetGainDb);

        // デジタルゼロ時の過大ゲイン上昇防止
        constexpr float digitalZeroFloorDb = -72.0f;
        const float activityWeight = juce::jlimit (0.0f, 1.0f, (detectedDb - digitalZeroFloorDb) / 12.0f);
        targetGainDb *= activityWeight;

        // TameNoise によるノイズ保護 ＆ リダクション (Amount 30% デフォルト)
        const float currSib   = tameNoiseSibilanceScore.load (std::memory_order_relaxed);
        const float currBr    = tameNoiseBreathScore.load (std::memory_order_relaxed);
        const float currNoise = std::max (currSib, currBr);

        if (isTameNoiseOn && currNoise > 0.20f)
        {
            // ノイズ発生時: レベラーがノイズを持ち上げるのを抑制
            if (targetGainDb > 0.0f)
                targetGainDb *= (1.0f - currNoise);

            // Tame Amount に応じた追加の音楽的ノイズリダクション (最大 -12dB)
            const float tameReductionDb = -12.0f * tameAmountNorm * currNoise;
            targetGainDb += tameReductionDb;
        }

        // アタック / リリース弾道計算
        if (targetGainDb < currentSmoothedGainDb)
            currentSmoothedGainDb += attackCoeff * (targetGainDb - currentSmoothedGainDb);
        else
            currentSmoothedGainDb += releaseCoeff * (targetGainDb - currentSmoothedGainDb);

        // 7. 出力ゲインの適用 (Lookahead 原音スルーバッファから取得)
        const float outG = juce::Decibels::decibelsToGain (smoothedOutputGainDb.getNextValue());
        const float currentGainLin = juce::Decibels::decibelsToGain (currentSmoothedGainDb) * outG;

        float delayedL = sampleInL;
        float delayedR = sampleInR;
        if (isLookaheadOn)
        {
            const int readPos = (delayBufferWritePos - lookaheadSamples + delayBufferSize) % delayBufferSize;
            delayedL = delayBuffer.getSample (0, readPos);
            if (numChannels > 1)
                delayedR = delayBuffer.getSample (1, readPos);
        }

        delayBufferWritePos = (delayBufferWritePos + 1) % delayBufferSize;

        float finalL = delayedL * currentGainLin;
        float finalR = delayedR * currentGainLin;

        // 8. TameNoise ソロ試聴 (LISTEN ボタン)
        if (isTameListenOn)
        {
            const bool active = tameNoiseTrigger.load (std::memory_order_relaxed);
            tameAuditionGain.setTargetValue (active ? 1.0f : 0.0f);
            const float soloG = tameAuditionGain.getNextValue();
            finalL *= soloG;
            finalR *= soloG;
        }

        outL[i] = finalL;
        if (numChannels > 1)
            outR[i] = finalR;

        // ピーク値の集約
        blockMaxInputPeak  = std::max ({ blockMaxInputPeak,  std::abs (sampleInL), std::abs (sampleInR) });
        blockMaxOutputPeak = std::max ({ blockMaxOutputPeak, std::abs (finalL),    std::abs (finalR) });

        // 9. GUI 波形描画用ダウンサンプリング
        if (isGuiEnabled)
        {
            inputAccumSumSquares  += 0.5f * (sampleInL * sampleInL + sampleInR * sampleInR);
            outputAccumSumSquares += 0.5f * (finalL * finalL + finalR * finalR);
            gainChangeAccumDb     += currentSmoothedGainDb;
            downsampleCounter++;

            if (downsampleCounter >= downsampleInterval)
            {
                VisualDataPoint pt;
                pt.inputRms      = std::sqrt (inputAccumSumSquares / static_cast<float>(downsampleCounter));
                pt.outputRms     = std::sqrt (outputAccumSumSquares / static_cast<float>(downsampleCounter));
                pt.gainChangeDb  = gainChangeAccumDb / static_cast<float>(downsampleCounter);

                inputAccumSumSquares  = 0.0f;
                outputAccumSumSquares = 0.0f;
                gainChangeAccumDb     = 0.0f;
                downsampleCounter     = 0;

                int start1, size1, start2, size2;
                visualFifo.prepareToWrite (1, start1, size1, start2, size2);
                if (size1 > 0) visualFifoBuffer[static_cast<size_t>(start1)] = pt;
                if (size2 > 0) visualFifoBuffer[static_cast<size_t>(start2)] = pt;
                visualFifo.finishedWrite (size1 + size2);
            }
        }
    }

    // メーター用アトミック更新
    const float blockRmsIn  = buffer.getRMSLevel (0, 0, numSamples);
    const float blockRmsOut = (numChannels > 1) ? 0.5f * (buffer.getRMSLevel(0, 0, numSamples) + buffer.getRMSLevel(1, 0, numSamples))
                                                : buffer.getRMSLevel(0, 0, numSamples);

    latestInputPeak.store  (blockMaxInputPeak,  std::memory_order_relaxed);
    latestOutputPeak.store (blockMaxOutputPeak, std::memory_order_relaxed);

    const float meterInCoeff  = 1.0f - std::exp (-static_cast<float>(numSamples) / (static_cast<float>(currentSampleRate) * 0.15f));
    const float meterOutCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / (static_cast<float>(currentSampleRate) * 0.15f));
    smoothedRmsIn  += meterInCoeff  * (blockRmsIn  - smoothedRmsIn);
    smoothedRmsOut += meterOutCoeff * (blockRmsOut - smoothedRmsOut);
    latestInputRms.store  (smoothedRmsIn,  std::memory_order_relaxed);
    latestOutputRms.store (smoothedRmsOut, std::memory_order_relaxed);

    const float vuCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / (static_cast<float>(currentSampleRate) * 0.30f));
    smoothedVuIn  += vuCoeff * (blockRmsIn  - smoothedVuIn);
    smoothedVuOut += vuCoeff * (blockRmsOut - smoothedVuOut);
    latestInputVu.store  (smoothedVuIn,  std::memory_order_relaxed);
    latestOutputVu.store (smoothedVuOut, std::memory_order_relaxed);
}

int AutoLevelerAudioProcessor::readVisualData (VisualDataPoint* destination, int maxPointsToRead)
{
    if (destination == nullptr || maxPointsToRead <= 0)
        return 0;

    int start1, size1, start2, size2;
    visualFifo.prepareToRead (maxPointsToRead, start1, size1, start2, size2);
    const int totalRead = size1 + size2;

    for (int i = 0; i < size1; ++i)
        destination[i] = visualFifoBuffer[static_cast<size_t>(start1 + i)];

    for (int i = 0; i < size2; ++i)
        destination[size1 + i] = visualFifoBuffer[static_cast<size_t>(start2 + i)];

    visualFifo.finishedRead (totalRead);
    return totalRead;
}

juce::AudioProcessorEditor* AutoLevelerAudioProcessor::createEditor()
{
    return new AutoLevelerAudioProcessorEditor (*this);
}

bool AutoLevelerAudioProcessor::hasEditor() const { return true; }

void AutoLevelerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AutoLevelerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoLevelerAudioProcessor();
}
