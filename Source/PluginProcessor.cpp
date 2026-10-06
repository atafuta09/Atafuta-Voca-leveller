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
    guiEnableParam       = apvts.getRawParameterValue (ParameterIDs::guiEnable);
    bypassParam          = apvts.getRawParameterValue (ParameterIDs::bypass);

    tameNoiseEnableParam = apvts.getRawParameterValue (ParameterIDs::tameNoiseEnable);
    tameNoiseListenParam = apvts.getRawParameterValue (ParameterIDs::tameNoiseListen);
    tameNoiseAmountParam = apvts.getRawParameterValue (ParameterIDs::tameNoiseAmount);
    scFilterEnableParam  = apvts.getRawParameterValue (ParameterIDs::scFilterEnable);

    for (auto* param : { inputGainParam, targetLevelParam, rangeParam, speedParam, outputGainParam,
                         lookaheadEnableParam, detectionModeParam,
                         guiEnableParam, bypassParam, tameNoiseEnableParam, tameNoiseListenParam,
                         tameNoiseAmountParam, scFilterEnableParam })
        jassertquiet (param != nullptr);

    // 起動時のデフォルトプリセットを登録
    presets.clear();
    presets.push_back ({ "Default", 0.0f, -24.0f, 7.0f, 93.6f, 0.0f, true, true, 24.0f, true, 0 });
    currentProgram = 0;

    // ユーザー保存プリセットをロード (Default の後ろに追加)
    loadUserPresets();

    // AI TameNoise 検出器の初期化 ＆ ユーザー学習プロファイルの自動ロード
    const auto learnedDir = AtafutaAudio::VocaNoiseLearnner::getDefaultLearnedDataDir();
    tameNoiseEngine.loadUserProfilesFromDirectory (learnedDir);
}

const juce::String AutoLevelerAudioProcessor::getName() const
{
    return "Atafuta09Leveler";
}

int AutoLevelerAudioProcessor::getNumPrograms()
{
    return static_cast<int>(presets.size());
}

int AutoLevelerAudioProcessor::getCurrentProgram()
{
    if (presets.empty())
        return 0;
    return juce::jlimit (0, static_cast<int>(presets.size()) - 1, currentProgram);
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
                p.targetLevel   = static_cast<float>(child->getDoubleAttribute ("target", -24.0));
                p.range         = static_cast<float>(child->getDoubleAttribute ("range", 7.0));
                p.speed         = static_cast<float>(child->getDoubleAttribute ("speed", 93.6));
                p.outGain       = static_cast<float>(child->getDoubleAttribute ("outGain", 0.0));
                p.lookahead     = child->getBoolAttribute ("lookahead", true);
                p.tameNoise     = child->getBoolAttribute ("tameNoise", true);
                p.tameAmount    = static_cast<float>(child->getDoubleAttribute ("tameAmount", 24.0));
                p.scFilter      = child->getBoolAttribute ("scFilter", true);
                p.detectionMode = child->getIntAttribute ("det", 0);

                if (p.name == "Default")
                    continue;

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
    // インデックス0は固定の Default プリセットなので、ユーザー保存分 (i = 1 以降) のみを保存
    for (size_t i = 1; i < presets.size(); ++i)
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
    }

    rootXml->writeTo (file, {});
}

