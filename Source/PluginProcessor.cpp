#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    breathFilterParam    = apvts.getRawParameterValue (ParameterIDs::breathFilter);
    sibilanceFilterParam = apvts.getRawParameterValue (ParameterIDs::sibilanceFilter);
    detectionModeParam   = apvts.getRawParameterValue (ParameterIDs::detectionMode);
    timingModeParam      = apvts.getRawParameterValue (ParameterIDs::timingMode);
    syncSpeedParam       = apvts.getRawParameterValue (ParameterIDs::syncSpeed);
    meterModeParam       = apvts.getRawParameterValue (ParameterIDs::meterMode);
    guiEnableParam       = apvts.getRawParameterValue (ParameterIDs::guiEnable);
    bypassParam          = apvts.getRawParameterValue (ParameterIDs::bypass);

    // 8つの実践的ファクトリープリセットの初期化
    // 1. Default (標準)
    presets.push_back ({ "Default", 0.0f, -12.0f, 6.0f, 50.0f, 0.0f, true, false, false, 0, 0, 1 });
    // 2. Synth Vocal (合成音声・打ち込みボーカル特化)
    presets.push_back ({ "Synth Vocal", 0.0f, -14.0f, 6.0f, 75.0f, 0.0f, true, true, true, 0, 0, 1 });
    // 3. Gentle Vocal Ride (自然な音量均一化)
    presets.push_back ({ "Gentle Vocal Ride", 0.0f, -14.0f, 4.0f, 35.0f, 0.0f, true, true, false, 0, 0, 1 });
    // 4. Aggressive Leveler (ロック・激しいボーカル向け)
    presets.push_back ({ "Aggressive Leveler", 0.0f, -10.0f, 10.0f, 75.0f, 0.0f, true, true, true, 0, 0, 1 });
    // 5. Fast Peak Tamer (ピーク抑制)
    presets.push_back ({ "Fast Peak Tamer", 0.0f, -8.0f, 6.0f, 90.0f, 0.0f, true, false, true, 1, 0, 1 });
    // 6. BPM Sync 1/4 (Mid) (テンポ同期ノーマル)
    presets.push_back ({ "BPM Sync 1/4 (Mid)", 0.0f, -12.0f, 6.0f, 50.0f, 0.0f, true, false, false, 0, 1, 1 });
    // 7. Podcast / Voiceover (配信・ナレーション)
    presets.push_back ({ "Podcast / Voiceover", 0.0f, -16.0f, 8.0f, 45.0f, 0.0f, true, true, true, 0, 0, 1 });
    // 8. Ballad Vocal (バラード向けスロー)
    presets.push_back ({ "Ballad Vocal", 0.0f, -15.0f, 5.0f, 25.0f, 0.0f, true, true, false, 0, 0, 1 });
    jassert (presets.size() == numFactoryPresets);

    // ユーザー保存プリセットの読み込み
    loadUserPresets();
}

AutoLevelerAudioProcessor::~AutoLevelerAudioProcessor()
{
}

const juce::String AutoLevelerAudioProcessor::getName() const
{
    return "Atafuta09Leveler";
}

bool AutoLevelerAudioProcessor::acceptsMidi() const
{
    return false;
}

bool AutoLevelerAudioProcessor::producesMidi() const
{
    return false;
}

bool AutoLevelerAudioProcessor::isMidiEffect() const
{
    return false;
}

double AutoLevelerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

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
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;

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
    if (auto* param = apvts.getParameter (ParameterIDs::breathFilter))
        param->setValueNotifyingHost (p.breathFilter ? 1.0f : 0.0f);
    if (auto* param = apvts.getParameter (ParameterIDs::sibilanceFilter))
        param->setValueNotifyingHost (p.sibilanceFilter ? 1.0f : 0.0f);
    if (auto* param = apvts.getParameter (ParameterIDs::detectionMode))
        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float>(p.detectionMode)));
    if (auto* param = apvts.getParameter (ParameterIDs::timingMode))
        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float>(p.timingMode)));
    if (auto* param = apvts.getParameter (ParameterIDs::syncSpeed))
        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float>(p.syncSpeed)));
}

