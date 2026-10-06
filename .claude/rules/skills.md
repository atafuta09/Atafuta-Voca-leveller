---
paths:
  - "**/SKILL.md"
  - ".codex/skills/**"
  - ".claude/skills/**"
---

# Skill 作成・編集時のルール

このルールは Skill 関連ファイルに触れたときだけロードされます（path スコープ）。配置方針はグローバル正本 `~/.agents/AGENTS.md` の「Skills と長期記憶」を参照してください。

- 複数プロジェクト共通 Skill の git 正本は `sozenium-md/skills/<name>/SKILL.md` に置きます。`~/.agents/skills/` は repo の `skills/` への symlink、`~/.claude/skills/<name>` と `~/.codex/skills/<name>` は `~/.agents/skills/<name>/` への symlink です。Skill を追加・削除したら `bash scripts/sync-skills.sh` を実行してください。プロジェクト固有 Skill のみ、対象プロジェクトの `.codex/skills/<name>/SKILL.md`（Claude は `.claude/skills/` から symlink）に置いてください。
- `frontmatter` は `name` と `description` を短く具体的にし、モデルが「いつ起動するか」を判断できるようにしてください。
- 補助スクリプト・テンプレート・参照資料は、その Skill ディレクトリ内にまとめてください。
- 長い参照テーブルや手順の詳細は本文に詰め込みすぎず、補助ファイルへ分けて必要時だけ読ませてください（progressive disclosure）。
- 成果物は原則として日本語で書いてください。
- 例外として、英語で配布された輸入 Skill（`emil-design-eng` などの外部由来 Skill）は、原文の英語を維持して構いません。
