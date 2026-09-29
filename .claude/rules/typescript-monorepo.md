---
paths:
  - "turbo.json"
  - "pnpm-workspace.yaml"
  - "pnpm-lock.yaml"
  - "package.json"
  - "**/package.json"
  - "tsconfig*.json"
  - "**/tsconfig*.json"
---

# TypeScript モノレポ構成ファイルに触れたときのルール

このルールは、モノレポの構成ファイル（`turbo.json` / `pnpm-workspace.yaml` / `package.json` / `tsconfig*.json`）に触れたときだけロードされます（path スコープ）。

- MUST: 作業を始める前に `typescript-monorepo` Skill を読んでください。規約の正本はそちらです（`apps/` と `packages/` の分離、責務パッケージ、`workspace:*`、`exports`、JIT コンパイル方針、`package.json` の生成・更新手順）。ここに本文は複製しません。
- MUST: Skill の内容を Codex へ委譲する場合は、委譲プロンプトに該当規約を含めてください。Skill は `~/.agents/skills/` 経由で Codex からも読めますが、`.claude/rules/` は Claude Code 専用のため、このルールが読まれたことは委譲先に伝わりません。

このルールの存在意義は、規約の再掲ではなく「該当ファイルに触れた時点で必ず Skill を参照させる」ことです。モデルの起動判断に委ねず、パスで決定的に発火させます。