bool AutoLevelerAudioProcessor::saveUserPreset (const juce::String& presetName)
{
    if (presetName.trim().isEmpty()) return false;

    Preset newPreset;
    newPreset.name          = presetName.trim();
    newPreset.inGain        = inputGainParam != nullptr ? inputGainParam->load() : 0.0f;
    newPreset.targetLevel   = targetLevelParam != nullptr ? targetLevelParam->load() : -24.0f;
    newPreset.range         = rangeParam != nullptr ? rangeParam->load() : 7.0f;
    newPreset.speed         = speedParam != nullptr ? speedParam->load() : 93.6f;
    newPreset.outGain       = outputGainParam != nullptr ? outputGainParam->load() : 0.0f;
    newPreset.lookahead     = lookaheadEnableParam != nullptr ? (lookaheadEnableParam->load() > 0.5f) : true;
    newPreset.tameNoise     = tameNoiseEnableParam != nullptr ? (tameNoiseEnableParam->load() > 0.5f) : true;
    newPreset.tameAmount    = tameNoiseAmountParam != nullptr ? tameNoiseAmountParam->load() : 24.0f;
    newPreset.scFilter      = scFilterEnableParam  != nullptr ? (scFilterEnableParam->load() > 0.5f) : true;
    newPreset.detectionMode = detectionModeParam != nullptr ? juce::roundToInt (detectionModeParam->load()) : 0;

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

// ==============================================================================
// 処理準備 (prepareToPlay)
// ==============================================================================
void AutoLevelerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = (sampleRate > 0.0) ? sampleRate : 44100.0;

    // 1. Lookaheadディレイ（22.5ms）のサンプル数計算
    lookaheadSamples = juce::jmax (1, juce::roundToInt (currentSampleRate * (lookaheadMs * 0.001f)));
    const bool isLookaheadOn = lookaheadEnableParam != nullptr && (lookaheadEnableParam->load() > 0.5f);
    setLatencySamples (isLookaheadOn ? lookaheadSamples : 0);

    // Lookaheadディレイバッファの初期化
    delayBufferSize = lookaheadSamples + samplesPerBlock + 1024;
    delayBuffer.setSize (getTotalNumInputChannels(), delayBufferSize);
    delayBuffer.clear();
    delayBufferWritePos = 0;

    // 2. ゲインスムージング初期化
    smoothedInputGainDb.reset (currentSampleRate, 0.02);
    smoothedInputGainDb.setCurrentAndTargetValue (inputGainParam != nullptr ? inputGainParam->load() : 0.0f);

    smoothedOutputGainDb.reset (currentSampleRate, 0.02);
    smoothedOutputGainDb.setCurrentAndTargetValue (outputGainParam != nullptr ? outputGainParam->load() : 0.0f);

    lookaheadMix.reset (currentSampleRate, 0.02);
    lookaheadMix.setCurrentAndTargetValue (isLookaheadOn ? 1.0f : 0.0f);

    bypassMix.reset (currentSampleRate, 0.02);
    bypassMix.setCurrentAndTargetValue ((bypassParam != nullptr && bypassParam->load() > 0.5f) ? 1.0f : 0.0f);

    // 3. 音質変化ゼロのサイドチェイン検出専用 HPF (100Hz) の初期化
    sidechainDetectorHPF.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (currentSampleRate, 100.0f);
    sidechainDetectorHPF.reset();

    // 4. AI TameNoise エンジンのリサンプラー・リングバッファ初期化
    tameResampleRatio = TAME_INTERNAL_SR / currentSampleRate;
    tameResamplePhase = 0.0;
    tameLastDawSample = 0.0f;
    tameSamplesSinceLastAnalysis = 0;
    tameRingBuffer16k.assign (4096, 0.0f);
    tameRingWritePos = 0;

    tameAuditionGain.reset (currentSampleRate, 0.01);
    tameAuditionGain.setCurrentAndTargetValue (0.0f);

    fastEnvelopeRms = 0.0f;
    fastEnvelopePeak = 0.0f;
    currentSmoothedGainDb = 0.0f;
    smoothedRmsIn = 0.0f;
    smoothedRmsOut = 0.0f;
    smoothedVuIn = 0.0f;
    smoothedVuOut = 0.0f;

    // 5. UI波形送信用ダウンサンプラーの初期化（約100Hz）
    downsampleInterval = juce::jmax (1, juce::roundToInt (currentSampleRate / 100.0));
    downsampleCounter = 0;
    inputAccumSumSquares = 0.0f;
    outputAccumSumSquares = 0.0f;
    gainChangeAccumDb = 0.0f;

    visualFifo.reset();
}

