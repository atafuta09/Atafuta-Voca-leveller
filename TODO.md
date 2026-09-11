# Atafuta Vocal Leveler - 開発TODOリスト

## 現在の進捗状況
- **Atafuta09Leveler v1.0.5**: リリース完了（TameNoise AI 検出回路統合、音質変化0サイドチェイン HPF 実装、Release ビルド・VST3 配置完了）
- **Atafuta09 VocaNoise Learnner v1.3.1**: 独立ボーカルノイズ学習・プロファイル収集専用 VST3 プラグイン完成（87女性ボーカルデータ学習済み、男女仕分け保存機能搭載）

---

## 完了したフェーズ: Atafuta09 VocaNoise Learnner 開発 & オートレベラー統合

### 📌 タスク 1: 独立学習プラグインの開発 (`Atafuta09 VocaNoise Learnner`)
- [x] **新仕様に基づく独立モジュールの作成 (`Atafuta09-TameNoise`)**
  - 超軽量 CNN + 32バンド音響指紋複合エンジンの設計
- [x] **DAW 検証用独立 VST3 プラグインの作成 (`Atafuta09 VocaNoise Learnner.vst3`)**
  - 洗練されたダークモダン UI（560x510px、60fps スムーズメーター、LED）
  - **TAME NOISE LISTEN ボタン**: 検出ノイズ成分のみをソロ可聴モニターする機能
  - **Learn（音響プロファイル学習）機能の実装**:
    - `[LEARN SIBILANCE]`, `[LEARN BREATH]`, `[LEARN NORMAL]`
    - **誤学習防止「試聴・確認（Human-in-the-Loop）フロー」**:
      - キャプチャ完了後に自動で確認モードに入り、ユーザーが LISTEN ボタンで耳で試聴・確認
      - `[♀ SAVE AS FEMALE]` / `[♂ SAVE AS MALE]` で男女専用フォルダへ仕分け保存
      - 失敗時は `[✕ DISCARD]` で安全に破棄
    - **87 検証済み女性ボーカルデータセットによる完全学習・標準搭載**:
      - NormalVocal (25), Sibilance (29), Breath (33)
      - 通常歌声の精密フォルマントマスクによる誤検知根絶 ＆ 超高感度検知
      - 通常ブレス（15/100ms）＆ 早めのブレス（2/40ms）対応の適応型弾道スムーザー
  - Release ビルドおよびシステム VST3 フォルダ (`C:\Program Files\Common Files\VST3`) への配置完了

### 📌 タスク 2: オートレベラー本体 (`Atafuta09Leveler v1.0.5`) への統合
- [x] **旧ブレス・歯擦音分離フィルター（IIR帯域通過）の完全撤廃**
  - 原音の位相歪みや帯域欠損の原因となる旧フィルターを廃止
- [x] **リダクション用 音質変化ゼロ（Zero-Coloration）サイドチェインフィルター (`SC FILTER`) の新設**
  - スルー音声パスは完全ビットパーフェクト（原音忠実・音質劣化ゼロ）
  - 検出器（サイドチェイン）パスのみ 100Hz HPF を適用し、吹かれや低域ゴトつきによる過大リダクション（ポンピング）を防止
  - トップヘッダー右端にワンクリックで切り替え可能な `SC FILTER` ボタンを配置
- [x] **87 学習済み AI TameNoise 検知回路の統合**
  - 16kHz リサンプリングによるリアルタイム音響プロファイルマッチング
  - 通常歌唱マスキング（歌唱中はノイズ判定を抑制）
  - ノイズ検出時のオートレベラー誤ゲインアップ防止 ＆ 音楽的リダクション
- [x] **シンプルな 3 操作子による TameNoise UI コントロール**
  - `[TAME NOISE]`: AI ノイズ抑制機能のワンクリック ON/OFF
  - `[LISTEN]`: 検出されたノイズ成分（サ行・ブレス等）のみをソロ可聴モニター
  - `TAME AMOUNT` ノブ: 効き具合の調整ノブ（0〜100%、**デフォルト 30%**）
- [x] **Release ビルド・VST3 自動配置完了 (`C:\Program Files\Common Files\VST3\Atafuta09Leveler.vst3`)**

---

## 次期フェーズ: 男性ボーカルデータの収集・学習
### 📌 タスク 3: 男性ボーカルデータの収集・学習
- [ ] `[♂ SAVE AS MALE]` による男性ボーカルノイズおよび歌声の蓄積
- [ ] 男性ボーカル用マルチテンプレートバンクの構築
