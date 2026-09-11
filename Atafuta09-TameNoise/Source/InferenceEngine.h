// ==============================================================================
// InferenceEngine.h
// VocalArtifactDetector の 2D-CNN C++ 高速推論クラス
// 外部依存なし、ゼロ動的アロケーション（リアルタイムセーフ）
// ==============================================================================
#pragma once

#include "ModelWeights.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace AtafutaAudio::TameNoise {

struct DetectionResult {
    float normalProb = 0.0f;
    float sibilanceProb = 0.0f;
    float plosiveProb = 0.0f;
    float breathProb = 0.0f;
    int dominantClass = 0; // 0: Normal, 1: Sibilance, 2: Plosive, 3: Breath
};

class InferenceEngine {
public:
    InferenceEngine() = default;

    /**
     * 正規化メルスペクトログラム [64][10] から 4 クラスの確率を出力
     */
    DetectionResult predict(const float melInput[N_MELS][TIME_FRAMES])
    {
        // ----------------------------------------------------------------------
        // Block 1: Conv1 + ReLU + MaxPool(2x2)
        // In: [1, 64, 10] -> Conv: [16, 64, 10] -> Pool: [16, 32, 5]
        // ----------------------------------------------------------------------
        for (int oc = 0; oc < 16; ++oc)
        {
            const float* w_oc = &CONV1_WEIGHT[oc * (1 * 3 * 3)];
            const float bias = CONV1_BIAS[oc];

            for (int h = 0; h < 64; ++h)
            {
                for (int w = 0; w < 10; ++w)
                {
                    float sum = bias;
                    for (int kh = 0; kh < 3; ++kh)
                    {
                        const int in_h = h + kh - 1; // pad=1
                        if (in_h < 0 || in_h >= 64) continue;

                        for (int kw = 0; kw < 3; ++kw)
                        {
                            const int in_w = w + kw - 1; // pad=1
                            if (in_w < 0 || in_w >= 10) continue;

                            sum += melInput[in_h][in_w] * w_oc[kh * 3 + kw];
                        }
                    }
                    // ReLU
                    conv1_out[oc][h][w] = std::max(0.0f, sum);
                }
            }

            // MaxPool 2x2
            for (int ph = 0; ph < 32; ++ph)
            {
                for (int pw = 0; pw < 5; ++pw)
                {
                    const int h0 = ph * 2;
                    const int w0 = pw * 2;
                    float m = conv1_out[oc][h0][w0];
                    if (h0 + 1 < 64) m = std::max(m, conv1_out[oc][h0 + 1][w0]);
                    if (w0 + 1 < 10) m = std::max(m, conv1_out[oc][h0][w0 + 1]);
                    if (h0 + 1 < 64 && w0 + 1 < 10) m = std::max(m, conv1_out[oc][h0 + 1][w0 + 1]);
                    pool1_out[oc][ph][pw] = m;
                }
            }
        }

        // ----------------------------------------------------------------------
        // Block 2: Conv2 + ReLU + MaxPool(2x2)
        // In: [16, 32, 5] -> Conv: [32, 32, 5] -> Pool: [32, 16, 2]
        // ----------------------------------------------------------------------
        for (int oc = 0; oc < 32; ++oc)
        {
            const float* w_oc = &CONV2_WEIGHT[oc * (16 * 3 * 3)];
            const float bias = CONV2_BIAS[oc];

            for (int h = 0; h < 32; ++h)
            {
                for (int w = 0; w < 5; ++w)
                {
                    float sum = bias;
                    for (int ic = 0; ic < 16; ++ic)
                    {
                        const float* w_ic = &w_oc[ic * (3 * 3)];
                        for (int kh = 0; kh < 3; ++kh)
                        {
                            const int in_h = h + kh - 1;
                            if (in_h < 0 || in_h >= 32) continue;

                            for (int kw = 0; kw < 3; ++kw)
                            {
                                const int in_w = w + kw - 1;
                                if (in_w < 0 || in_w >= 5) continue;

                                sum += pool1_out[ic][in_h][in_w] * w_ic[kh * 3 + kw];
                            }
                        }
                    }
                    conv2_out[oc][h][w] = std::max(0.0f, sum);
                }
            }

            // MaxPool 2x2
            for (int ph = 0; ph < 16; ++ph)
            {
                for (int pw = 0; pw < 2; ++pw)
                {
                    const int h0 = ph * 2;
                    const int w0 = pw * 2;
                    float m = conv2_out[oc][h0][w0];
                    if (h0 + 1 < 32) m = std::max(m, conv2_out[oc][h0 + 1][w0]);
                    if (w0 + 1 < 5)  m = std::max(m, conv2_out[oc][h0][w0 + 1]);
                    if (h0 + 1 < 32 && w0 + 1 < 5) m = std::max(m, conv2_out[oc][h0 + 1][w0 + 1]);
                    pool2_out[oc][ph][pw] = m;
                }
            }
        }

        // ----------------------------------------------------------------------
        // Block 3: Conv3 + ReLU + AdaptiveAvgPool2d((1, 1))
        // In: [32, 16, 2] -> Conv: [64, 16, 2] -> Pool: [64]
        // ----------------------------------------------------------------------
        for (int oc = 0; oc < 64; ++oc)
        {
            const float* w_oc = &CONV3_WEIGHT[oc * (32 * 3 * 3)];
            const float bias = CONV3_BIAS[oc];
            double channelSum = 0.0;

            for (int h = 0; h < 16; ++h)
            {
                for (int w = 0; w < 2; ++w)
                {
                    float sum = bias;
                    for (int ic = 0; ic < 32; ++ic)
                    {
                        const float* w_ic = &w_oc[ic * (3 * 3)];
                        for (int kh = 0; kh < 3; ++kh)
                        {
                            const int in_h = h + kh - 1;
                            if (in_h < 0 || in_h >= 16) continue;

                            for (int kw = 0; kw < 3; ++kw)
                            {
                                const int in_w = w + kw - 1;
                                if (in_w < 0 || in_w >= 2) continue;

                                sum += pool2_out[ic][in_h][in_w] * w_ic[kh * 3 + kw];
                            }
                        }
                    }
                    const float act = std::max(0.0f, sum);
                    channelSum += act;
                }
            }

            // AdaptiveAvgPool2d((1, 1)): 16 x 2 = 32 要素の平均
            pool3_out[oc] = static_cast<float>(channelSum / (16 * 2));
        }

        // ----------------------------------------------------------------------
        // Classifier Head: FC1 + ReLU -> FC2 -> Softmax
        // ----------------------------------------------------------------------
        // FC1: [32][64] * [64] + [32]
        for (int i = 0; i < 32; ++i)
        {
            float sum = FC1_BIAS[i];
            const float* w_i = &FC1_WEIGHT[i * 64];
            for (int j = 0; j < 64; ++j)
            {
                sum += pool3_out[j] * w_i[j];
            }
            fc1_out[i] = std::max(0.0f, sum);
        }

        // FC2: [4][32] * [32] + [4]
        float maxLogit = -1e9f;
        for (int i = 0; i < 4; ++i)
        {
            float sum = FC2_BIAS[i];
            const float* w_i = &FC2_WEIGHT[i * 32];
            for (int j = 0; j < 32; ++j)
            {
                sum += fc1_out[j] * w_i[j];
            }
            logits[i] = sum;
            if (sum > maxLogit) maxLogit = sum;
        }

        // Softmax
        float sumExp = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            probs[i] = std::exp(logits[i] - maxLogit);
            sumExp += probs[i];
        }
        const float invSumExp = 1.0f / (sumExp + 1e-9f);
        for (int i = 0; i < 4; ++i)
        {
            probs[i] *= invSumExp;
        }

        DetectionResult res;
        res.normalProb    = probs[0];
        res.sibilanceProb = probs[1];
        res.plosiveProb   = probs[2];
        res.breathProb    = probs[3];

        int bestClass = 0;
        float bestProb = probs[0];
        for (int i = 1; i < 4; ++i)
        {
            if (probs[i] > bestProb)
            {
                bestProb = probs[i];
                bestClass = i;
            }
        }
        res.dominantClass = bestClass;

        return res;
    }

private:
    // 静的バッファ（ヒープ動的アロケーションなし）
    float conv1_out[16][64][10];
    float pool1_out[16][32][5];

    float conv2_out[32][32][5];
    float pool2_out[32][16][2];

    float pool3_out[64];

    float fc1_out[32];
    float logits[4];
    float probs[4];
};

} // namespace AtafutaAudio::TameNoise
