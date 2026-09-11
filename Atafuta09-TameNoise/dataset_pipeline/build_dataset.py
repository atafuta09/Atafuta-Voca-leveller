"""
build_dataset.py
================
Hugging Face の LibriSpeech データセットと Wav2Vec2 CTC Forced Alignment を用いて、
4 クラス（Normal, Sibilance, Plosive, Breath）の音素タイミングを自動検出し、
メルスペクトログラムのデータセットを構築・保存するスクリプト。
"""

import os
import sys
import numpy as np
import torch
import torchaudio
import torchaudio.transforms as T
from datasets import load_dataset
from transformers import Wav2Vec2ForCTC, Wav2Vec2Processor
from tqdm import tqdm

# ==============================================================================
# 設定パラメータ
# ==============================================================================
TARGET_SR = 16000          # 16 kHz
SLICE_DURATION_SEC = 0.080 # 80 ms (1280 samples)
SLICE_SAMPLES = int(TARGET_SR * SLICE_DURATION_SEC) # 1280

# メルスペクトログラム設定 (64 mel bins x 10 frames)
N_MELS = 64
N_FFT = 512
HOP_LENGTH = 128

# クラス定義
CLASS_NAMES = ["Normal", "Sibilance", "Plosive", "Breath"]
CLASS_TO_IDX = {name: idx for idx, name in enumerate(CLASS_NAMES)}

# 音素・文字マッピング
SIBILANCE_CHARS = set("SZ")     # 英語テキスト中の s, z (sh は s として CTC 検出)
PLOSIVE_CHARS   = set("KPTBDG")  # 破裂音 (k, p, t, b, d, g)
NORMAL_CHARS    = set("AEIOU")   # 母音 (通常有声音)

# 出力パス
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "data")
OUTPUT_PATH = os.path.join(OUTPUT_DIR, "vocal_artifacts_dataset.npz")


def get_mel_transform(sample_rate=TARGET_SR):
    """メルスペクトログラム変換モジュール"""
    return T.MelSpectrogram(
        sample_rate=sample_rate,
        n_fft=N_FFT,
        win_length=N_FFT,
        hop_length=HOP_LENGTH,
        n_mels=N_MELS,
        power=2.0
    )


def extract_mel(waveform_segment, mel_transform):
    """波形セグメントから対数メルスペクトログラム (1, 64, T) を計算"""
    mel = mel_transform(waveform_segment)
    log_mel = torch.log(torch.clamp(mel, min=1e-5))
    # 平均0、分散1に正規化
    norm_mel = (log_mel - log_mel.mean()) / (log_mel.std() + 1e-6)
    return norm_mel.squeeze(0).cpu().numpy() # (64, T)


