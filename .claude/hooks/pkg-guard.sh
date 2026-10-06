#!/usr/bin/env bash
# PreToolUse(Bash / Edit / Write / MultiEdit) パッケージ管理ガード
#
# 目的: typescript-monorepo / AGENTS.md が定める pnpm への統一と、package.json の
#       依存関係を直接編集しない規約を、指示テキストではなく機械的に強制する。
#       違反時は exit 2 で tool 呼び出しをブロックし、代替手段を stderr で返す。
# 参照: .codex/skills/typescript-monorepo/SKILL.md
#
# 設定: .claude/settings.json の hooks.PreToolUse から呼ばれる。
#       stdin に {"tool_name":"...","tool_input":{...}} 形式の JSON を受け取る。
#
# 設計方針: pnpm-lock.yaml / pnpm-workspace.yaml があるプロジェクトだけを対象にする。
#           パース不能・想定外入力では fail-open（exit 0）にしてセッションを壊さない。
#           確実に違反と判定できた時だけブロックし、無関係な入力では重い処理をしない。

project_dir="${CLAUDE_PROJECT_DIR:-$PWD}"
if [ ! -f "${project_dir}/pnpm-lock.yaml" ] && [ ! -f "${project_dir}/pnpm-workspace.yaml" ]; then
  exit 0
fi

input="$(cat)"

extract_tool_name() {
  if command -v jq >/dev/null 2>&1; then
    printf '%s' "$1" | jq -r 'if .tool_name | type == "string" then .tool_name else "" end' 2>/dev/null
  elif command -v python3 >/dev/null 2>&1; then
    printf '%s' "$1" | python3 -c 'import sys,json
try:
    value=json.load(sys.stdin).get("tool_name","")
    print(value if isinstance(value,str) else "")
except Exception:
    pass' 2>/dev/null
  else
    # パーサ非搭載環境向けの粗いフォールバック
    printf '%s' "$1" | sed -n 's/.*"tool_name"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p'
  fi
}

extract_scalar() {
  key="$2"
  if command -v jq >/dev/null 2>&1; then
    printf '%s' "$1" | jq -r --arg key "$key" \
      'if .tool_input[$key] | type == "string" then .tool_input[$key] else "" end' 2>/dev/null
  elif command -v python3 >/dev/null 2>&1; then
    printf '%s' "$1" | python3 -c 'import sys,json
try:
    value=json.load(sys.stdin).get("tool_input",{}).get(sys.argv[1],"")
    print(value if isinstance(value,str) else "")
except Exception:
    pass' "$key" 2>/dev/null
  else
    # エスケープを完全には復元しない。確実に読めない入力は後段で一致せず fail-open になる。
    printf '%s' "$1" | sed -n "s/.*\"${key}\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p"
  fi
}

extract_edit_content() {
  if command -v jq >/dev/null 2>&1; then
    printf '%s' "$1" | jq -r --arg tool "$2" '
      if $tool == "Edit" then
        [.tool_input.old_string, .tool_input.new_string]
      elif $tool == "Write" then
        [.tool_input.content]
      elif $tool == "MultiEdit" then
        [.tool_input.edits[]? | .old_string, .new_string]
      else
        []
      end
      | .[]? | select(type == "string")
    ' 2>/dev/null
  elif command -v python3 >/dev/null 2>&1; then
    printf '%s' "$1" | python3 -c 'import sys,json
try:
    data=json.load(sys.stdin).get("tool_input",{})
    tool=sys.argv[1]
    values=[]
    if tool == "Edit":
        values=[data.get("old_string"),data.get("new_string")]
    elif tool == "Write":
        values=[data.get("content")]
    elif tool == "MultiEdit":
        for edit in data.get("edits",[]):
            if isinstance(edit,dict):
                values.extend((edit.get("old_string"),edit.get("new_string")))
    print("\n".join(value for value in values if isinstance(value,str)))
except Exception:
    pass' "$2" 2>/dev/null
  else
    # 最終フォールバックでは入力全体を使う。file_path と tool_name の判定後に限るため、
    # 依存関係を示す明確な文字列がなければブロックしない。
    printf '%s' "$1" | sed 's/\\"/"/g'
  fi
}

tool_name="$(extract_tool_name "$input")"
[ -z "$tool_name" ] && exit 0

block() {
  echo "ブロック: $1" >&2
  exit 2
}

# 引用符内を除いた「コマンド解析用」文字列を使い、説明文や検索文字列にある
# npm install / yarn add をコマンドと誤認しない。区切りごとの先頭語だけを評価する。
strip_heredoc_bodies() {
  command -v python3 >/dev/null 2>&1 || return 1

  printf '%s' "$1" | python3 -c 'import re,sys
try:
    lines=sys.stdin.read().splitlines(keepends=True)
    kept=[]
    pending=[]
    heredoc=re.compile(r"<<(-?)[ \t]*(?:\047([^\047\n]+)\047|\"([^\"\n]+)\"|([A-Za-z_][A-Za-z0-9_]*))")
    for line in lines:
        if pending:
            delimiter,strip_tabs=pending[0]
            candidate=line.rstrip("\r\n")
            if strip_tabs:
                candidate=candidate.lstrip("\t")
            if candidate == delimiter:
                pending.pop(0)
            kept.append("\n" if line.endswith("\n") else "")
            continue
        kept.append(line)
        for match in heredoc.finditer(line):
            delimiter=next(group for group in match.groups()[1:] if group is not None)
            pending.append((delimiter,match.group(1) == "-"))
    sys.stdout.write("".join(kept))
except Exception:
    sys.exit(1)' 2>/dev/null
}

