// ==============================================================================
// PluginProcessor.cpp
// Atafuta09 VocaNoise Learnner: 音響プロファイル学習 & 照合 VST3 プラグイン
// ==============================================================================
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace AtafutaAudio::VocaNoiseLearnner {

Atafuta09VocaNoiseLearnnerAudioProcessor::Atafuta09VocaNoiseLearnnerAudioProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    listenParam          = apvts.getRawParameterValue("listen");
    sensitivityParam     = apvts.getRawParameterValue("sensitivity");
    detectSibilanceParam = apvts.getRawParameterValue("detectSibilance");
    detectPlosiveParam   = apvts.getRawParameterValue("detectPlosive");
    detectBreathParam    = apvts.getRawParameterValue("detectBreath");

    learnedDataDir = getDefaultLearnedDataDir();
    juce::File(learnedDataDir).createDirectory();
    juce::File(learnedDataDir).getChildFile("female").createDirectory();
    juce::File(learnedDataDir).getChildFile("male").createDirectory();

    // 起動時に保存済み全JSONプロファイルを自動ロード
    fingerprintEngine.loadUserProfilesFromDirectory(learnedDataDir);
}

Atafuta09VocaNoiseLearnnerAudioProcessor::~Atafuta09VocaNoiseLearnnerAudioProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Atafuta09VocaNoiseLearnnerAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "listen", "Noise Listen", false));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sensitivity", "Detection Sensitivity",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.50f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "detectSibilance", "Detect Sibilance", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "detectPlosive", "Detect Plosive", true));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        "detectBreath", "Detect Breath", true));

    return { params.begin(), params.end() };
}

void Atafuta09VocaNoiseLearnnerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentDawSampleRate = sampleRate;
    resampleRatio = INTERNAL_SR / currentDawSampleRate;
    resamplePhase = 0.0;
    lastDawSample = 0.0f;

    ringBuffer16k.assign(BLOCK_SIZE * 4, 0.0f);
    ringWritePos = 0;
    samplesSinceLastAnalysis = 0;

    fingerprintEngine.reset();

    auditionGain.reset(sampleRate, 0.015); // 15ms クロスフェード
    auditionGain.setCurrentAndTargetValue(0.0f);
}

void Atafuta09VocaNoiseLearnnerAudioProcessor::releaseResources()
{
}

bool Atafuta09VocaNoiseLearnnerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void Atafuta09VocaNoiseLearnnerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    const bool listenEnabled = (listenParam != nullptr && listenParam->load() > 0.5f);
    const float sensitivity  = (sensitivityParam != nullptr) ? sensitivityParam->load() : 0.5f;
    const bool detSibilance  = (detectSibilanceParam != nullptr && detectSibilanceParam->load() > 0.5f);
    const bool detPlosive    = (detectPlosiveParam != nullptr && detectPlosiveParam->load() > 0.5f);
    const bool detBreath     = (detectBreathParam != nullptr && detectBreathParam->load() > 0.5f);

    // 感度に応じた閾値計算 (0.20 ~ 0.85)
    const float threshold = 0.65f - (sensitivity * 0.50f);

    const float* inL = buffer.getReadPointer(0);
    const float* inR = (numChannels > 1) ? buffer.getReadPointer(1) : inL;

    const int ringSize = static_cast<int>(ringBuffer16k.size());

    for (int i = 0; i < numSamples; ++i)
    {
        const float monoIn = 0.5f * (inL[i] + inR[i]);

        resamplePhase += resampleRatio;
        while (resamplePhase >= 1.0)
        {
            resamplePhase -= 1.0;
            const float interp = lastDawSample + static_cast<float>(resamplePhase) * (monoIn - lastDawSample);
            ringBuffer16k[ringWritePos] = interp;
            ringWritePos = (ringWritePos + 1) % ringSize;
            samplesSinceLastAnalysis++;

            if (samplesSinceLastAnalysis >= HOP_SIZE)
            {
                samplesSinceLastAnalysis = 0;

                std::array<float, BLOCK_SIZE> blockBuffer;
                int readPos = (ringWritePos - BLOCK_SIZE + ringSize) % ringSize;
                for (int b = 0; b < BLOCK_SIZE; ++b)
                {
                    blockBuffer[b] = ringBuffer16k[readPos];
                    readPos = (readPos + 1) % ringSize;
                }

                auto match = fingerprintEngine.analyzeBlock(blockBuffer.data(), BLOCK_SIZE);

                // UI メーター値更新
                meterNormal.store(match.normalScore, std::memory_order_relaxed);
                meterSibilance.store(match.sibilanceScore, std::memory_order_relaxed);
                meterPlosive.store(match.plosiveScore, std::memory_order_relaxed);
                meterBreath.store(match.breathScore, std::memory_order_relaxed);

                // ノイズ判定
                bool noiseActive = false;
                if (detSibilance && match.sibilanceScore >= threshold) noiseActive = true;
                if (detPlosive   && match.plosiveScore   >= threshold) noiseActive = true;
                if (detBreath    && match.breathScore    >= threshold) noiseActive = true;

                meterNoiseTrigger.store(noiseActive, std::memory_order_relaxed);
            }
        }
        lastDawSample = monoIn;

        // 可聴 (Listen) モードゲイン
        const bool active = meterNoiseTrigger.load(std::memory_order_relaxed);
        const float targetAudition = (listenEnabled ? (active ? 1.0f : 0.0f) : 1.0f);
        auditionGain.setTargetValue(targetAudition);

        const float g = auditionGain.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.getWritePointer(ch)[i] *= g;
        }
    }
}

juce::AudioProcessorEditor* Atafuta09VocaNoiseLearnnerAudioProcessor::createEditor()
{
    return new Atafuta09VocaNoiseLearnnerAudioProcessorEditor(*this);
}

void Atafuta09VocaNoiseLearnnerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Atafuta09VocaNoiseLearnnerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

} // namespace AtafutaAudio::VocaNoiseLearnner

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AtafutaAudio::VocaNoiseLearnner::Atafuta09VocaNoiseLearnnerAudioProcessor();
}
