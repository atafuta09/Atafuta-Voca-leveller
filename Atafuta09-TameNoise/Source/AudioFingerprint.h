// ==============================================================================
// AudioFingerprint.h
// Atafuta09 VocaNoise Learnner: 高精度音響プロファイル学習 & 照合エンジン v1.3.1
// (87 検証済み女性ボーカルデータセットから学習・標準搭載)
// ==============================================================================
#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace AtafutaAudio::VocaNoiseLearnner {

constexpr int NUM_BANDS = 32;

// 音響プロファイル構造体 (32バンド正規化スペクトル + ZCR + 高域比)
struct AcousticProfile {
    bool isCustom = false;
    float zcr = 0.0f;
    float highRatio = 0.0f;
    std::array<float, NUM_BANDS> spectrum {};

    // コサイン類似度 + 特徴ペナルティ計算 (0.0f ~ 1.0f)
    float calculateSimilarity(const std::array<float, NUM_BANDS>& inSpec, float inZcr, float inHighRatio) const
    {
        double dot = 0.0;
        double normA = 0.0;
        double normB = 0.0;

        for (int i = 0; i < NUM_BANDS; ++i)
        {
            dot += inSpec[i] * spectrum[i];
            normA += inSpec[i] * inSpec[i];
            normB += spectrum[i] * spectrum[i];
        }

        if (normA <= 1e-9 || normB <= 1e-9)
            return 0.0f;

        const float specCos = static_cast<float>(dot / (std::sqrt(normA) * std::sqrt(normB)));

        // ZCR と高域比の差分ペナルティ
        const float zcrDiff = std::abs(inZcr - zcr);
        const float zcrScore = std::max(0.0f, 1.0f - zcrDiff * 2.0f);

        const float highDiff = std::abs(inHighRatio - highRatio);
        const float highScore = std::max(0.0f, 1.0f - highDiff * 1.5f);

        // 重み付け: スペクトル形状 70% + ZCR 15% + 高域比 15%
        float score = (specCos * 0.70f) + (zcrScore * 0.15f) + (highScore * 0.15f);
        return std::clamp(score, 0.0f, 1.0f);
    }
};

// ==============================================================================
// 事前学習済み 女性ボーカル音響プロファイルバンク (87 検証済み女性ボーカルから抽出)
// NormalVocal (25), Sibilance (29), Breath (33)
// ==============================================================================

// 1. Female NormalVocal Cluster 0 (16サンプル: 豊かな母音・基音・低中フォルマント)
inline const AcousticProfile female_learned_normalvocal_0 = {
    true, 0.1293f, 0.0141f,
    {
      0.001105f, 0.001105f, 0.001105f, 0.001105f, 0.000534f, 0.000534f, 0.000534f, 0.000551f,
      0.000613f, 0.000613f, 0.000751f, 0.002407f, 0.003286f, 0.166856f, 0.265064f, 0.506327f,
      0.304511f, 0.454169f, 0.263775f, 0.298405f, 0.267708f, 0.153187f, 0.113334f, 0.227676f,
      0.120261f, 0.034632f, 0.025229f, 0.098217f, 0.042447f, 0.012252f, 0.009206f, 0.009653f }
};

// 2. Female NormalVocal Cluster 1 (9サンプル: 抜けの良い張りのある歌唱母音)
inline const AcousticProfile female_learned_normalvocal_1 = {
    true, 0.2954f, 0.0894f,
    {
      0.000317f, 0.000317f, 0.000317f, 0.000317f, 0.000120f, 0.000120f, 0.000120f, 0.000093f,
      0.000077f, 0.000077f, 0.000073f, 0.000256f, 0.003503f, 0.054391f, 0.139654f, 0.085367f,
      0.016152f, 0.015470f, 0.098213f, 0.012718f, 0.282478f, 0.026230f, 0.113744f, 0.145460f,
      0.184146f, 0.085622f, 0.292475f, 0.738685f, 0.415258f, 0.015949f, 0.005818f, 0.013670f }
};