const juce::String AutoLevelerAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < static_cast<int>(presets.size()))
        return presets[static_cast<size_t>(index)].name;
    return {};
}

void AutoLevelerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    // ファクトリープリセットは保存対象外のため改名させない (再起動で元に戻ってしまうため)
    if (index >= static_cast<int>(numFactoryPresets) && index < static_cast<int>(presets.size()))
    {
        presets[static_cast<size_t>(index)].name = newName;
        saveUserPresetsToFile();
    }
}

static juce::File getUserPresetFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Atafuta09")
                   .getChildFile ("Atafuta09Leveler");
    if (!dir.exists())
        dir.createDirectory();
    return dir.getChildFile ("user_presets.xml");
}

void AutoLevelerAudioProcessor::loadUserPresets()
{
    const auto file = getUserPresetFile();
    if (!file.existsAsFile())
        return;

    auto xml = juce::parseXML (file);
    if (xml == nullptr || !xml->hasTagName ("UserPresets"))
        return;

    for (auto* child : xml->getChildIterator())
    {
        if (child != nullptr && child->hasTagName ("Preset"))
        {
            Preset p;
            p.name            = child->getStringAttribute ("name", "User Preset");
            p.inGain          = static_cast<float>(child->getDoubleAttribute ("inGain", 0.0));
            p.targetLevel     = static_cast<float>(child->getDoubleAttribute ("target", -12.0));
            p.range           = static_cast<float>(child->getDoubleAttribute ("range", 6.0));
            p.speed           = static_cast<float>(child->getDoubleAttribute ("speed", 50.0));
            p.outGain         = static_cast<float>(child->getDoubleAttribute ("outGain", 0.0));
            p.lookahead       = child->getBoolAttribute ("lookahead", true);
            p.breathFilter    = child->getBoolAttribute ("breath", false);
            p.sibilanceFilter = child->getBoolAttribute ("sibilance", false);
            p.detectionMode   = child->getIntAttribute ("det", 0);
            p.timingMode      = child->getIntAttribute ("timing", 0);
            p.syncSpeed       = child->getIntAttribute ("syncSpeed", 1);

            presets.push_back (p);
        }
    }
}

void AutoLevelerAudioProcessor::saveUserPresetsToFile()
{
    const auto file = getUserPresetFile();
    auto rootXml = std::make_unique<juce::XmlElement> ("UserPresets");

    // ファクトリープリセットの後ろがユーザー追加プリセット
    for (size_t i = numFactoryPresets; i < presets.size(); ++i)
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
        child->setAttribute ("breath", p.breathFilter);
        child->setAttribute ("sibilance", p.sibilanceFilter);
        child->setAttribute ("det", p.detectionMode);
        child->setAttribute ("timing", p.timingMode);
        child->setAttribute ("syncSpeed", p.syncSpeed);
    }

    rootXml->writeTo (file, {});
}

