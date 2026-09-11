"""
export_weights_cpp.py
=====================
学習済みモデル (vocal_artifact_detector.pt) の重みと
torchaudio 準拠の Mel フィルタバンクを C++ ヘッダー (Source/ModelWeights.h) に出力する。
Conv2d と BatchNorm2d を事前に数学的に統合 (fuse) し、C++ 側の計算負荷と複雑度を最小化。
"""

import os
import sys
import torch
import torchaudio.functional as F_audio
from audio_cnn import VocalArtifactCNN

CURRENT_DIR = os.path.dirname(__file__)
PT_MODEL_PATH = os.path.join(CURRENT_DIR, "vocal_artifact_detector.pt")
OUTPUT_HEADER_PATH = os.path.join(CURRENT_DIR, "..", "Source", "ModelWeights.h")


def fuse_conv_bn(conv, bn):
    w = conv.weight.data
    b = conv.bias.data if conv.bias is not None else torch.zeros(conv.out_channels)
    gamma = bn.weight.data
    beta = bn.bias.data
    mean = bn.running_mean.data
    var = bn.running_var.data
    eps = bn.eps

    std = torch.sqrt(var + eps)
    scale = gamma / std

    w_fused = w * scale.view(-1, 1, 1, 1)
    b_fused = (b - mean) * scale + beta
    return w_fused, b_fused


def main():
    print("=" * 60)
    print("  VocalArtifactDetector: C++ 重みエクスポート")
    print("=" * 60)

    if not os.path.exists(PT_MODEL_PATH):
        print(f"[!] エラー: {PT_MODEL_PATH} が存在しません。先に train.py を実行してください。")
        sys.exit(1)

    model = VocalArtifactCNN(num_classes=4)
    model.load_state_dict(torch.load(PT_MODEL_PATH, map_location="cpu", weights_only=True))
    model.eval()

    # 1. Conv + BN の Fuse
    w1, b1 = fuse_conv_bn(model.conv1, model.bn1) # [16, 1, 3, 3], [16]
    w2, b2 = fuse_conv_bn(model.conv2, model.bn2) # [32, 16, 3, 3], [32]
    w3, b3 = fuse_conv_bn(model.conv3, model.bn3) # [64, 32, 3, 3], [64]

    w_fc1 = model.fc1.weight.data # [32, 64]
    b_fc1 = model.fc1.bias.data   # [32]

    w_fc2 = model.fc2.weight.data # [4, 32]
    b_fc2 = model.fc2.bias.data   # [4]

    # 2. Mel フィルタバンクの計算 (n_fft=512 -> n_freqs=257, n_mels=64, sr=16000)
    # torchaudio.functional.melscale_fbanks は [n_freqs, n_mels] = [257, 64]
    mel_fbanks = F_audio.melscale_fbanks(
        n_freqs=257,
        f_min=0.0,
        f_max=8000.0,
        n_mels=64,
        sample_rate=16000,
        norm="slaney",
        mel_scale="htk"
    ) # shape: (257, 64)

    # 転置して [64, 257] (各 mel bin に対する FFT ビンの重み)
    mel_fbanks_t = mel_fbanks.t().contiguous() # (64, 257)

    # 3. C++ ヘッダーファイルの書き出し
    print(f"[*] C++ ヘッダーを出力中: {OUTPUT_HEADER_PATH}")

    with open(OUTPUT_HEADER_PATH, "w", encoding="utf-8") as f:
        f.write("// ==============================================================================\n")
        f.write("// ModelWeights.h\n")
        f.write("// VocalArtifactDetector の学習済みパラメータおよび Mel フィルタバンク定数\n")
        f.write("// (export_weights_cpp.py により自動生成)\n")
        f.write("// ==============================================================================\n\n")
        f.write("#pragma once\n\n")
        f.write("namespace AtafutaAudio::TameNoise {\n\n")

        # 定数
        f.write("constexpr int NUM_CLASSES = 4;\n")
        f.write("constexpr int N_MELS = 64;\n")
        f.write("constexpr int N_FFT = 512;\n")
        f.write("constexpr int N_FREQS = 257;\n")
        f.write("constexpr int TIME_FRAMES = 10;\n\n")

        def write_array_1d(name, tensor):
            arr = tensor.flatten().tolist()
            f.write(f"inline const float {name}[{len(arr)}] = {{\n    ")
            for i, val in enumerate(arr):
                f.write(f"{val:.8e}f, ")
                if (i + 1) % 8 == 0:
                    f.write("\n    ")
            f.write("\n};\n\n")

        # Mel フィルタバンク
        f.write("// Mel Filterbank Matrix [64][257]\n")
        write_array_1d("MEL_FILTERBANK", mel_fbanks_t)

        # Fused Conv1
        f.write("// Conv1 Fused: Weight [16][1][3][3], Bias [16]\n")
        write_array_1d("CONV1_WEIGHT", w1)
        write_array_1d("CONV1_BIAS", b1)

        # Fused Conv2
        f.write("// Conv2 Fused: Weight [32][16][3][3], Bias [32]\n")
        write_array_1d("CONV2_WEIGHT", w2)
        write_array_1d("CONV2_BIAS", b2)

        # Fused Conv3
        f.write("// Conv3 Fused: Weight [64][32][3][3], Bias [64]\n")
        write_array_1d("CONV3_WEIGHT", w3)
        write_array_1d("CONV3_BIAS", b3)

        # FC1
        f.write("// FC1: Weight [32][64], Bias [32]\n")
        write_array_1d("FC1_WEIGHT", w_fc1)
        write_array_1d("FC1_BIAS", b_fc1)

        # FC2
        f.write("// FC2: Weight [4][32], Bias [4]\n")
        write_array_1d("FC2_WEIGHT", w_fc2)
        write_array_1d("FC2_BIAS", b_fc2)

        f.write("} // namespace AtafutaAudio::TameNoise\n")

    print(f"[+] エクスポート完了: {OUTPUT_HEADER_PATH}")
    print("=" * 60)


if __name__ == "__main__":
    main()