// 3. Female Sibilance Cluster 0 (19サンプル: 自然な歯擦音 / "サ" "シ" 等)
inline const AcousticProfile female_learned_sibilance_0 = {
    true, 0.5354f, 0.7992f,
    {
      0.007507f, 0.007507f, 0.007507f, 0.007507f, 0.003864f, 0.003864f, 0.003864f, 0.003822f,
      0.004474f, 0.004474f, 0.004784f, 0.011783f, 0.012806f, 0.040110f, 0.074843f, 0.044059f,
      0.037837f, 0.061822f, 0.024083f, 0.019789f, 0.017954f, 0.020285f, 0.022634f, 0.027879f,
      0.032763f, 0.046034f, 0.074189f, 0.125492f, 0.240186f, 0.542823f, 0.548429f, 0.551710f }
};

// 4. Female Sibilance Cluster 1 (10サンプル: 鋭い高域歯擦音 / "ス" "ツ" 等)
inline const AcousticProfile female_learned_sibilance_1 = {
    true, 0.7461f, 0.9413f,
    {
      0.000762f, 0.000762f, 0.000762f, 0.000762f, 0.000542f, 0.000542f, 0.000542f, 0.000671f,
      0.000574f, 0.000574f, 0.000239f, 0.001103f, 0.000286f, 0.000788f, 0.005488f, 0.017456f,
      0.008203f, 0.002028f, 0.002097f, 0.003664f, 0.002553f, 0.003609f, 0.003619f, 0.005594f,
      0.010521f, 0.027677f, 0.020833f, 0.029530f, 0.060711f, 0.187849f, 0.436257f, 0.876373f }
};

// 5. Female Breath Cluster 0 (4サンプル: 擦れ気流・早めのブレス / Quick Inhale)
inline const AcousticProfile female_learned_breath_0 = {
    true, 0.5016f, 0.5490f,
    {
      0.000848f, 0.000848f, 0.000848f, 0.000848f, 0.000308f, 0.000308f, 0.000308f, 0.000392f,
      0.000437f, 0.000437f, 0.000567f, 0.001640f, 0.000640f, 0.001123f, 0.001217f, 0.006867f,
      0.052613f, 0.102683f, 0.065077f, 0.037427f, 0.045059f, 0.014828f, 0.060191f, 0.389890f,
      0.076631f, 0.067990f, 0.102367f, 0.478028f, 0.238312f, 0.194137f, 0.311629f, 0.618085f }
};

// 6. Female Breath Cluster 1 (8サンプル: 深部吸気音 / ゆったりした肺吸気)
inline const AcousticProfile female_learned_breath_1 = {
    true, 0.3167f, 0.1738f,
    {
      0.008412f, 0.008412f, 0.008412f, 0.008412f, 0.009937f, 0.009937f, 0.009937f, 0.012837f,
      0.010139f, 0.010139f, 0.009824f, 0.031653f, 0.016779f, 0.028925f, 0.097329f, 0.061702f,
      0.020687f, 0.020249f, 0.104807f, 0.186779f, 0.206709f, 0.198015f, 0.066082f, 0.350203f,
      0.743150f, 0.186378f, 0.075629f, 0.152639f, 0.200771f, 0.229634f, 0.109275f, 0.082996f }
};

// 7. Female Breath Cluster 2 (21サンプル: 標準的な自然なブレス / Natural Breath)
inline const AcousticProfile female_learned_breath_2 = {
    true, 0.4501f, 0.4145f,
    {
      0.003896f, 0.003896f, 0.003896f, 0.003896f, 0.001558f, 0.001558f, 0.001558f, 0.002723f,
      0.004325f, 0.004325f, 0.003065f, 0.005812f, 0.003052f, 0.005489f, 0.011588f, 0.010831f,
      0.012347f, 0.018778f, 0.036213f, 0.070011f, 0.130521f, 0.128681f, 0.118706f, 0.360443f,
      0.380797f, 0.222898f, 0.286840f, 0.475893f, 0.375982f, 0.280412f, 0.250025f, 0.171123f }
};


inline juce::String getDefaultLearnedDataDir()
{
    // 1. プロジェクト開発フォルダが存在すれば最優先
    juce::File devDir("D:/Atafuta09PluginBuild/Atafuta Vocal leveler/Atafuta09-TameNoise/learned_data");
    if (devDir.exists() && devDir.isDirectory())
        return devDir.getFullPathName();

    // 2. なければ %APPDATA%/AtafutaAudio/VocaNoise/learned_data
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("AtafutaAudio")
                       .getChildFile("VocaNoise")
                       .getChildFile("learned_data");
    if (!appData.exists())
        appData.createDirectory();
    return appData.getFullPathName();
}

