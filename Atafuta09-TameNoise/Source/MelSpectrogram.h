// ==============================================================================
// MelSpectrogram.h
// JUCE DSP FFT を用いた 64 mel bins x 10 frames 対数メルスペクトログラム抽出器
// ==============================================================================
#pragma once

#include <juce_dsp/juce_dsp.h>
#include "ModelWeights.h"
#include <cmath>
#include <vector>
#include <algorithm>

namespace AtafutaAudio::TameNoise {

class MelSpectrogramExtractor {
public:
    MelSpectrogramExtractor()
        : forwardFFT(9), // 2^9 = 512 points
          window(512, juce::dsp::WindowingFunction<float>::hann, false)
    {
    }

    /**
     * 16kHz・1280サンプルの波形から 64 mel bins x 10 frames の正規化メルスペクトログラムを抽出
     * @param inputSamples: 1280 サンプルの float 配列
     * @param outputMel: [64][10] の出力バッファ (行: mel bin, 列: time frame)
     */
    void process(const float* inputSamples, float outputMel[N_MELS][TIME_FRAMES])
    {
        float logMel[N_MELS][TIME_FRAMES];
        double sum = 0.0;
        double sumSq = 0.0;
        const int totalElements = N_MELS * TIME_FRAMES;

        // 10 フレームの STFT と Mel フィルタバンク適用
        // hop_length = 128
        for (int frame = 0; frame < TIME_FRAMES; ++frame)
        {
            const int startSample = frame * 128;
            
            // FFT バッファの初期化 (512 * 2 = 1024 floats for complex)
            std::fill(fftBuffer.begin(), fftBuffer.end(), 0.0f);
            
            // 窓関数の適用とコピー
            for (int i = 0; i < N_FFT; ++i)
            {
                fftBuffer[i] = inputSamples[startSample + i];
            }
            window.multiplyWithWindowingTable(fftBuffer.data(), N_FFT);

            // FFT 実行
            forwardFFT.performFrequencyOnlyForwardTransform(fftBuffer.data());

            // fftBuffer[0..256] にはマグニチュードが入っているので 2乗してパワースペクトルに変換
            for (int k = 0; k < N_FREQS; ++k)
            {
                powerSpectrum[k] = fftBuffer[k] * fftBuffer[k];
            }

            // Mel フィルタバンクの乗算 [64][257] * [257]
            for (int m = 0; m < N_MELS; ++m)
            {
                float melEnergy = 0.0f;
                const float* filterRow = &MEL_FILTERBANK[m * N_FREQS];
                for (int k = 0; k < N_FREQS; ++k)
                {
                    melEnergy += filterRow[k] * powerSpectrum[k];
                }

                // clamp(min=1e-5) & log
                if (melEnergy < 1e-5f)
                    melEnergy = 1e-5f;
                
                float val = std::log(melEnergy);
                logMel[m][frame] = val;

                sum += val;
                sumSq += val * val;
            }
        }

        // 平均と標準偏差の計算 (Python の norm_mel = (log_mel - mean) / (std + 1e-6))
        const float mean = static_cast<float>(sum / totalElements);
        const float variance = static_cast<float>((sumSq / totalElements) - (mean * mean));
        const float stdDev = std::sqrt(std::max(variance, 0.0f));
        const float invStd = 1.0f / (stdDev + 1e-6f);

        for (int m = 0; m < N_MELS; ++m)
        {
            for (int t = 0; t < TIME_FRAMES; ++t)
            {
                outputMel[m][t] = (logMel[m][t] - mean) * invStd;
            }
        }
    }

private:
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, N_FFT * 2> fftBuffer;
    std::array<float, N_FREQS> powerSpectrum;
};

} // namespace AtafutaAudio::TameNoise
