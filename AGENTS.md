# Agent Instructions

**このファイルには、このプロジェクトに固有の事項だけを書いてください。**普遍ルール（全プロジェクト共通の判断原則）はグローバル指示にあります。ここへ重複させないでください。

## このプロジェクトについて

- ボーカル特化のオートレベラー（Vocal Dynamics Rider）VST3 プラグイン `Atafuta09Leveler`。JUCE 8.0.4 / C++（CMake は `CMAKE_CXX_STANDARD 17`）。ユーザー向けの機能説明は `README.md` / `MANUAL.md`、変更履歴は `CHANGELOG.md`、残タスクは `TODO.md`。
- `Source/`: DSP（`PluginProcessor.*`）、GUI（`PluginEditor.*`、`ModernDarkLookAndFeel.h`）。`ui-preview/`: GUI 検討用の静的 HTML モック（ビルド対象外）。JUCE は `JUCE/` があればそれを、無ければ CMake の FetchContent で取得する。
- ビルドは Windows では `build_plugin.bat`（MSVC + Ninja、ターゲット `AutoLeveler_VST3`）、macOS では `build_plugin_mac.sh`（Xcode または Ninja、ターゲット `AutoLeveler_VST3` / `AutoLeveler_AU`、出力先 `build_mac/`）。自動テストは無い。macOS では AU を `auval -v aufx AtLv Ataf` で検証できる（事前に `~/Library/Audio/Plug-Ins/Components/` へ配置が必要）。
- 注意: CMake のターゲット名は `AutoLeveler`、製品名は `Atafuta09Leveler`。バージョンは `CMakeLists.txt` の `project(VERSION)` と各ドキュメントで揃える。他社の商標・製品名をプリセット名や文言に使わない。オーディオスレッド（`processBlock`）内ではメモリ確保・ロック・ファイル I/O を行わない。

## Git

- NEVER: リモートへ出る Git 操作（push、PR 作成・マージ）、履歴改変（rebase、force push）、pull、ローカルブランチの削除を、ユーザーの明示的な依頼なしに実行しないでください。
- MAY: 依頼された作業の範囲でのローカルブランチ作成・コミットは、都度の確認なしで行って構いません（ツール既定の「コミットは依頼時のみ」と矛盾する場合はこちらを優先してください）。
- MUST: ブランチ切り替えは、未コミットの差分を確認してから行ってください。

## モデル分担

- 既定では、調査から実装・検証までメインセッションが自分で行います。
- MUST: 他モデル（Codex、Cursor の Composer / Grok、Claude など）への委譲は、ユーザーが明示的に指示した場合だけ行ってください（手順は委譲先ごとの `delegate-codex` / `delegate-cursor` / `delegate-claude` Skill）。NEVER: 作業の種類を理由に、指示なしで自動委譲しないでください。
- NEVER: Blender の操作（`bpy` スクリプト、アドオン、MCP 経由のシーン操作など）は、委譲を指示された場合でも他モデルへ委譲しないでください。メインセッションが直接実行してください。

## 決定的ガードレールと参照先

- Git / GitHub / パッケージ管理の危険操作は `.claude/settings.json` と `.claude/hooks/`（PreToolUse）、コミット件名は `.githooks/commit-msg` と `.github/workflows/commit-msg.yml` で機械的にブロックされます。禁止事項の詳細は各ファイルが正本です。
- `.githooks/commit-msg` は `git config core.hooksPath .githooks` で有効化します。`import-foundation` が取り込み時に条件付きで自動設定しますが、clone ごと・マシンごとのローカル設定なので、新しい環境では `git config core.hooksPath` で確認してください。`.github/workflows/commit-msg.yml` は PR 内の全コミット件名を同じフックで検証します。
- 共有の正本は `.claude/settings.json` です。`.claude/settings.local.json` は個人環境用（gitignore 済み）です。
- このプロジェクトだけで使う Skill は `.codex/skills/<name>/SKILL.md` に置いてください（Claude Code は `.claude/skills/` から symlink）。複数プロジェクト共通の Skills は `~/.agents/skills/` にあり（git 正本は `sozenium-md`）、ここには置きません。
- プロジェクト固有の詳細仕様・主要ファイル・ビルド / テスト手順・外部連携・UI 方針は、`docs/` 配下に仕様書として置いてください。機密を含むなら `.gitignore` で git 管理から外してください。

## 土台の更新

- この土台（エージェント指示と決定的ガードレール）は `sozenium-md` リポジトリから `import-foundation` Skill で取り込んでいます。
- ガードレールを最新化する場合は、同 Skill の `update.sh` を使ってください。既定は dry-run で、`--apply` でガードレール系ファイルだけを更新します。`AGENTS.md` / `CLAUDE.md` は指示ファイルとして扱われ、自動上書きされません。