class AudioFingerprintEngine {
public:
    AudioFingerprintEngine()
        : forwardFFT(9), // 512 points
          window(512, juce::dsp::WindowingFunction<float>::hann, false)
    {
        initBanks();
    }

    void reset()
    {
        learningSibilance.store(false);
        learningBreath.store(false);
        learningNormal.store(false);
        hasPending.store(false);
        accumFrames = 0;
        accumSpectrum.fill(0.0);
        accumZcr = 0.0;
        accumHighRatio = 0.0;
        smoothedBreathScore = 0.0f;
        breathHoldCounter = 0;
    }

    void startLearnSibilance()
    {
        stopAllLearning();
        learningSibilance.store(true);
    }

    void startLearnBreath()
    {
        stopAllLearning();
        learningBreath.store(true);
    }

    void startLearnNormal()
    {
        stopAllLearning();
        learningNormal.store(true);
    }

    // ユーザー確認: 男性/女性を指定して保存
    bool confirmPendingProfile(const juce::String& baseDataDir, const juce::String& gender)
    {
        if (!hasPending.load(std::memory_order_relaxed))
            return false;

        const int pClass = pendingClass.load(std::memory_order_relaxed);
        if (pClass == 1) // Sibilance
        {
            customSibilanceProfile = pendingProfile;
            hasCustomSibilance.store(true);
            saveProfileToJson("Sibilance", customSibilanceProfile, gender, baseDataDir);
        }
        else if (pClass == 2) // Breath
        {
            customBreathProfile = pendingProfile;
            hasCustomBreath.store(true);
            saveProfileToJson("Breath", customBreathProfile, gender, baseDataDir);
        }
        else if (pClass == 3) // Normal Vocal
        {
            customNormalProfile = pendingProfile;
            hasCustomNormal.store(true);
            saveProfileToJson("NormalVocal", customNormalProfile, gender, baseDataDir);
        }

        hasPending.store(false);
        return true;
    }

    // ユーザー確認: 破棄
    void discardPendingProfile()
    {
        hasPending.store(false);
    }

    void resetToDefaults()
    {
        initBanks();
        hasCustomSibilance.store(false);
        hasCustomBreath.store(false);
        hasCustomNormal.store(false);
        hasPending.store(false);
        smoothedBreathScore = 0.0f;
        breathHoldCounter = 0;
    }

    bool isLearning() const noexcept
    {
        return learningSibilance.load(std::memory_order_relaxed) ||
               learningBreath.load(std::memory_order_relaxed) ||
               learningNormal.load(std::memory_order_relaxed);
    }

    bool isLearningSibilance() const noexcept { return learningSibilance.load(std::memory_order_relaxed); }
    bool isLearningBreath() const noexcept    { return learningBreath.load(std::memory_order_relaxed); }
    bool isLearningNormal() const noexcept    { return learningNormal.load(std::memory_order_relaxed); }
    float getLearningProgress() const noexcept
    {
        if (learningSibilance.load(std::memory_order_relaxed))
            return std::min(1.0f, static_cast<float>(accumFrames) / 75.0f);
        if (learningBreath.load(std::memory_order_relaxed))
            return std::min(1.0f, static_cast<float>(accumFrames) / 95.0f);
        if (learningNormal.load(std::memory_order_relaxed))
            return std::min(1.0f, static_cast<float>(accumFrames) / 120.0f);
        return 0.0f;
    }
    bool isPendingConfirmation() const noexcept { return hasPending.load(std::memory_order_relaxed); }
    int getPendingClassType() const noexcept { return pendingClass.load(std::memory_order_relaxed); }

    bool hasCustomSibProfile() const noexcept { return hasCustomSibilance.load(std::memory_order_relaxed); }
    bool hasCustomBrProfile() const noexcept  { return hasCustomBreath.load(std::memory_order_relaxed); }
    bool hasCustomNormProfile() const noexcept { return hasCustomNormal.load(std::memory_order_relaxed); }

    int getExportCount() const noexcept { return exportCounter.load(std::memory_order_relaxed); }

    struct MatchResult {
        float sibilanceScore = 0.0f;
        float breathScore = 0.0f;
        float plosiveScore = 0.0f;
        float normalScore = 0.0f;
        float rms = 0.0f;
    };

