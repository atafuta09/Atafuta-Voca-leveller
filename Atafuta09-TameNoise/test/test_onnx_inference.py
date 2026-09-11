"""
test_onnx_inference.py
======================
ONNX Runtime を用いて学習済み ONNX モデルに WAV 音声を流し込み、
時系列で「k（Plosive）」や「s（Sibilance）」などの確率変化を検証するテストスクリプト。
"""

import os
import sys
import argparse
import numpy as np
import soundfile as sf
import onnxruntime as ort
import torchaudio.transforms as T
import torch

# クラス定義
CLASS_NAMES = ["Normal", "Sibilance", "Plosive", "Breath"]

# メルスペクトログラムパラメータ (学習時と完全同一)
TARGET_SR = 16000
N_MELS = 64
N_FFT = 512
HOP_LENGTH = 128
WINDOW_SAMPLES = int(TARGET_SR * 0.080) # 80ms = 1280 samples
STRIDE_SAMPLES = int(TARGET_SR * 0.020) # 20ms = 320 samples (推論ステップ)

CURRENT_DIR = os.path.dirname(__file__)
DEFAULT_ONNX_PATH = os.path.join(CURRENT_DIR, "..", "model", "vocal_artifact_detector.onnx")


def softmax(x):
    e_x = np.exp(x - np.max(x, axis=-1, keepdims=True))
    return e_x / np.sum(e_x, axis=-1, keepdims=True)


def generate_synthetic_test_wav(output_path, duration=3.0, sample_rate=TARGET_SR):
    """
    検証用の人工テスト音声を生成
    0.0s - 0.7s: 母音 (Normal, 440Hz+倍音)
    0.7s - 1.0s: 破裂音 'k' (Plosive, 突発的アタック)
    1.0s - 1.8s: 母音 (Normal)
    1.8s - 2.2s: 歯擦音 's' (Sibilance, 6kHz~10kHz高域ノイズ)
    2.2s - 2.8s: ブレス音 (Breath, 低音量かすれ息)
    2.8s - 3.0s: 無音
    """
    total_samples = int(duration * sample_rate)
    wav = np.zeros(total_samples, dtype=np.float32)
    t = np.linspace(0, duration, total_samples, endpoint=False)

    # 1. 通常歌声 (0.0s - 0.7s)
    idx1 = (t >= 0.0) & (t < 0.7)
    wav[idx1] = (0.40 * np.sin(2 * np.pi * 440 * t[idx1]) +
                 0.20 * np.sin(2 * np.pi * 880 * t[idx1]) +
                 0.10 * np.sin(2 * np.pi * 1320 * t[idx1])).astype(np.float32)

    # 2. 破裂音 'k' (0.7s - 0.85s: 鋭いアタック + 減衰)
    idx2 = (t >= 0.70) & (t < 0.85)
    t_rel = t[idx2] - 0.70
    burst = np.exp(-t_rel * 45) * np.sin(2 * np.pi * 1800 * t_rel) * 0.75
    wav[idx2] = burst.astype(np.float32)

    # 3. 再び通常歌声 (1.0s - 1.8s)
    idx3 = (t >= 1.0) & (t < 1.8)
    wav[idx3] = (0.35 * np.sin(2 * np.pi * 523.25 * t[idx3]) +
                 0.18 * np.sin(2 * np.pi * 1046.5 * t[idx3])).astype(np.float32)

    # 4. 歯擦音 's' (1.8s - 2.2s: 7kHz バンドノイズ)
    idx4 = (t >= 1.8) & (t < 2.2)
    white = np.random.uniform(-0.35, 0.35, np.sum(idx4)).astype(np.float32)
    # 簡易高域強調 (微分)
    sibilance = np.diff(white, prepend=white[0]) * 2.5
    wav[idx4] = sibilance

    # 5. ブレス音 (2.2s - 2.8s: 低音量ノイズ)
    idx5 = (t >= 2.2) & (t < 2.8)
    wav[idx5] = np.random.uniform(-0.02, 0.02, np.sum(idx5)).astype(np.float32)

    sf.write(output_path, wav, sample_rate)
    print(f"[*] 人工テスト音声 WAV を生成しました: {output_path}")
    return output_path