classify_package_command() {
  command_text="$1"

  # ヒアドキュメント本文は実行コマンドではない。本文を確実に除去できない環境では、
  # データ中の npm / yarn を誤検知しないよう、この入力全体を fail-open にする。
  if printf '%s' "$command_text" | grep -Eq '<<-?[[:space:]]*'; then
    command_text="$(strip_heredoc_bodies "$command_text")" || return 1
  fi

  unquoted="$(printf '%s' "$command_text" | sed "s/'[^']*'//g; s/\"[^\"]*\"//g")"
  segments="$(printf '%s' "$unquoted" | tr ';&|' '\n')"

  while IFS= read -r segment; do
    set -f
    set -- $segment
    set +f
    [ "$#" -lt 2 ] && continue

    manager="$1"
    subcommand="$2"
    shift 2

    if [ "$manager" = "npm" ]; then
      if [ "$subcommand" = "i" ] || [ "$subcommand" = "install" ]; then
        for argument in "$@"; do
          if [ "$argument" = "-g" ] || [ "$argument" = "--global" ]; then
            printf '%s\n' npm-global
            return 0
          fi
        done
      fi

      case "$subcommand" in
        # npm i は npm install の別名。引数がなければ全依存のインストールなので
        # pnpm install を案内し、パッケージ指定がある場合だけ pnpm add を案内する。
        install|i)
          if [ "$#" -eq 0 ]; then printf '%s\n' npm-install; else printf '%s\n' npm-add; fi
          return 0
          ;;
        add) printf '%s\n' npm-add; return 0 ;;
        ci) printf '%s\n' npm-ci; return 0 ;;
        update) printf '%s\n' npm-update; return 0 ;;
        uninstall|remove|rm) printf '%s\n' npm-remove; return 0 ;;
      esac
    elif [ "$manager" = "yarn" ]; then
      if [ "$subcommand" = "global" ] && [ "${1:-}" = "add" ]; then
        printf '%s\n' yarn-global
        return 0
      fi

      case "$subcommand" in
        add) printf '%s\n' yarn-add; return 0 ;;
        install) printf '%s\n' yarn-install; return 0 ;;
        remove) printf '%s\n' yarn-remove; return 0 ;;
        upgrade) printf '%s\n' yarn-upgrade; return 0 ;;
      esac
    fi
  done <<EOF
$segments
EOF

  return 1
}

if [ "$tool_name" = "Bash" ]; then
  cmd="$(extract_scalar "$input" command)"
  [ -z "$cmd" ] && exit 0

  # npm / yarn がなければ、引用符除去やコマンド区間の解析を行わない。
  printf '%s' "$cmd" | grep -Eq '(^|[^[:alnum:]_-])(npm|yarn)([^[:alnum:]_-]|$)' || exit 0
  violation="$(classify_package_command "$cmd")" || exit 0

  case "$violation" in
    npm-global|yarn-global)
      block "グローバルインストールは禁止です。pnpm exec <コマンド> か、ルートの devDependency を使ってください"
      ;;
    npm-install|yarn-install)
      block "pnpm install を使ってください"
      ;;
    npm-ci)
      block "pnpm install --frozen-lockfile を使ってください"
      ;;
    npm-add|yarn-add)
      block "pnpm add <パッケージ名> を使ってください（ワークスペース内は --filter <パッケージ名> を併用）"
      ;;
    npm-update|yarn-upgrade)
      block "pnpm update を使ってください（ワークスペース内は --filter <パッケージ名> を併用）"
      ;;
    npm-remove|yarn-remove)
      block "pnpm remove <パッケージ名> を使ってください（ワークスペース内は --filter <パッケージ名> を併用）"
      ;;
  esac
  exit 0
fi

case "$tool_name" in
  Edit|Write|MultiEdit) ;;
  *) exit 0 ;;
esac

file_path="$(extract_scalar "$input" file_path)"
[ "${file_path##*/}" = "package.json" ] || exit 0

# Edit の payload だけでは JSON 上の文脈を確定できないため、たとえば依存関係の
# `"zod": "^3.0.0"` をキー名なしで `"^3.1.0"` に変える編集は検出できない。
# engines などにある同形のバージョン値を誤検知しないことを優先し、この取りこぼしは
# fail-open として許容する。依存関係キーかプロトコルを含む明確な編集だけを止める。
edit_content="$(extract_edit_content "$input" "$tool_name")"
[ -z "$edit_content" ] && exit 0

if printf '%s' "$edit_content" | grep -Eq \
  '"(dependencies|devDependencies|peerDependencies|optionalDependencies)"[[:space:]]*:|workspace:|catalog:'; then
  block "package.json の依存関係を直接編集しないでください。pnpm add / pnpm update / pnpm remove を使ってください（ワークスペース内は --filter <パッケージ名>、内部依存は --workspace を併用）"
fi

exit 0