bool AutoLevelerAudioProcessor::saveUserPreset (const juce::String& presetName)
{
    if (presetName.trim().isEmpty())
        return false;

    Preset newPreset;
    newPreset.name            = presetName.trim();
    newPreset.inGain          = inputGainParam != nullptr ? inputGainParam->load() : 0.0f;
    newPreset.targetLevel     = targetLevelParam != nullptr ? targetLevelParam->load() : -12.0f;
    newPreset.range           = rangeParam != nullptr ? rangeParam->load() : 6.0f;
    newPreset.speed           = speedParam != nullptr ? speedParam->load() : 50.0f;
    newPreset.outGain         = outputGainParam != nullptr ? outputGainParam->load() : 0.0f;
    newPreset.lookahead       = lookaheadEnableParam != nullptr ? (lookaheadEnableParam->load() > 0.5f) : true;
    newPreset.breathFilter    = breathFilterParam != nullptr ? (breathFilterParam->load() > 0.5f) : false;
    newPreset.sibilanceFilter = sibilanceFilterParam != nullptr ? (sibilanceFilterParam->load() > 0.5f) : false;
    newPreset.detectionMode   = detectionModeParam != nullptr ? juce::roundToInt (detectionModeParam->load()) : 0;
    newPreset.timingMode      = timingModeParam != nullptr ? juce::roundToInt (timingModeParam->load()) : 0;
    newPreset.syncSpeed       = syncSpeedParam != nullptr ? juce::roundToInt (syncSpeedParam->load()) : 1;

    presets.push_back (newPreset);
    currentProgram = static_cast<int>(presets.size()) - 1;

    saveUserPresetsToFile();
    return true;
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

void AutoLevelerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;

    // 1. Lookaheadディレイ（5ms）のサンプル数計算
    lookaheadSamples = juce::jmax (1, juce::roundToInt (currentSampleRate * (lookaheadMs * 0.001f)));
    const bool isLookaheadOn = (lookaheadEnableParam != nullptr && lookaheadEnableParam->load() > 0.5f);
    setLatencySamples (isLookaheadOn ? lookaheadSamples : 0);

    // ディレイバッファの初期化（ブロックサイズ + ルックアヘッド分を確保）
    delayBufferSize = lookaheadSamples + samplesPerBlock + 64;
    const int totalChannels = juce::jmax (1, getTotalNumInputChannels());
    delayBuffer.setSize (totalChannels, delayBufferSize);
    delayBuffer.clear();
    delayBufferWritePos = 0;

    // 2. ゲインスムージング初期化
    smoothedInputGainDb.reset (currentSampleRate, 0.02);
    smoothedInputGainDb.setCurrentAndTargetValue (inputGainParam != nullptr ? inputGainParam->load() : 0.0f);

    smoothedOutputGainDb.reset (currentSampleRate, 0.02);
    smoothedOutputGainDb.setCurrentAndTargetValue (outputGainParam != nullptr ? outputGainParam->load() : 0.0f);

    // 3. 歯擦音・ブレス用サイドチェーンバンドパスフィルターの初期化
    sidechainHPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 150.0f);
    sidechainLPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (currentSampleRate, 4000.0f);
    sidechainHPF.reset();
    sidechainLPF.reset();

    fastEnvelopeRms = 0.0f;
    fastEnvelopePeak = 0.0f;
    currentSmoothedGainDb = 0.0f;
    smoothedRmsIn = 0.0f;
    smoothedRmsOut = 0.0f;
    smoothedVuIn = 0.0f;
    smoothedVuOut = 0.0f;

    // 4. UI波形送信用ダウンサンプラーの初期化（約100Hz）
    downsampleInterval = juce::jmax (1, juce::roundToInt (currentSampleRate / 100.0));
    downsampleCounter = 0;
    inputAccumSumSquares = 0.0f;
    outputAccumSumSquares = 0.0f;
    gainChangeAccumDb = 0.0f;

    visualFifo.reset();
}

void AutoLevelerAudioProcessor::releaseResources()
{
    delayBuffer.setSize (0, 0);
}

void AutoLevelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, numSamples);

    if (numSamples == 0 || totalNumInputChannels == 0)
        return;

    // -------------------------------------------------------------
    // 1. ホストBPMの取得 (BPM Sync用)
    // -------------------------------------------------------------
    if (auto* currentPlayHead = getPlayHead())
    {
        if (auto pos = currentPlayHead->getPosition())
        {
            if (pos->getBpm().hasValue())
                currentBpm.store (static_cast<float>(*pos->getBpm()), std::memory_order_relaxed);
        }
    }

    // -------------------------------------------------------------
    // 2. パラメーター取得 (Lock-Free)
    // -------------------------------------------------------------
    const float inGainDb          = inputGainParam       != nullptr ? inputGainParam->load (std::memory_order_relaxed)       : 0.0f;
    const float targetLevelDb     = targetLevelParam     != nullptr ? targetLevelParam->load (std::memory_order_relaxed)     : -12.0f;
    const float rangeDb           = rangeParam           != nullptr ? rangeParam->load (std::memory_order_relaxed)           : 6.0f;
    const float speedVal          = speedParam           != nullptr ? speedParam->load (std::memory_order_relaxed)           : 50.0f;
    const float outGainDb         = outputGainParam      != nullptr ? outputGainParam->load (std::memory_order_relaxed)      : 0.0f;
    const bool isLookaheadOn      = lookaheadEnableParam != nullptr && (lookaheadEnableParam->load (std::memory_order_relaxed) > 0.5f);
    const bool isBreathFilterOn   = breathFilterParam    != nullptr && (breathFilterParam->load (std::memory_order_relaxed)    > 0.5f);
    const bool isSibilanceFilterOn= sibilanceFilterParam != nullptr && (sibilanceFilterParam->load (std::memory_order_relaxed) > 0.5f);
    const bool isPeakDetection    = detectionModeParam   != nullptr && (detectionModeParam->load (std::memory_order_relaxed)   > 0.5f);
    const bool isSyncMode         = timingModeParam      != nullptr && (timingModeParam->load (std::memory_order_relaxed)      > 0.5f);
    const int syncSpeedChoice     = syncSpeedParam       != nullptr ? juce::roundToInt (syncSpeedParam->load (std::memory_order_relaxed)) : 1;
    const bool isGuiEnabled       = guiEnableParam       != nullptr && (guiEnableParam->load (std::memory_order_relaxed)       > 0.5f);
    const bool isBypassed         = bypassParam          != nullptr && (bypassParam->load (std::memory_order_relaxed)          > 0.5f);

    // Lookahead切り替えに伴うDAWレイテンシー報告
    const int targetLatency = (isLookaheadOn && !isBypassed) ? lookaheadSamples : 0;
    if (getLatencySamples() != targetLatency)
        setLatencySamples (targetLatency);

    smoothedInputGainDb.setTargetValue (inGainDb);
    smoothedOutputGainDb.setTargetValue (outGainDb);

    // -------------------------------------------------------------
    // 3. アタック / リリースの計算 (Free vs BPM Sync 3段階: Fast, Mid, Slow)
    // -------------------------------------------------------------
    const auto timing = calculateTimingMs (speedVal, isSyncMode, syncSpeedChoice, currentBpm.load (std::memory_order_relaxed));
    const float attackCoeff  = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * (timing.attackMs * 0.001f)));
    const float releaseCoeff = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * (timing.releaseMs * 0.001f)));

    // エンベロープ検出時定数 (RMS平滑化用 ~15ms, Peak高速追従用 ~1.5ms)
    const float rmsEnvCoeff  = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * 0.015f));
    const float peakAttCoeff = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * 0.0015f));
    const float peakRelCoeff = 1.0f - std::exp (-1.0f / static_cast<float>(currentSampleRate * 0.040f));

    // チャンネルポインタ
    const float* inL = buffer.getReadPointer (0);
    const float* inR = (totalNumInputChannels > 1) ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = (totalNumOutputChannels > 1) ? buffer.getWritePointer (1) : outL;

    float blockPeakIn  = 0.0f;
    float blockPeakOut = 0.0f;
    float blockSumSqIn = 0.0f;
    float blockSumSqOut = 0.0f;

    // -------------------------------------------------------------
    // 4. サンプル単位のオーディオ処理ループ
    // -------------------------------------------------------------
    for (int n = 0; n < numSamples; ++n)
    {
        // A. Input Gain 適用
        const float curInGainLinear = juce::Decibels::decibelsToGain (smoothedInputGainDb.getNextValue());
        const float gainedSampleL = inL[n] * curInGainLinear;
        const float gainedSampleR = inR[n] * curInGainLinear;

        // B. Lookaheadディレイバッファへの書き込み
        delayBuffer.setSample (0, delayBufferWritePos, gainedSampleL);
        if (totalNumInputChannels > 1)
            delayBuffer.setSample (1, delayBufferWritePos, gainedSampleR);

        // C. 遅延サンプルの読み出し (Lookahead ON時は5ms前、OFF時は現在サンプル)
        const int effectiveDelay = isLookaheadOn ? lookaheadSamples : 0;
        int readPos = delayBufferWritePos - effectiveDelay;
        if (readPos < 0)
            readPos += delayBufferSize;

        const float delayedSampleL = delayBuffer.getSample (0, readPos);
        const float delayedSampleR = (totalNumInputChannels > 1) ? delayBuffer.getSample (1, readPos) : delayedSampleL;

        if (++delayBufferWritePos >= delayBufferSize)
            delayBufferWritePos = 0;

        // D. サイドチェーン入力の準備
        const float monoDetectorInput = 0.5f * (gainedSampleL + gainedSampleR);
        float filteredDetector = monoDetectorInput;

        // ブレス除去 (HPF 150Hz) ＆ 歯擦音除去 (LPF 4000Hz)
        if (isBreathFilterOn)
            filteredDetector = sidechainHPF.processSample (filteredDetector);

        if (isSibilanceFilterOn)
            filteredDetector = sidechainLPF.processSample (filteredDetector);

        // E. 駆動方式 (RMS vs Peak) の判定とレベル検出
        float detectedDb = -100.0f;
        if (!isPeakDetection)
        {
            // RMSモード (デフォルト)
            fastEnvelopeRms += rmsEnvCoeff * ((filteredDetector * filteredDetector) - fastEnvelopeRms);
            const float linLevel = std::sqrt (std::max (0.0f, fastEnvelopeRms));
            detectedDb = juce::Decibels::gainToDecibels (linLevel + 1e-6f, -100.0f);
        }
        else
        {
            // Peakモード
            const float detAbs = std::abs (filteredDetector);
            fastEnvelopePeak += (detAbs > fastEnvelopePeak ? peakAttCoeff : peakRelCoeff) * (detAbs - fastEnvelopePeak);
            detectedDb = juce::Decibels::gainToDecibels (fastEnvelopePeak + 1e-6f, -100.0f);
        }

        // F. ブレス音検知・安全ゲート
        float activityWeight = 1.0f;
        if (isBreathFilterOn)
        {
            // ブレス検知ON: 低フォルマント息継ぎ区間（-48dB以下）を滑らかにスルー
            constexpr float breathFloorDb = -48.0f;
            constexpr float breathCeilDb  = -36.0f;
            activityWeight = juce::jlimit (0.0f, 1.0f, (detectedDb - breathFloorDb) / (breathCeilDb - breathFloorDb));
        }
        else
        {
            // ブレス検知OFF: 完全無音時のみ保護
            constexpr float digitalZeroFloorDb = -72.0f;
            activityWeight = juce::jlimit (0.0f, 1.0f, (detectedDb - digitalZeroFloorDb) / 6.0f);
        }

        // G. 目的ゲインの算出と Range パラメーターによるクランプ ([-Range, +Range])
        const float targetDifferenceDb = (targetLevelDb - detectedDb) * activityWeight;
        const float clampedTargetCorrectionDb = juce::jlimit (-rangeDb, rangeDb, targetDifferenceDb);

        // H. 動的バリスティクス平滑化 (dB空間)
        if (clampedTargetCorrectionDb < currentSmoothedGainDb)
            currentSmoothedGainDb += attackCoeff * (clampedTargetCorrectionDb - currentSmoothedGainDb);
        else
            currentSmoothedGainDb += releaseCoeff * (clampedTargetCorrectionDb - currentSmoothedGainDb);

        // I. 総合ゲイン乗算と Output Gain の適用
        const float curOutGainLinear = juce::Decibels::decibelsToGain (smoothedOutputGainDb.getNextValue());
        const float levelingGainLinear = juce::Decibels::decibelsToGain (currentSmoothedGainDb);
        const float totalGain = levelingGainLinear * curOutGainLinear;

        const float finalOutL = isBypassed ? inL[n] : (delayedSampleL * totalGain);
        const float finalOutR = isBypassed ? inR[n] : (delayedSampleR * totalGain);

        outL[n] = juce::jlimit (-4.0f, 4.0f, finalOutL);
        if (totalNumOutputChannels > 1)
            outR[n] = juce::jlimit (-4.0f, 4.0f, finalOutR);

        // ピークトラッキング (左右最大値) & 2乗和集計 (RMS / VU用)
        const float peakSampleIn  = std::max (std::abs (gainedSampleL), std::abs (gainedSampleR));
        const float peakSampleOut = std::max (std::abs (outL[n]), totalNumOutputChannels > 1 ? std::abs (outR[n]) : std::abs (outL[n]));
        blockPeakIn  = std::max (blockPeakIn, peakSampleIn);
        blockPeakOut = std::max (blockPeakOut, peakSampleOut);

        blockSumSqIn  += 0.5f * (gainedSampleL * gainedSampleL + gainedSampleR * gainedSampleR);
        blockSumSqOut += 0.5f * (outL[n] * outL[n] + (totalNumOutputChannels > 1 ? outR[n] * outR[n] : outL[n] * outL[n]));

        // J. UI波形描画用データの集計とFIFO送信
        if (isGuiEnabled)
        {
            const float monoInSample  = 0.5f * (gainedSampleL + gainedSampleR);
            const float monoOutSample = 0.5f * (outL[n] + (totalNumOutputChannels > 1 ? outR[n] : outL[n]));

            inputAccumSumSquares  += monoInSample * monoInSample;
            outputAccumSumSquares += monoOutSample * monoOutSample;
            gainChangeAccumDb     += (isBypassed ? 0.0f : currentSmoothedGainDb);

            if (++downsampleCounter >= downsampleInterval)
            {
                const float countF = static_cast<float>(downsampleCounter);
                VisualDataPoint point;
                point.inputRms     = std::sqrt (inputAccumSumSquares / countF);
                point.outputRms    = std::sqrt (outputAccumSumSquares / countF);
                point.gainChangeDb = gainChangeAccumDb / countF;

                inputAccumSumSquares  = 0.0f;
                outputAccumSumSquares = 0.0f;
                gainChangeAccumDb     = 0.0f;
                downsampleCounter     = 0;

                int start1, size1, start2, size2;
                visualFifo.prepareToWrite (1, start1, size1, start2, size2);
                if (size1 > 0)
                {
                    visualFifoBuffer[static_cast<size_t>(start1)] = point;
                    visualFifo.finishedWrite (1);
                }
            }
        }
    }

    // ブロック全体のRMS & VUバリスティクス算出
    const float blockRmsIn  = std::sqrt (std::max (0.0f, blockSumSqIn  / static_cast<float>(numSamples)));
    const float blockRmsOut = std::sqrt (std::max (0.0f, blockSumSqOut / static_cast<float>(numSamples)));

    // RMSメーター平滑化 (時定数約 50ms)
    const float rmsMeterCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / static_cast<float>(currentSampleRate * 0.050f));
    smoothedRmsIn  += rmsMeterCoeff * (blockRmsIn  - smoothedRmsIn);
    smoothedRmsOut += rmsMeterCoeff * (blockRmsOut - smoothedRmsOut);

    // VUメーター平滑化 (時定数約 160ms = 300msで99%到達の標準VUバリスティクス)
    const float vuMeterCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / static_cast<float>(currentSampleRate * 0.160f));
    smoothedVuIn  += vuMeterCoeff * (blockRmsIn  - smoothedVuIn);
    smoothedVuOut += vuMeterCoeff * (blockRmsOut - smoothedVuOut);

    // 各メーター用アトミック値の更新
    latestInputPeak.store  (blockPeakIn,     std::memory_order_relaxed);
    latestOutputPeak.store (blockPeakOut,    std::memory_order_relaxed);
    latestInputRms.store   (smoothedRmsIn,   std::memory_order_relaxed);
    latestOutputRms.store  (smoothedRmsOut,  std::memory_order_relaxed);
    latestInputVu.store    (smoothedVuIn,    std::memory_order_relaxed);
    latestOutputVu.store   (smoothedVuOut,   std::memory_order_relaxed);
}