def run_inference(wav_path, onnx_path):
    print("=" * 65)
    print("  VocalArtifactDetector: ONNX 時系列推論テスト")
    print("=" * 65)
    print(f"[*] WAV ファイル:  {wav_path}")
    print(f"[*] ONNX モデル: {onnx_path}")

    if not os.path.exists(onnx_path):
        print(f"[!] エラー: ONNX モデルが見つかりません: {onnx_path}")
        print("    先に `model/train.py` を実行してモデルを学習・エクスポートしてください。")
        sys.exit(1)

    # ONNX Runtime セッション開始
    session = ort.InferenceSession(onnx_path)
    input_name = session.get_inputs()[0].name
    output_name = session.get_outputs()[0].name

    # 音声読み込み
    data, sr = sf.read(wav_path)
    if data.ndim > 1:
        data = np.mean(data, axis=1) # モノラル化
    
    waveform = torch.from_numpy(data).float().unsqueeze(0)
    if sr != TARGET_SR:
        resampler = T.Resample(sr, TARGET_SR)
        waveform = resampler(waveform)

    total_samples = waveform.shape[1]
    duration_sec = total_samples / TARGET_SR
    print(f"[*] 音声の長さ: {duration_sec:.2f} 秒 ({total_samples} samples, @{TARGET_SR}Hz)")

    mel_transform = T.MelSpectrogram(
        sample_rate=TARGET_SR,
        n_fft=N_FFT,
        win_length=N_FFT,
        hop_length=HOP_LENGTH,
        n_mels=N_MELS,
        power=2.0
    )

    print("\n--- 時系列推論トラッキング (スライド窓: 80ms, ステップ: 20ms) ---")
    print(" 時間 (秒) | 優勢クラス  | Normal | Sibilance | Plosive | Breath | 確信度バー")
    print("-----------+-------------+--------+-----------+---------+--------+------------------")

    for start_idx in range(0, total_samples - WINDOW_SAMPLES, STRIDE_SAMPLES):
        cur_time = (start_idx + WINDOW_SAMPLES // 2) / TARGET_SR
        slice_wav = waveform[:, start_idx:start_idx + WINDOW_SAMPLES]

        # RMS 判定 (完全な無音はスキップ)
        rms = torch.sqrt(torch.mean(slice_wav ** 2)).item()
        if rms < 0.001:
            continue

        # メルスペクトログラム計算
        mel = mel_transform(slice_wav) # [1, 64, 11]
        log_mel = torch.log(torch.clamp(mel, min=1e-5))
        norm_mel = (log_mel - log_mel.mean()) / (log_mel.std() + 1e-6)
        norm_mel = norm_mel[:, :, :10] # [1, 64, 10] にスライス
        mel_input = norm_mel.unsqueeze(0).cpu().numpy().astype(np.float32) # [1, 1, 64, 10]

        # ONNX 推論
        logits = session.run([output_name], {input_name: mel_input})[0]
        probs = softmax(logits)[0] # (4,)

        best_class_idx = np.argmax(probs)
        best_class = CLASS_NAMES[best_class_idx]
        best_prob = probs[best_class_idx]

        # 視覚バー
        bar_len = int(best_prob * 14)
        bar = "[" + "=" * bar_len + " " * (14 - bar_len) + "]"

        # ノイズ（Sibilance, Plosive, Breath）が跳ね上がった箇所をハイライト
        marker = "★" if best_class in ["Sibilance", "Plosive", "Breath"] and best_prob > 0.60 else " "

        print(f" {cur_time:6.2f} s | {best_class:11s} | "
              f"{probs[0]*100:5.1f}% | {probs[1]*100:6.1f}%  | {probs[2]*100:5.1f}%  | {probs[3]*100:5.1f}% | {bar} {marker}")

    print("=" * 65)
    print("[+] 時系列推論テストが完了しました。")


def main():
    parser = argparse.ArgumentParser(description="VocalArtifactDetector ONNX Inference Tester")
    parser.add_argument("--wav", type=str, default=None, help="テスト対象の WAV ファイルパス")
    parser.add_argument("--onnx", type=str, default=DEFAULT_ONNX_PATH, help="ONNX モデルパス")
    args = parser.parse_args()

    wav_path = args.wav
    if wav_path is None:
        # 指定がない場合は自動テスト WAV を生成
        test_wav_path = os.path.join(CURRENT_DIR, "synthetic_test.wav")
        generate_synthetic_test_wav(test_wav_path)
        wav_path = test_wav_path

    run_inference(wav_path, args.onnx)


if __name__ == "__main__":
    main()