// ==============================================================================
// オーディオ処理 (processBlock)
// ==============================================================================
void AutoLevelerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // パラメータ値の取得
    constexpr auto relaxed = std::memory_order_relaxed;
    const float inGainDb          = inputGainParam->load (relaxed);
    const float targetLevelDb     = targetLevelParam->load (relaxed);
    const float rangeDb           = rangeParam->load (relaxed);
    const float speedVal          = speedParam->load (relaxed);
    const float outGainDb         = outputGainParam->load (relaxed);
    const bool isLookaheadOn      = lookaheadEnableParam->load (relaxed) > 0.5f;
    const bool isPeakDetection    = detectionModeParam->load (relaxed)   > 0.5f;
    const bool isGuiEnabled       = guiEnableParam->load (relaxed)       > 0.5f;
    const bool isBypassed         = bypassParam->load (relaxed)          > 0.5f;

    const bool isTameNoiseOn      = tameNoiseEnableParam->load (relaxed) > 0.5f;
    const bool isTameListenOn     = tameNoiseListenParam->load (relaxed) > 0.5f;
    const float tameAmountNorm    = tameNoiseAmountParam->load (relaxed) * 0.01f;
    const bool isScFilterOn       = scFilterEnableParam->load (relaxed)  > 0.5f;

    // DAWレイテンシー報告
    const int targetLatency = (isLookaheadOn && !isBypassed) ? lookaheadSamples : 0;
    if (getLatencySamples() != targetLatency)
        setLatencySamples (targetLatency);

    if (isBypassed)
    {
        latestInputPeak.store  (0.0f, relaxed);
        latestOutputPeak.store (0.0f, relaxed);
        latestInputRms.store   (0.0f, relaxed);
        latestOutputRms.store  (0.0f, relaxed);
        latestInputVu.store    (0.0f, relaxed);
        latestOutputVu.store   (0.0f, relaxed);
        return;
    }

    smoothedInputGainDb.setTargetValue (inGainDb);
    smoothedOutputGainDb.setTargetValue (outGainDb);
    bypassMix.setTargetValue (isBypassed ? 1.0f : 0.0f);
    lookaheadMix.setTargetValue (isLookaheadOn ? 1.0f : 0.0f);

    // 時定数計算 (Free モード、ヒープ確保なし)
    const auto timing = calculateTimingMs (speedVal);
    const float attackCoeff  = 1.0f - std::exp (-1.0f / (static_cast<float>(currentSampleRate) * (timing.attackMs * 0.001f)));
    const float releaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float>(currentSampleRate) * (timing.releaseMs * 0.001f)));
    constexpr float tameReleaseMs = 50.0f;
    const float tameReleaseCoeff = 1.0f - std::exp (-1.0f / (static_cast<float>(currentSampleRate) * (tameReleaseMs * 0.001f)));

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

        // 2. Lookahead 遅延バッファへの書き込み (原音スルー用: 音質変化0)
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
                tameNoiseSibilanceScore.store (match.sibilanceScore, relaxed);
                tameNoiseBreathScore.store (match.breathScore, relaxed);
                tameNoiseNormalScore.store (match.normalScore, relaxed);

                // Learnnerプラグインと同一のSensitivity閾値検知 (0.15 ~ 0.65, デフォルト0.5で0.40)
                const float sensitivity = tameAmountNorm; // 0.0 ~ 1.0
                const float threshold   = 0.65f - (sensitivity * 0.50f);

                bool noiseActive = false;
                if (match.sibilanceScore >= threshold) noiseActive = true;
                if (match.breathScore    >= threshold) noiseActive = true;

                tameNoiseTrigger.store (isTameNoiseOn && noiseActive, relaxed);

                // LED インジケーター強度
                const float maxNoise = std::max (match.sibilanceScore, match.breathScore);
                const float targetLed = (isTameNoiseOn && noiseActive)
                    ? std::min (1.0f, 0.45f + ((maxNoise - threshold) / (1.0f - threshold + 1e-4f)) * 0.55f)
                    : 0.0f;

                float currentLed = tameNoiseLedIntensity.load (relaxed);
                if (targetLed > currentLed)
                    currentLed = targetLed;
                else
                    currentLed = std::max (0.0f, currentLed - 0.05f);
                tameNoiseLedIntensity.store (currentLed, relaxed);
            }
        }
        tameLastDawSample = monoDetectorInput;

        // 4. サイドチェイン検出器パス
        float detSignal = monoDetectorInput;
        if (isScFilterOn)
            detSignal = sidechainDetectorHPF.processSample (detSignal);

        // 5. エンベロープ検出 (RMS vs Peak)
        const float sampleAbs = std::abs (detSignal);
        fastEnvelopeRms  += rmsEnvCoeff  * (sampleAbs * sampleAbs - fastEnvelopeRms);
        fastEnvelopePeak += peakEnvCoeff * (sampleAbs - fastEnvelopePeak);

        const float envVal = isPeakDetection ? fastEnvelopePeak : std::sqrt (std::max (1e-12f, fastEnvelopeRms));
        const float envDb  = juce::Decibels::gainToDecibels (envVal, -100.0f);

        // 6. ゲイン補正計算
        const float errorDb = targetLevelDb - envDb;
        const float targetGainDb = juce::jlimit (-rangeDb, rangeDb, errorDb);

        // 無音・極小入力時はゲイン補正を行わず 0 dB (レンジ中央) に戻す
        // -60 dBFS 以下では完全に中央 (0 dB)、-48 dBFS 以上では通常動作、その間は滑らかにクロスフェード
        constexpr float silenceThresholdLowDb  = -60.0f;
        constexpr float silenceThresholdHighDb = -48.0f;
        float activeRatio = 1.0f;
        if (envDb <= silenceThresholdLowDb)
            activeRatio = 0.0f;
        else if (envDb < silenceThresholdHighDb)
            activeRatio = (envDb - silenceThresholdLowDb) / (silenceThresholdHighDb - silenceThresholdLowDb);

        float effectiveTargetGainDb = targetGainDb * activeRatio;

        // TameNoise 検出時のゲイン処理
        const bool isNoiseActive = isTameNoiseOn && tameNoiseTrigger.load (relaxed);
        if (isNoiseActive)
        {
            // ノイズ発生時は誤ブーストを遮断し、リダクション
            effectiveTargetGainDb = std::min (effectiveTargetGainDb, -rangeDb * tameAmountNorm);
        }

        const float coeff = isNoiseActive ? tameReleaseCoeff
                                          : (effectiveTargetGainDb < currentSmoothedGainDb ? attackCoeff : releaseCoeff);
        currentSmoothedGainDb += coeff * (effectiveTargetGainDb - currentSmoothedGainDb);

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
            const bool active = tameNoiseTrigger.load (relaxed);
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

    latestInputPeak.store  (blockMaxInputPeak,  relaxed);
    latestOutputPeak.store (blockMaxOutputPeak, relaxed);

    const float meterInCoeff  = 1.0f - std::exp (-static_cast<float>(numSamples) / (static_cast<float>(currentSampleRate) * 0.15f));
    const float meterOutCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / (static_cast<float>(currentSampleRate) * 0.15f));
    smoothedRmsIn  += meterInCoeff  * (blockRmsIn  - smoothedRmsIn);
    smoothedRmsOut += meterOutCoeff * (blockRmsOut - smoothedRmsOut);
    latestInputRms.store  (smoothedRmsIn,  relaxed);
    latestOutputRms.store (smoothedRmsOut, relaxed);

    const float vuCoeff = 1.0f - std::exp (-static_cast<float>(numSamples) / (static_cast<float>(currentSampleRate) * 0.30f));
    smoothedVuIn  += vuCoeff * (blockRmsIn  - smoothedVuIn);
    smoothedVuOut += vuCoeff * (blockRmsOut - smoothedVuOut);
    latestInputVu.store  (smoothedVuIn,  relaxed);
    latestOutputVu.store (smoothedVuOut, relaxed);
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