    MatchResult analyzeBlock(const float* samples, int numSamples)
    {
        MatchResult res;
        if (numSamples < 512) return res;

        // 1. RMS 音量と ZCR (ゼロ交差率) の計算
        double sumSq = 0.0;
        int zeroCrossings = 0;
        for (int i = 0; i < 512; ++i)
        {
            sumSq += samples[i] * samples[i];
            if (i > 0 && ((samples[i] >= 0.0f && samples[i - 1] < 0.0f) ||
                          (samples[i] < 0.0f && samples[i - 1] >= 0.0f)))
            {
                zeroCrossings++;
            }
        }
        res.rms = static_cast<float>(std::sqrt(sumSq / 512.0));
        const float zcr = static_cast<float>(zeroCrossings) / 512.0f;

        if (res.rms < 0.0015f)
        {
            res.normalScore = 0.0f;
            if (breathHoldCounter > 0)
            {
                breathHoldCounter--;
                smoothedBreathScore *= 0.97f;
            }
            else
            {
                smoothedBreathScore *= 0.92f;
            }
            res.breathScore = std::clamp(smoothedBreathScore, 0.0f, 1.0f);
            return res;
        }

        // 2. FFT パワースペクトル計算
        std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
        for (int i = 0; i < 512; ++i)
            fftBuffer[i] = samples[i];
        window.multiplyWithWindowingTable(fftBuffer.data(), 512);
        forwardFFT.performFrequencyOnlyForwardTransform(fftBuffer.data());

        // 32 バンドに対数集約
        std::array<float, NUM_BANDS> currentSpec {};
        double totalEnergy = 0.0;
        double lowEnergy = 0.0;  // < 150Hz
        double midEnergy = 0.0;  // 300Hz ~ 3kHz
        double highEnergy = 0.0; // > 4.5kHz

        for (int b = 0; b < NUM_BANDS; ++b)
        {
            const int startBin = static_cast<int>(std::pow(256.0, static_cast<double>(b) / NUM_BANDS));
            const int endBin   = std::max(startBin + 1, static_cast<int>(std::pow(256.0, static_cast<double>(b + 1) / NUM_BANDS)));
            
            double bandSum = 0.0;
            for (int k = startBin; k < endBin && k < 257; ++k)
            {
                const float p = fftBuffer[k] * fftBuffer[k];
                bandSum += p;
                
                const float freq = (k * 16000.0f) / 512.0f;
                if (freq < 150.0f) lowEnergy += p;
                else if (freq >= 300.0f && freq <= 3000.0f) midEnergy += p;
                else if (freq >= 4500.0f) highEnergy += p;
            }
            currentSpec[b] = static_cast<float>(bandSum);
            totalEnergy += bandSum;
        }

        // スペクトル正規化
        if (totalEnergy > 1e-9)
        {
            const float invTot = 1.0f / static_cast<float>(totalEnergy);
            for (int b = 0; b < NUM_BANDS; ++b)
                currentSpec[b] *= invTot;
        }

        const float highRatio = (midEnergy > 1e-7) ? static_cast<float>(highEnergy / (midEnergy + highEnergy)) : 0.0f;
        const float lowRatio  = (totalEnergy > 1e-7) ? static_cast<float>(lowEnergy / totalEnergy) : 0.0f;

        // 3. Learn の蓄積処理
        handleLearning(currentSpec, zcr, highRatio, res.rms);

        // 4. 通常ボーカル (Normal Vocal) 類似度の算出
        float normalSim = matchNormalVocal(currentSpec, zcr, highRatio);
        if (zcr < 0.22f && highRatio < 0.08f && res.rms > 0.008f)
            normalSim = std::max(normalSim, 0.85f);
        res.normalScore = normalSim;

        // 5. マルチテンプレート照合 (Max Similarity)
        float rawSibScore = matchSibilance(currentSpec, zcr, highRatio);
        float rawBrScore  = matchBreath(currentSpec, zcr, highRatio);

        // 6. 通常ボーカル対比 (Precision Vocal Masking):
        // ブレス継続中はマスキングを緩和して途切れを防止
        if (normalSim > 0.35f)
        {
            const float maskIntensity = (breathHoldCounter > 0) ? 0.9f : 1.8f;
            const float vocalMask = std::clamp(1.0f - (normalSim - 0.35f) * maskIntensity, 0.0f, 1.0f);
            rawSibScore *= vocalMask;
            rawBrScore  *= vocalMask;
        }

        // 7. ブレス前後を滑らかにつなぐ ホールド ＆ 適応型スムーザー
        // ブレスが立ち上がったらホールドカウンターをセット (約160ms = 20フレーム維持して途切れを防止)
        if (rawBrScore > 0.25f)
        {
            breathHoldCounter = 20; // 20 frames * 8ms = 160ms ホールド
        }
        else if (breathHoldCounter > 0)
        {
            breathHoldCounter--;
            // ホールド中は急な落ち込みを防ぎ、滑らかに維持
            rawBrScore = std::max(rawBrScore, smoothedBreathScore * 0.94f);
        }

        // アタック (素早く追従: 約25ms) と リリース (前後に滑らかに繋がる: 約150〜200ms)
        const float alphaAtt = 0.75f;
        const float alphaRel = (breathHoldCounter > 0) ? 0.04f : 0.08f;

        if (rawBrScore > smoothedBreathScore)
            smoothedBreathScore += alphaAtt * (rawBrScore - smoothedBreathScore);
        else
            smoothedBreathScore += alphaRel * (rawBrScore - smoothedBreathScore);

        res.sibilanceScore = rawSibScore;
        res.breathScore    = std::clamp(smoothedBreathScore, 0.0f, 1.0f);
        res.plosiveScore   = std::clamp(lowRatio * 2.2f, 0.0f, 1.0f);

        const float maxNoise = std::max({ res.sibilanceScore, res.breathScore, res.plosiveScore });
        res.normalScore = std::clamp(std::max(normalSim, 1.0f - maxNoise), 0.0f, 1.0f);

        return res;
    }

private:
    void stopAllLearning()
    {
        learningSibilance.store(false);
        learningBreath.store(false);
        learningNormal.store(false);
        hasPending.store(false);
        accumFrames = 0;
        accumSpectrum.fill(0.0);
        accumZcr = 0.0;
        accumHighRatio = 0.0;
    }

