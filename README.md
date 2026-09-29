# Atafuta09Leveler (Ver 1.04)

![Atafuta09Leveler](https://img.shields.io/badge/version-1.04-blue.svg)
![VST3](https://img.shields.io/badge/format-VST3-orange.svg)
![AU](https://img.shields.io/badge/format-AU%20(macOS)-orange.svg)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![JUCE8](https://img.shields.io/badge/JUCE-8.0.4-green.svg)

**Atafuta09Leveler** は、JUCE 8 / C++20 で開発されたプロ仕様のボーカル特化型オートレベラー（Vocal Dynamics Rider）プラグインです（Windows: VST3 / macOS: VST3・Audio Unit）。  
コンプレッサーの潰れた質感を与えることなく、ボーカルの手動ボリュームオートメーション（手書きフェーダー操作）を完全に自動化し、オケに埋もれない自然で安定したボーカルトラックを瞬時に作成します。

---

## 主な特徴

- **Auto Dynamics Riding**: ターゲットレベルと許容補正幅（Range: 0〜15dB）に基づき、ボーカルの抑揚を滑らかに自動制御。
- **Modern Dark & White Mode**:
  - 実機ラック機材の質感を持つ **Dark Mode**
  - 高品位スタジオコンソールを模した高コントラストな **White Mode**（`COLOR` ボタンで瞬時切替）
- **トラック内メーター統合型ターゲットフェーダー**: スライダー内部にリアルタイム入力音量が光り上がる直感的なGUI。
- **リアルタイム波形ビジュアライザー**: 入力波形、出力波形、およびゲイン補正レーザー軌跡を60fpsで滑らかにプロット。
- **充実のファクトリープリセット ＆ ユーザープリセット保存**:
  - `Default`、`Synth Vocal`（合成音声・打ち込みボーカル特化）、`Podcast`、`Aggressive Leveler` など全8種内蔵。
  - **`SAVE` ボタン** により、自作プリセットを `%APPDATA%`（Windows）/ `~/Library`（macOS）に安全に永続化保存可能。
- **ピークホールド付きマルチメーター**: Peak（1.5秒ホールドライン付）、RMS、VU-18（0 VU = -18 dBFS、レッドゾーン警告付）を切替可能。
- **プロセッシングフィルター**:
  - **Lookahead (5ms)**: 先読みバッファ（DAW遅延補正対応）によりアタックの頭潰れを完全防止。
  - **Breath Filter**: 吹かれノイズ除去（150Hz HPF）と息継ぎ区間の音量暴走を防止。
  - **Sibilance Filter**: 歯擦音除去（4000Hz LPF）によりサ行での不自然なダッキングを防止。
- **UIスケーリング**: 50% 〜 130%（10%刻み）でウィンドウサイズを自由に拡大縮小。

---

## ドキュメント一覧

- 📖 **[取扱説明書・機能解説 (MANUAL.md)](MANUAL.md)**: 各ノブやフェーダー、フィルターの詳細な役割と使い方。
- 📜 **[バージョン別 変更履歴 (CHANGELOG.md)](CHANGELOG.md)**: Ver 1.00 から Ver 1.04 までの開発作業と更新記録。

---

## 開発環境・ビルド仕様

- **言語**: C++20 / C++17
- **フレームワーク**: JUCE 8 (8.0.4)
- **ビルドツール**: CMake 3.22+
  - Windows: Visual Studio 2022/2026 (MSVC) + Ninja
  - macOS: Xcode（Ninja があれば Ninja を優先）
- **プラグイン形式**:
  - Windows: VST3 (64-bit)
  - macOS: VST3 / Audio Unit (AUv2)、Apple Silicon + Intel ユニバーサルバイナリ、macOS 12 以降
- **プラグイン表示名**: `Atafuta09Leveler`

### ビルド方法

**Windows**

```bat
build_plugin.bat
```

**macOS**

```sh
./build_plugin_mac.sh            # build_mac/AutoLeveler_artefacts/Release/ に VST3 と AU を生成
./build_plugin_mac.sh --install  # ビルド後に ~/Library/Audio/Plug-Ins/{VST3,Components} へコピー
```

- 手動で配置する場合は、`Atafuta09Leveler.vst3` を `~/Library/Audio/Plug-Ins/VST3/`、`Atafuta09Leveler.component` を `~/Library/Audio/Plug-Ins/Components/` にコピーします。
- AU が DAW に表示されない場合は、`killall -9 AudioComponentRegistrar` を実行してから DAW を再起動してください。
- AU の動作検証: `auval -v aufx AtLv Ataf`
- ローカルビルドは ad-hoc 署名です。他の Mac へ配布する場合は Developer ID 署名と公証（notarization）が必要です。

---

## リンク

- 公式 X (Twitter): [@atafuta09](https://x.com/atafuta09)
- 公式 YouTube: [@atafuta09](https://www.youtube.com/@atafuta09)