juce::AudioProcessorValueTreeState::ParameterLayout AutoLevelerAudioProcessor::createParameterLayout()
{
    using Float  = juce::AudioParameterFloat;
    using Bool   = juce::AudioParameterBool;
    using Choice = juce::AudioParameterChoice;
    const auto dB = juce::AudioParameterFloatAttributes().withLabel ("dB");
    const auto id = [] (const char* paramID) { return juce::ParameterID { paramID, 1 }; };

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<Float>  (id (ParameterIDs::inputGain),       "Input Gain",       juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),   0.0f, dB),
                std::make_unique<Float>  (id (ParameterIDs::targetLevel),     "Target Level",     juce::NormalisableRange<float> (-36.0f,  0.0f, 0.1f), -24.0f, dB),
                std::make_unique<Float>  (id (ParameterIDs::range),           "Range",            juce::NormalisableRange<float> (  0.0f, 15.0f, 0.1f),   7.0f, dB),
                std::make_unique<Float>  (id (ParameterIDs::speed),           "Speed",            juce::NormalisableRange<float> (  0.0f, 100.0f, 0.1f), 93.6f,
                                          juce::AudioParameterFloatAttributes().withLabel ("%")),
                std::make_unique<Float>  (id (ParameterIDs::outputGain),      "Output Gain",      juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f),   0.0f, dB),
                std::make_unique<Bool>   (id (ParameterIDs::lookaheadEnable), "Lookahead",        true),
                std::make_unique<Choice> (id (ParameterIDs::detectionMode),   "Detection Mode",   juce::StringArray { "RMS", "Peak" }, 0),
                std::make_unique<Choice> (id (ParameterIDs::meterMode),       "Meter Mode",       juce::StringArray { "Peak", "RMS", "VU" }, 0),
                std::make_unique<Bool>   (id (ParameterIDs::guiEnable),       "GUI Animation",    true),
                std::make_unique<Bool>   (id (ParameterIDs::bypass),          "Bypass",           false),
                std::make_unique<Bool>   (id (ParameterIDs::tameNoiseEnable), "TameNoise Enable", true),
                std::make_unique<Bool>   (id (ParameterIDs::tameNoiseListen), "TameNoise Listen", false),
                std::make_unique<Float>  (id (ParameterIDs::tameNoiseAmount), "TameNoise Amount", juce::NormalisableRange<float> (  0.0f, 100.0f, 1.0f), 24.0f,
                                          juce::AudioParameterFloatAttributes().withLabel ("%")),
                std::make_unique<Bool>   (id (ParameterIDs::scFilterEnable),  "SC Filter 100Hz",  true));
    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AutoLevelerAudioProcessor();
}