    void initBanks()
    {
        normalVocalBank.clear();
        normalVocalBank.push_back(female_learned_normalvocal_0);
        normalVocalBank.push_back(female_learned_normalvocal_1);

        sibilanceBank.clear();
        sibilanceBank.push_back(female_learned_sibilance_0);
        sibilanceBank.push_back(female_learned_sibilance_1);

        breathBank.clear();
        breathBank.push_back(female_learned_breath_0);
        breathBank.push_back(female_learned_breath_1);
        breathBank.push_back(female_learned_breath_2);
    }

    float matchNormalVocal(const std::array<float, NUM_BANDS>& spec, float zcr, float highRatio) const
    {
        if (hasPending.load(std::memory_order_relaxed) && pendingClass.load(std::memory_order_relaxed) == 3)
            return pendingProfile.calculateSimilarity(spec, zcr, highRatio);

        float maxScore = 0.0f;
        for (const auto& tmpl : normalVocalBank)
            maxScore = std::max(maxScore, tmpl.calculateSimilarity(spec, zcr, highRatio));

        if (hasCustomNormal.load(std::memory_order_relaxed))
            maxScore = std::max(maxScore, customNormalProfile.calculateSimilarity(spec, zcr, highRatio));

        return maxScore;
    }

    float matchSibilance(const std::array<float, NUM_BANDS>& spec, float zcr, float highRatio) const
    {
        if (hasPending.load(std::memory_order_relaxed) && pendingClass.load(std::memory_order_relaxed) == 1)
            return pendingProfile.calculateSimilarity(spec, zcr, highRatio);

        float maxScore = 0.0f;
        for (const auto& tmpl : sibilanceBank)
            maxScore = std::max(maxScore, tmpl.calculateSimilarity(spec, zcr, highRatio));

        if (hasCustomSibilance.load(std::memory_order_relaxed))
            maxScore = std::max(maxScore, customSibilanceProfile.calculateSimilarity(spec, zcr, highRatio));

        return maxScore;
    }

    float matchBreath(const std::array<float, NUM_BANDS>& spec, float zcr, float highRatio) const
    {
        if (hasPending.load(std::memory_order_relaxed) && pendingClass.load(std::memory_order_relaxed) == 2)
            return pendingProfile.calculateSimilarity(spec, zcr, highRatio);

        float maxScore = 0.0f;
        for (const auto& tmpl : breathBank)
            maxScore = std::max(maxScore, tmpl.calculateSimilarity(spec, zcr, highRatio));

        if (hasCustomBreath.load(std::memory_order_relaxed))
            maxScore = std::max(maxScore, customBreathProfile.calculateSimilarity(spec, zcr, highRatio));

        return maxScore;
    }

