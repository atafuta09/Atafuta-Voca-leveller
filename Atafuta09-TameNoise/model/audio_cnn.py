"""
audio_cnn.py
============
プラグイン組み込みを想定した超軽量 2D-CNN 音声アーティファクト分類モデル
入力: [Batch, 1, 64, 10] (メルスペクトログラム 64ビン x 10フレーム = 80ms)
出力: [Batch, 4] (Normal, Sibilance, Plosive, Breath のロジット)
"""

import torch
import torch.nn as nn
import torch.nn.functional as F


class VocalArtifactCNN(nn.Module):
    def __init__(self, num_classes=4):
        super().__init__()
        
        # Block 1: [B, 1, 64, 10] -> [B, 16, 32, 5]
        self.conv1 = nn.Conv2d(1, 16, kernel_size=3, padding=1)
        self.bn1 = nn.BatchNorm2d(16)
        self.pool1 = nn.MaxPool2d(kernel_size=2, stride=2)

        # Block 2: [B, 16, 32, 5] -> [B, 32, 16, 2]
        self.conv2 = nn.Conv2d(16, 32, kernel_size=3, padding=1)
        self.bn2 = nn.BatchNorm2d(32)
        self.pool2 = nn.MaxPool2d(kernel_size=2, stride=2)

        # Block 3: [B, 32, 16, 2] -> [B, 64, 1, 1]
        self.conv3 = nn.Conv2d(32, 64, kernel_size=3, padding=1)
        self.bn3 = nn.BatchNorm2d(64)
        self.global_pool = nn.AdaptiveAvgPool2d((1, 1))

        # Classifier Head
        self.fc1 = nn.Linear(64, 32)
        self.dropout = nn.Dropout(0.25)
        self.fc2 = nn.Linear(32, num_classes)

    def forward(self, x):
        # 入力が [B, 64, 10] の場合はチャンネル次元を追加して [B, 1, 64, 10] に成形
        if x.dim() == 3:
            x = x.unsqueeze(1)

        x = self.pool1(F.relu(self.bn1(self.conv1(x))))
        x = self.pool2(F.relu(self.bn2(self.conv2(x))))
        x = self.global_pool(F.relu(self.bn3(self.conv3(x))))

        x = torch.flatten(x, 1) # [B, 64]
        x = F.relu(self.fc1(x))
        x = self.dropout(x)
        logits = self.fc2(x)   # [B, 4]

        return logits

    @torch.no_grad()
    def predict_probabilities(self, x):
        """推論時: Softmax 確率を出力"""
        logits = self.forward(x)
        return F.softmax(logits, dim=-1)


if __name__ == "__main__":
    model = VocalArtifactCNN(num_classes=4)
    dummy_input = torch.randn(2, 1, 64, 10)
    output = model(dummy_input)
    print("モデルテスト:")
    print(f"  入力形状: {dummy_input.shape}")
    print(f"  出力形状: {output.shape}")
    total_params = sum(p.numel() for p in model.parameters() if p.requires_grad)
    print(f"  総パラメータ数: {total_params:,} (軽量・超高速)")
