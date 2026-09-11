"""
train.py
========
Task 1 のデータセットを用いて VocalArtifactCNN を学習し、
評価および ONNX フォーマットへのエクスポートを行うスクリプト。
"""

import os
import sys
import numpy as np
import torch
import torch.nn as nn
from torch.utils.data import TensorDataset, DataLoader
import torch.optim as optim

# モデル定義のインポート
from audio_cnn import VocalArtifactCNN

# パス設定
CURRENT_DIR = os.path.dirname(__file__)
DATASET_PATH = os.path.join(CURRENT_DIR, "..", "data", "vocal_artifacts_dataset.npz")
MODEL_DIR = CURRENT_DIR
PT_MODEL_PATH = os.path.join(MODEL_DIR, "vocal_artifact_detector.pt")
ONNX_MODEL_PATH = os.path.join(MODEL_DIR, "vocal_artifact_detector.onnx")

# 学習ハイパーパラメータ
EPOCHS = 25
BATCH_SIZE = 32
LEARNING_RATE = 1e-3
WEIGHT_DECAY = 1e-4


def train_one_epoch(model, loader, criterion, optimizer, device):
    model.train()
    running_loss = 0.0
    correct = 0
    total = 0

    for inputs, labels in loader:
        inputs, labels = inputs.to(device), labels.to(device)
        optimizer.zero_grad()
        outputs = model(inputs)
        loss = criterion(outputs, labels)
        loss.backward()
        optimizer.step()

        running_loss += loss.item() * inputs.size(0)
        _, predicted = outputs.max(1)
        total += labels.size(0)
        correct += predicted.eq(labels).sum().item()

    return running_loss / total, (correct / total) * 100.0


@torch.no_grad()
def evaluate(model, loader, criterion, device):
    model.eval()
    running_loss = 0.0
    correct = 0
    total = 0

    for inputs, labels in loader:
        inputs, labels = inputs.to(device), labels.to(device)
        outputs = model(inputs)
        loss = criterion(outputs, labels)

        running_loss += loss.item() * inputs.size(0)
        _, predicted = outputs.max(1)
        total += labels.size(0)
        correct += predicted.eq(labels).sum().item()

    return running_loss / total, (correct / total) * 100.0


def export_to_onnx(model, onnx_path, device):
    """学習済みモデルを ONNX 形式にエクスポート"""
    print(f"\n[*] ONNX モデルをエクスポート中: {onnx_path}")
    model.eval()
    dummy_input = torch.randn(1, 1, 64, 10, device=device)

    torch.onnx.export(
        model,
        dummy_input,
        onnx_path,
        export_params=True,
        opset_version=14,
        do_constant_folding=True,
        input_names=["input"],
        output_names=["logits"],
        dynamic_axes={
            "input": {0: "batch_size"},
            "logits": {0: "batch_size"}
        }
    )
    print("[+] ONNX エクスポート完了!")


def main():
    print("=" * 60)
    print("  VocalArtifactDetector: CNN モデル学習 & ONNX エクスポート")
    print("=" * 60)

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    print(f"[*] 使用デバイス: {device}")

    if not os.path.exists(DATASET_PATH):
        print(f"[!] エラー: データセットが見つかりません: {DATASET_PATH}")
        print("    先に `dataset_pipeline/build_dataset.py` を実行してください。")
        sys.exit(1)

    print(f"[*] データセットをロード中: {DATASET_PATH}")
    data = np.load(DATASET_PATH)
    X_train, y_train = data["X_train"], data["y_train"]
    X_val, y_val     = data["X_val"], data["y_val"]
    class_names      = data["class_names"]

    print(f"    Train: {len(X_train)} samples, Val: {len(X_val)} samples")
    print(f"    クラス一覧: {class_names}")

    # テンソル化 [N, 1, 64, 10]
    train_dataset = TensorDataset(
        torch.from_numpy(X_train).unsqueeze(1).float(),
        torch.from_numpy(y_train).long()
    )
    val_dataset = TensorDataset(
        torch.from_numpy(X_val).unsqueeze(1).float(),
        torch.from_numpy(y_val).long()
    )

    train_loader = DataLoader(train_dataset, batch_size=BATCH_SIZE, shuffle=True)
    val_loader   = DataLoader(val_dataset, batch_size=BATCH_SIZE, shuffle=False)

    model = VocalArtifactCNN(num_classes=len(class_names)).to(device)
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.AdamW(model.parameters(), lr=LEARNING_RATE, weight_decay=WEIGHT_DECAY)
    scheduler = optim.lr_scheduler.CosineAnnealingLR(optimizer, T_max=EPOCHS)

    best_acc = 0.0

    print(f"\n[*] 学習開始 (全 {EPOCHS} エポック)...")
    for epoch in range(1, EPOCHS + 1):
        train_loss, train_acc = train_one_epoch(model, train_loader, criterion, optimizer, device)
        val_loss, val_acc     = evaluate(model, val_loader, criterion, device)
        scheduler.step()

        is_best = val_acc > best_acc
        if is_best:
            best_acc = val_acc
            torch.save(model.state_dict(), PT_MODEL_PATH)

        star = " ★ BEST" if is_best else ""
        print(f"  Epoch [{epoch:2d}/{EPOCHS:2d}] "
              f"Train Loss: {train_loss:.4f} (Acc: {train_acc:5.1f}%) | "
              f"Val Loss: {val_loss:.4f} (Acc: {val_acc:5.1f}%){star}")

    print(f"\n[+] 最高検証精度 (Best Val Accuracy): {best_acc:.2f} %")

    # ベストモデルのロードと ONNX 出力
    model.load_state_dict(torch.load(PT_MODEL_PATH, map_location=device))
    export_to_onnx(model, ONNX_MODEL_PATH, device)
    print("=" * 60)


if __name__ == "__main__":
    main()