    void handleLearning(const std::array<float, NUM_BANDS>& spec, float zcr, float highRatio, float rms)
    {
        if (learningSibilance.load(std::memory_order_relaxed) && rms > 0.006f)
        {
            accumulateFrame(spec, zcr, highRatio);
            if (accumFrames >= 75) // 延長: 約 0.60 秒 (旧: 32 frames = 0.25s)
                finalizePending(1); // Sibilance
        }
        else if (learningBreath.load(std::memory_order_relaxed) && rms > 0.003f && rms < 0.08f)
        {
            accumulateFrame(spec, zcr, highRatio);
            if (accumFrames >= 95) // 延長: 約 0.76 秒 (旧: 28 frames = 0.22s)
                finalizePending(2); // Breath
        }
        else if (learningNormal.load(std::memory_order_relaxed) && rms > 0.010f)
        {
            accumulateFrame(spec, zcr, highRatio);
            if (accumFrames >= 120) // 延長: 約 0.96 秒 (旧: 35 frames = 0.28s)
                finalizePending(3); // Normal Vocal
        }
    }

    void accumulateFrame(const std::array<float, NUM_BANDS>& spec, float zcr, float highRatio)
    {
        for (int b = 0; b < NUM_BANDS; ++b) accumSpectrum[b] += spec[b];
        accumZcr += zcr;
        accumHighRatio += highRatio;
        accumFrames++;
    }

    void finalizePending(int pClass)
    {
        for (int b = 0; b < NUM_BANDS; ++b)
            pendingProfile.spectrum[b] = static_cast<float>(accumSpectrum[b] / accumFrames);
        pendingProfile.zcr = static_cast<float>(accumZcr / accumFrames);
        pendingProfile.highRatio = static_cast<float>(accumHighRatio / accumFrames);
        pendingProfile.isCustom = true;

        pendingClass.store(pClass);
        hasPending.store(true);
        learningSibilance.store(false);
        learningBreath.store(false);
        learningNormal.store(false);
    }

    void saveProfileToJson(const juce::String& className, const AcousticProfile& prof, const juce::String& gender, const juce::String& baseDataDir)
    {
        try {
            juce::File baseDir(baseDataDir);
            juce::File targetDir = baseDir.getChildFile(gender.toLowerCase());
            if (!targetDir.exists())
                targetDir.createDirectory();

            const auto now = juce::Time::getCurrentTime();
            const auto timeStr = now.formatted("%Y%m%d_%H%M%S") + "_" + juce::String(now.getMilliseconds());
            juce::File targetFile = targetDir.getChildFile("profile_" + className + "_" + timeStr + ".json");

            juce::String json;
            json << "{\n";
            json << "  \"gender\": \"" << gender << "\",\n";
            json << "  \"class\": \"" << className << "\",\n";
            json << "  \"timestamp\": \"" << timeStr << "\",\n";
            json << "  \"zcr\": " << juce::String(prof.zcr, 6) << ",\n";
            json << "  \"highRatio\": " << juce::String(prof.highRatio, 6) << ",\n";
            json << "  \"spectrum\": [";
            for (size_t i = 0; i < prof.spectrum.size(); ++i)
            {
                json << juce::String(prof.spectrum[i], 8) << (i + 1 < prof.spectrum.size() ? ", " : "");
            }
            json << "]\n";
            json << "}\n";

            targetFile.replaceWithText(json);
        } catch (...) {}
    }

public:
    
    std::atomic<int> totalUserLoaded { 0 };

public:
    int getTotalNormalProfiles() const noexcept    { return static_cast<int>(normalVocalBank.size()); }
    int getTotalSibilanceProfiles() const noexcept { return static_cast<int>(sibilanceBank.size()); }
    int getTotalBreathProfiles() const noexcept    { return static_cast<int>(breathBank.size()); }
    int getTotalUserLoaded() const noexcept        { return totalUserLoaded.load(std::memory_order_relaxed); }