int AutoLevelerAudioProcessor::readVisualData (VisualDataPoint* destination, int maxPointsToRead)
{
    if (destination == nullptr || maxPointsToRead <= 0)
        return 0;

    const int numReady = visualFifo.getNumReady();
    const int numToRead = std::min (numReady, maxPointsToRead);
    if (numToRead <= 0)
        return 0;

    int start1, size1, start2, size2;
    visualFifo.prepareToRead (numToRead, start1, size1, start2, size2);

    if (size1 > 0)
        std::copy_n (&visualFifoBuffer[static_cast<size_t>(start1)], size1, destination);

    if (size2 > 0)
        std::copy_n (&visualFifoBuffer[static_cast<size_t>(start2)], size2, destination + size1);

    visualFifo.finishedRead (numToRead);
    return numToRead;
}

bool AutoLevelerAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AutoLevelerAudioProcessor::createEditor()
{
    return new AutoLevelerAudioProcessorEditor (*this);
}

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
    {
        auto state = juce::ValueTree::fromXml (*xmlState);
        apvts.replaceState (state);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AutoLevelerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Input Gain (-18 dB ~ +18 dB, デフォルト 0 dB)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::inputGain, 1 },
        "Input Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    // 2. Target Level (-36 dB ~ 0 dB, デフォルト -12 dB)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::targetLevel, 1 },
        "Target Level",
        juce::NormalisableRange<float> (-36.0f, 0.0f, 0.1f),
        -12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    // 3. Reduction Range (0 dB ~ 13 dB, 0.1 dB刻み, デフォルト 6 dB)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::range, 1 },
        "Range",
        juce::NormalisableRange<float> (0.0f, 13.0f, 0.1f),
        6.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    // 4. Speed (0% ~ 100%, デフォルト 50%)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::speed, 1 },
        "Speed",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")
    ));

    // 5. Output Gain (-18 dB ~ +18 dB, デフォルト 0 dB)
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ParameterIDs::outputGain, 1 },
        "Output Gain",
        juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")
    ));

    // 6. Lookahead Enable (トグル, デフォルト ON)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::lookaheadEnable, 1 },
        "Lookahead",
        true
    ));

    // 7. Breath Filter (トグル, デフォルト OFF)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::breathFilter, 1 },
        "Breath Filter",
        false
    ));

    // 8. Sibilance Filter (トグル, デフォルト OFF)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::sibilanceFilter, 1 },
        "Sibilance Filter",
        false
    ));

    // 9. Detection Mode (RMS: 0 [デフォルト], Peak: 1)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::detectionMode, 1 },
        "Detection Mode",
        juce::StringArray { "RMS", "Peak" },
        0
    ));

    // 10. Timing Mode (Free: 0 [デフォルト], Sync: 1)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::timingMode, 1 },
        "Timing Mode",
        juce::StringArray { "Free (ms)", "Sync (BPM)" },
        0
    ));

    // 11. Sync Speed Mode (Fast: 0, Mid: 1 [デフォルト], Slow: 2)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::syncSpeed, 1 },
        "Sync Speed",
        juce::StringArray { "Fast", "Mid", "Slow" },
        1
    ));

    // 12. Meter Mode (Peak: 0 [デフォルト], RMS: 1, VU-18: 2)
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { ParameterIDs::meterMode, 1 },
        "Meter Mode",
        juce::StringArray { "Peak", "RMS", "VU-18" },
        0
    ));

    // 13. GUI Enable (描画ON/OFF トグル, デフォルト ON)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::guiEnable, 1 },
        "GUI Enable",
        true
    ));

    // 14. Bypass (バイパス トグル, デフォルト OFF)
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParameterIDs::bypass, 1 },
        "Bypass",
        false
    ));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoLevelerAudioProcessor();
}