def main():
    print("=" * 60)
    print("  VocalArtifactDetector: データセット自動構築パイプライン")
    print("=" * 60)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[*] 使用デバイス: {device}")
    if device.type == "cuda":
        print(f"[*] GPU 名: {torch.cuda.get_device_name(0)}")

    os.makedirs(OUTPUT_DIR, exist_ok=True)

    # 1. Wav2Vec2 CTC モデルとプロセッサのロード
    model_id = "facebook/wav2vec2-base-960h"
    print(f"[*] Wav2Vec2 アライメントモデルをロード中: {model_id}")
    processor = Wav2Vec2Processor.from_pretrained(model_id)
    model = Wav2Vec2ForCTC.from_pretrained(model_id).to(device)
    model.eval()

    mel_transform = get_mel_transform().to(device)

    import io
    import soundfile as sf
    import datasets

    # 2. LibriSpeech のストリーミングロード (openslr/librispeech_asr)
    print("[*] Hugging Face より LibriSpeech (clean) ストリーミング接続中...")
    dataset = datasets.load_dataset("openslr/librispeech_asr", "clean", split="train.100", streaming=True)
    dataset = dataset.cast_column("audio", datasets.Audio(decode=False))

    # クラスごとの目標サンプル数
    TARGET_PER_CLASS = 400 # プロトタイプ検証用: 各クラス400サンプル (計1600)
    samples_by_class = {cls: [] for cls in CLASS_NAMES}

    print(f"[*] 各クラス {TARGET_PER_CLASS} サンプル (計 {TARGET_PER_CLASS * 4} サンプル) の自動抽出を開始...")
    pbar = tqdm(total=TARGET_PER_CLASS * 4, desc="Samples collected")

    for item in dataset:
        # 全クラス目標達成で終了
        if all(len(v) >= TARGET_PER_CLASS for v in samples_by_class.values()):
            break

        audio_info = item["audio"]
        audio_bytes = audio_info["bytes"]
        if not audio_bytes:
            continue

        wav_raw, orig_sr = sf.read(io.BytesIO(audio_bytes))
        waveform = torch.from_numpy(wav_raw).float()
        if orig_sr != TARGET_SR:
            resampler = T.Resample(orig_sr, TARGET_SR)
            waveform = resampler(waveform)

        if waveform.dim() == 1:
            waveform = waveform.unsqueeze(0)

        # 音声が短すぎる場合はスキップ
        if waveform.shape[1] < SLICE_SAMPLES * 4:
            continue

        # Wav2Vec2 推論による CTC ロジット算出
        input_values = processor(waveform.squeeze(0), return_tensors="pt", sampling_rate=TARGET_SR).input_values.to(device)
        with torch.no_grad():
            logits = model(input_values).logits # (1, Time, Vocab)

        predicted_ids = torch.argmax(logits, dim=-1)[0]
        # Wav2Vec2 の 1 フレームあたりのサンプル数 (通常 320 サンプル = 20ms)
        frame_stride = waveform.shape[1] / logits.shape[1]

        # トークン列をスキャンして音素区間を特定
        vocab = processor.tokenizer.get_vocab()
        id_to_token = {v: k for k, v in vocab.items()}

        num_frames = len(predicted_ids)
        for t in range(1, num_frames - 1):
            token = id_to_token.get(predicted_ids[t].item(), "")
            if not token or token == "<pad>" or token == "|":
                continue

            center_sample = int(t * frame_stride)
            start_sample = center_sample - (SLICE_SAMPLES // 2)
            end_sample   = start_sample + SLICE_SAMPLES

            if start_sample < 0 or end_sample > waveform.shape[1]:
                continue

            slice_wav = waveform[:, start_sample:end_sample].to(device)
            # RMS チェック (極小音量の誤検出を除外)
            rms = torch.sqrt(torch.mean(slice_wav ** 2)).item()
            if rms < 0.005:
                continue

            matched_class = None
            if any(c in SIBILANCE_CHARS for c in token):
                if len(samples_by_class["Sibilance"]) < TARGET_PER_CLASS:
                    matched_class = "Sibilance"
            elif any(c in PLOSIVE_CHARS for c in token):
                if len(samples_by_class["Plosive"]) < TARGET_PER_CLASS:
                    matched_class = "Plosive"
            elif any(c in NORMAL_CHARS for c in token):
                if len(samples_by_class["Normal"]) < TARGET_PER_CLASS:
                    matched_class = "Normal"

            if matched_class:
                mel_feat = extract_mel(slice_wav, mel_transform)
                mel_feat = mel_feat[:, :10] # 仕様通り 10 フレーム (64, 10) にスライス
                if mel_feat.shape == (64, 10):
                    samples_by_class[matched_class].append(mel_feat)
                    pbar.update(1)
                    pbar.set_postfix({c: len(samples_by_class[c]) for c in CLASS_NAMES})

        # ブレス音 / 吸気音の抽出: 発話直前・直後の低エネルギー区間 (0.002 < RMS < 0.035)
        if len(samples_by_class["Breath"]) < TARGET_PER_CLASS:
            # 先頭や末尾のポーズ区間を探索
            for s in range(0, min(int(TARGET_SR * 1.5), waveform.shape[1] - SLICE_SAMPLES), SLICE_SAMPLES // 2):
                breath_slice = waveform[:, s:s + SLICE_SAMPLES].to(device)
                b_rms = torch.sqrt(torch.mean(breath_slice ** 2)).item()
                if 0.002 < b_rms < 0.035: # 典型的なブレス音量
                    b_mel = extract_mel(breath_slice, mel_transform)
                    b_mel = b_mel[:, :10]
                    if b_mel.shape == (64, 10):
                        samples_by_class["Breath"].append(b_mel)
                        pbar.update(1)
                        if len(samples_by_class["Breath"]) >= TARGET_PER_CLASS:
                            break

    pbar.close()

    # データセット集約
    X_list = []
    y_list = []
    print("\n[*] 抽出完了サマリー:")
    for cls_name in CLASS_NAMES:
        arr = np.array(samples_by_class[cls_name])
        count = len(arr)
        print(f"  ・{cls_name:10s}: {count:4d} サンプル (形状: {arr.shape if count > 0 else 'none'})")
        if count > 0:
            X_list.append(arr)
            y_list.append(np.full(count, CLASS_TO_IDX[cls_name], dtype=np.int64))

    X_all = np.concatenate(X_list, axis=0) # (N, 64, 10)
    y_all = np.concatenate(y_list, axis=0) # (N,)

    # シャッフル
    indices = np.random.permutation(len(X_all))
    X_all = X_all[indices]
    y_all = y_all[indices]

    # 訓練用 (85%) と 検証用 (15%) に分割
    split_idx = int(len(X_all) * 0.85)
    X_train, X_val = X_all[:split_idx], X_all[split_idx:]
    y_train, y_val = y_all[:split_idx], y_all[split_idx:]

    print(f"\n[*] データセットを保存中: {OUTPUT_PATH}")
    np.savez_compressed(
        OUTPUT_PATH,
        X_train=X_train,
        y_train=y_train,
        X_val=X_val,
        y_val=y_val,
        class_names=CLASS_NAMES,
        sample_rate=TARGET_SR,
        n_mels=N_MELS,
        slice_samples=SLICE_SAMPLES
    )
    print(f"[+] 保存完了! 総サンプル数: {len(X_all)} (Train: {len(X_train)}, Val: {len(X_val)})")
    print("=" * 60)


if __name__ == "__main__":
    main()