    int loadUserProfilesFromDirectory(const juce::String& baseDataDir)
    {
        int loadedCount = 0;
        try {
            juce::File baseDir(baseDataDir);
            if (!baseDir.exists()) return 0;

            const juce::StringArray subDirs = { "female", "male" };
            for (const auto& subDirName : subDirs)
            {
                juce::File subDir = baseDir.getChildFile(subDirName);
                if (!subDir.isDirectory()) continue;

                juce::Array<juce::File> jsonFiles;
                subDir.findChildFiles(jsonFiles, juce::File::findFiles, false, "*.json");

                for (const auto& f : jsonFiles)
                {
                    auto result = juce::JSON::parse(f);
                    if (!result.isObject()) continue;

                    auto* obj = result.getDynamicObject();
                    if (obj == nullptr) continue;

                    juce::String cls = obj->getProperty("class").toString().toLowerCase();
                    float zcr = static_cast<float>(obj->getProperty("zcr"));
                    float highRatio = static_cast<float>(obj->getProperty("highRatio"));
                    auto specVar = obj->getProperty("spectrum");

                    if (!specVar.isArray()) continue;
                    auto* specArr = specVar.getArray();
                    if (specArr == nullptr || specArr->size() != NUM_BANDS) continue;

                    AcousticProfile prof;
                    prof.zcr = zcr;
                    prof.highRatio = highRatio;
                    prof.isCustom = true;
                    for (int b = 0; b < NUM_BANDS; ++b)
                    {
                        prof.spectrum[b] = static_cast<float>((*specArr)[b]);
                    }

                    if (cls.contains("normal"))
                    {
                        normalVocalBank.push_back(prof);
                        loadedCount++;
                    }
                    else if (cls.contains("sibilance") || cls.contains("sib"))
                    {
                        sibilanceBank.push_back(prof);
                        loadedCount++;
                    }
                    else if (cls.contains("breath"))
                    {
                        breathBank.push_back(prof);
                        loadedCount++;
                    }
                }
            }
        } catch (...) {}
        totalUserLoaded.store(loadedCount);
        return loadedCount;
    }

    int exportActiveProfiles(const juce::String& baseDataDir, const juce::String& gender = "female")
    {
        int count = 0;
        if (hasCustomSibilance.load(std::memory_order_relaxed))
        {
            saveProfileToJson("Sibilance", customSibilanceProfile, gender, baseDataDir);
            count++;
        }
        else
        {
            saveProfileToJson("Sibilance_Tmpl0", female_learned_sibilance_0, gender, baseDataDir);
            count++;
        }

        if (hasCustomBreath.load(std::memory_order_relaxed))
        {
            saveProfileToJson("Breath", customBreathProfile, gender, baseDataDir);
            count++;
        }
        else
        {
            saveProfileToJson("Breath_Tmpl0", female_learned_breath_0, gender, baseDataDir);
            saveProfileToJson("Breath_Quick", female_learned_breath_2, gender, baseDataDir);
            count += 2;
        }

        if (hasCustomNormal.load(std::memory_order_relaxed))
        {
            saveProfileToJson("NormalVocal", customNormalProfile, gender, baseDataDir);
            count++;
        }
        else
        {
            saveProfileToJson("NormalVocal_Tmpl0", female_learned_normalvocal_0, gender, baseDataDir);
            count++;
        }

        exportCounter.fetch_add(1, std::memory_order_relaxed);
        return count;
    }

    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, 1024> fftBuffer;

    // マルチテンプレートバンク
    std::vector<AcousticProfile> normalVocalBank;
    std::vector<AcousticProfile> sibilanceBank;
    std::vector<AcousticProfile> breathBank;

    // カスタムプロファイル
    AcousticProfile customNormalProfile;
    AcousticProfile customSibilanceProfile;
    AcousticProfile customBreathProfile;

    // 確認待ち (Pending) プロファイル
    AcousticProfile pendingProfile;
    std::atomic<bool> hasPending { false };
    std::atomic<int>  pendingClass { 0 }; // 1: Sibilance, 2: Breath, 3: Normal

    // スムーザー用内部ステート
    float smoothedBreathScore = 0.0f;
    int   breathHoldCounter   = 0;

    int accumFrames = 0;
    std::array<double, NUM_BANDS> accumSpectrum {};
    double accumZcr = 0.0;
    double accumHighRatio = 0.0;

    std::atomic<bool> learningSibilance { false };
    std::atomic<bool> learningBreath { false };
    std::atomic<bool> learningNormal { false };

    std::atomic<bool> hasCustomSibilance { false };
    std::atomic<bool> hasCustomBreath { false };
    std::atomic<bool> hasCustomNormal { false };

    std::atomic<int> exportCounter { 0 };
};

} // namespace AtafutaAudio::VocaNoiseLearnner
