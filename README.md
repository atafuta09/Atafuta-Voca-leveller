# Atafuta09Leveler (Ver 1.05)

![Atafuta09Leveler](https://img.shields.io/badge/version-1.05-blue.svg)
![Format](https://img.shields.io/badge/format-VST3%20%7C%20AU-orange.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS-lightgrey.svg)
![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![JUCE8](https://img.shields.io/badge/JUCE-8.0.4-green.svg)

**Atafuta09Leveler** は、JUCE 8 / C++17 で開発されたプロ仕様のボーカル特化型オートレベラー（Vocal Dynamics Rider）プラグインです。  
コンプレッサーのような音痩せや潰れた質感を与えることなく、ボーカルの手動ボリュームオートメーション（手書きフェーダー操作）を高度に自動化し、オケに埋もれない自然で安定したボーカルトラックを瞬時に作成します。

さらに、最新の **AI TameNoise 検出回路** と **音質変化ゼロ（Zero-Coloration）サイドチェインフィルター** を搭載し、歯擦音（サ行）・ブレス・マイク吹かれによる誤動作を完全に抑制します。

---

## 主な特徴

- **Auto Dynamics Riding**:
  - ターゲットレベルと許容補正幅（Range: 0〜15dB）に基づき、ボーカルの抑揚を滑らかに自動制御。
- **AI TameNoise ボーカルノイズ検知・抑制機能 (v1.05 新機能)**:
  - 87データセットの音響プロファイルに基づく軽量検出エンジンを統合。
  - 通常の歌声をマスキングしながら、耳障りな歯擦音（サ行）やブレス・破裂音をミリ秒単位で検出し、過剰なゲインアップをブロック＆音楽的リダクション。
  - `TAME NOISE` (ON/OFF), `LISTEN` (ノイズ成分試聴), `TAME AMOUNT` (0〜100%) ノブで直感操作。
- **音質変化ゼロ（Zero-Coloration）サイドチェイン HPF (v1.05 新機能)**:
  - メイン音声パスはビットパーフェクト（原音忠実・位相歪みゼロ）。
  - レベラー検出器パスにのみ 100Hz HPF を適用し、マイクの吹かれや低域ノイズによる不自然なポンピングリダクションを完全防止。
- **Modern Dark & White Mode**:
  - 実機ラック機材の質感を持つ **Dark Mode**
  - 高品位スタジオコンソールを模した高コントラストな **White Mode**（`COLOR` ボタンで瞬時切替）
- **トラック内メーター統合型ターゲットフェーダー**:
  - スライダー内部に入力音量がリアルタイムに発光表示される直感的なGUI。
- **リアルタイム波形ビジュアライザー**:
  - 入力波形、出力波形、およびゲイン補正レーザー軌跡を60fpsで滑らかにプロット。
- **ユーザープリセット保存・管理機能**:
  - **`SAVE` ボタン** により、ボーカルスタイルに合わせた自作プリセットを安全に永続化保存・ワンクリック呼び出し可能。
- **ピークホールド付きマルチメーター**:
  - Peak（1.5秒ホールドライン付）、RMS、VU-18（0 VU = -18 dBFS、レッドゾーン警告付）を切替可能。
- **Lookahead (5ms)**:
  - 先読みバッファ（DAW遅延補正対応）によりアタックの頭潰れを防止。
- **UIスケーリング**:
  - 50% 〜 130%（10%刻み）でウィンドウサイズを自由に拡大縮小。

---

## ドキュメント一覧

- 📖 **[取扱説明書・機能解説 (MANUAL.md)](MANUAL.md)**: 各ノブやフェーダー、フィルターの詳細な役割と使い方。
- 📜 **[バージョン別 変更履歴 (CHANGELOG.md)](CHANGELOG.md)**: Ver 1.00 から Ver 1.05 までの開発作業と更新記録。
- 📝 **[開発TODOリスト (TODO.md)](TODO.md)**: 今後のロードマップと実装課題。

---

## プラットフォーム・ビルド仕様

- **対応プラットフォーム**: Windows 10/11 (x64), macOS (Apple Silicon arm64 / Intel x86_64)
- **プラグイン形式**: VST3, AU (Audio Unit ※macOSのみ)
- **言語 / 規格**: C++17
- **フレームワーク**: JUCE 8 (8.0.4)
- **ビルドツール**: CMake 3.22+, Visual Studio 2022/2026 (Windows), Xcode / Clang (macOS)
- **プラグイン表示名**: `Atafuta09Leveler`

### Windows でのビルド
```cmd
build_plugin.bat
```
または CMake:
```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### macOS でのビルド (Universal Binary)
```bash
cmake -B build-mac -G Xcode -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac --config Release
```

※ GitHub Actions によるクロスビルド（Windows / macOS 自動生成）にも対応しています。

---

## リンク

- 公式 X (Twitter): [@atafuta09](https://x.com/atafuta09)
- 公式 YouTube: [@atafuta09](https://www.youtube.com/@atafuta09)
