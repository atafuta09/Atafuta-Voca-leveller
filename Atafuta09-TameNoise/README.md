# Atafuta09 VocaNoise Learnner
**AI Vocal Noise & Acoustic Profile Learning VST3 Plugin**

---

## 概要 (Overview)

`Atafuta09 VocaNoise Learnner` は、ボーカルの特定ノイズ（歯擦音・ブレス音・破裂音）および通常歌唱の音響特徴をリアルタイムに学習・蓄積・モニタリングするための**独立した専用VST3プラグイン**です。

### オートレベラー本体 (`Atafuta09Leveler`) との関係性
- **位置づけ**: オートレベラー本体 (`Atafuta09Leveler`) とは明確に分離された、独立稼働の研究開発・学習ツールです。
- **今後の役割**: 日常的なボーカルミックス現場において、男性・女性問わず多様なボーカルノイズの音響プロファイルを収集・学習・検証し続ける専用モジュールとして継続運用されます。
- **最終目標**: 本プラグインで収集・最適化された高品質な音響モデル・テンプレートバンクは、将来的に `Atafuta09Leveler` 本体のオートレベラー回路（サイドチェイン／ダイナミクス検出器）へ直接統合されます。

---

## 主な機能 (Key Features)

1. **女性ボーカル 87 データセットから学習済みの標準マルチテンプレートバンク**:
   - プラグインを挿した瞬間（Learn を押さなくても）から、検証済みの高品質テンプレートで全自動検知が動作します：
     - **Normal Vocal Bank**: 豊かな母音基音、張りのあるシンガーズフォルマント (25サンプル)
     - **Sibilance Bank**: 自然なサ行 (19サンプル)、鋭い高域サ行 (10サンプル)
     - **Breath Bank**: 擦れ/早めのブレス (4サンプル)、深部肺吸気 (8サンプル)、自然な呼吸音 (21サンプル)
2. **通常歌唱の完全マスク (Precision Vocal Masking)**:
   - 通常の歌声（母音フォルマント）が出ている間はノイズ判定を完全にマスクして誤検知をゼロに防止。
   - 歌声が途切れた無声区間ではサ行・ブレス判定を超高感度化し、**取り逃しを根絶**。
3. **適応型 二重時定数弾道スムーザー (Adaptive Ballistics Filter)**:
   - **通常ブレス**: アタック 15ms / リリース 100ms で「すう〜っ」と滑らかに追従。
   - **早めのブレス (Quick Inhale)**: アタック 5ms で一瞬の息も逃さず捉え、リリース 40ms で直後の歌い出しを削りません。
4. **ラーニング後の「男性 / 女性」選択・仕分け保存フロー (Human-in-the-Loop)**:
   - 音声キャプチャ完了後、`NOISE LISTEN` ボタンで耳で試聴確認。
   - `[♀ SAVE AS FEMALE]` をクリック → `learned_data/female/` へ自動保存。
   - `[♂ SAVE AS MALE]` をクリック → `learned_data/male/` へ自動保存。
   - `[✗ DISCARD]` をクリック → 破棄して安全にやり直せます。

---

## ディレクトリ構成 (Directory Structure)

```
Atafuta09-TameNoise/
├── CMakeLists.txt              # CMake ビルド定義 (Atafuta09VocaNoiseLearnner)
├── README.md                   # 本ドキュメント
├── .gitignore                  # Git 除外設定 (.venv, build 等)
├── Source/
│   ├── AudioFingerprint.h      # 音響プロファイルバンク & 特徴量抽出照合エンジン
│   ├── PluginProcessor.h/cpp   # VST3 プロセッサ (16kHzリサンプラー & 判定)
│   ├── PluginEditor.h/cpp      # UI (ダークモダン 4連メーター & 男女選択コントロール)
│   ├── InferenceEngine.h       # 軽量推論エンジン
│   ├── MelSpectrogram.h        # メルスペクトログラム抽出
│   └── ModelWeights.h          # 内部モデル定数
└── learned_data/               # 音響プロファイル保管庫 (JSON)
    ├── female/                 # 女性ボーカル学習データ (87ファイル)
    └── male/                   # 男性ボーカル学習データ (今後蓄積)
```

---

## ビルド手順 (Build)

```powershell
# ビルドディレクトリの作成と構成
cmake -B build -S . -DCOPY_PLUGIN_AFTER_BUILD=TRUE

# Release ビルドと VST3 配置
cmake --build build --config Release
```

ビルド完了後、`C:\Program Files\Common Files\VST3\Atafuta09VocaNoiseLearnner.vst3` へ自動インストールされます。
