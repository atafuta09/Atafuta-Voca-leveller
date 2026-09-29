#!/usr/bin/env bash
# PreToolUse(Bash) ガード
#
# 目的: commit-and-git / AGENTS.md が「決定的に止めたい」とする Git 禁止事項を、
#       指示テキストではなく機械的に強制する。違反時は exit 2 で tool 呼び出しを
#       ブロックし、理由を stderr で Claude に返す。
# 参照: .codex/skills/commit-and-git/SKILL.md
#
# 設定: .claude/settings.json の hooks.PreToolUse(matcher: "Bash") から呼ばれる。
#       stdin に {"tool_input":{"command":"..."}} 形式の JSON を受け取る。
#
# 設計方針: パース不能・想定外入力では fail-open（exit 0）にしてセッションを壊さない。
#           確実に違反と判定できた時だけブロックする。

input="$(cat)"

extract_command() {
  if command -v jq >/dev/null 2>&1; then
    printf '%s' "$1" | jq -r '.tool_input.command // ""' 2>/dev/null
  elif command -v python3 >/dev/null 2>&1; then
    printf '%s' "$1" | python3 -c 'import sys,json
try:
    print(json.load(sys.stdin).get("tool_input",{}).get("command",""))
except Exception:
    pass' 2>/dev/null
  else
    # パーサ非搭載環境向けの粗いフォールバック
    printf '%s' "$1" | sed -n 's/.*"command"[[:space:]]*:[[:space:]]*"\(.*\)".*/\1/p'
  fi
}

cmd="$(extract_command "$input")"
[ -z "$cmd" ] && exit 0

block() {
  echo "ブロック: $1" >&2
  echo "→ commit-and-git スキルの規約に反しています。明示パス／安全な代替で実行してください。" >&2
  exit 2
}

# 引用符内（コミットメッセージ等）を除いた「フラグ解析用」文字列。
# フラグはクォート外にあるため、メッセージ本文に -a / --all 等が含まれても誤検知しない。
flags="$(printf '%s' "$cmd" | sed "s/'[^']*'//g; s/\"[^\"]*\"//g")"

fhas() { printf '%s' "$flags" | grep -Eq -- "$1"; }   # フラグ解析用（クォート除去後）
ihas() { printf '%s' "$cmd"   | grep -Eiq -- "$1"; }   # メッセージ内容用（原文・大小無視）

# 1. git add . / -A / --all（全件ステージング禁止。明示パスを使う）
#    "git add ./path" のような明示サブパスは許可（. の直後が空白か行末の時だけブロック）
if fhas 'git[[:space:]]+add([[:space:]]+[^&|;]*)?([[:space:]]-A|[[:space:]]--all|[[:space:]]\.([[:space:]]|$))'; then
  block "git add . / -A / --all は禁止です。git add -- <path> で明示的にステージングしてください"
fi

# 2. git commit -a / --all（明示ステージングの迂回）。-am のような連結短縮フラグも検出。
#    -ma（= -m a, メッセージ "a"）を誤検知しないよう、m より前に a が来る短縮形だけを対象にする。
if fhas 'git[[:space:]]+commit[^&|;]*([[:space:]]--all([[:space:]]|$)|[[:space:]]-[a-ln-zA-LN-Z]*a)'; then
  block "git commit -a / --all は禁止です。git add -- <path> で明示的にステージしてからコミットしてください"
fi

# 3. force push（--force-with-lease は許可）
if fhas 'git[[:space:]]+push[^&|;]*(--force([[:space:]]|$)|[[:space:]]-f([[:space:]]|$))' \
   && ! fhas '--force-with-lease'; then
  block "git push --force / -f は禁止です。必要なら --force-with-lease を使い、ユーザー承認を得てください"
fi

# 4. squash merge（merge commit 方式を使う）
if fhas '(gh[[:space:]]+pr[[:space:]]+merge[^&|;]*--squash|git[[:space:]]+merge[^&|;]*--squash)'; then
  block "squash merge は禁止です。merge commit 方式（gh pr merge --merge）を使ってください"
fi

# 実際のコマンド区間にある、クォートされた対象引数だけを検査する。
# Python がない場合や解釈できない入力は、設計方針どおり fail-open にする。
guard_match() {
  command -v python3 >/dev/null 2>&1 || return 1

  python3 - "$1" "$cmd" <<'PY' 2>/dev/null
import re
import sys

mode, command = sys.argv[1], sys.argv[2]


def strip_heredoc_bodies(text):
    lines = text.splitlines(keepends=True)
    kept = []
    pending = []
    heredoc = re.compile(r"<<(-?)[ \t]*(?:'([^'\n]+)'|\"([^\"\n]+)\"|([A-Za-z_][A-Za-z0-9_]*))")
    i = 0
    while i < len(lines):
        if pending:
            delimiter, strip_tabs = pending[0]
            candidate = lines[i].rstrip("\r\n")
            if strip_tabs:
                candidate = candidate.lstrip("\t")
            if candidate == delimiter:
                pending.pop(0)
            kept.append("\n" if lines[i].endswith("\n") else "")
            i += 1
            continue

        line = lines[i]
        kept.append(line)
        for match in heredoc.finditer(line):
            delimiter = next(group for group in match.groups()[1:] if group is not None)
            pending.append((delimiter, match.group(1) == "-"))
        i += 1
    return "".join(kept)


def split_segments(text):
    segments = []
    current = []
    quote = None
    escaped = False
    comment = False
    for char in text:
        if comment:
            if char == "\n":
                comment = False
                if current:
                    segments.append("".join(current))
                    current = []
            continue
        if escaped:
            current.append(char)
            escaped = False
            continue
        if quote:
            current.append(char)
            if char == "\\" and quote == '"':
                escaped = True
            elif char == quote:
                quote = None
            continue
        if char in "'\"":
            quote = char
            current.append(char)
        elif char == "\\":
            current.append(char)
            escaped = True
        elif char == "#" and (not current or current[-1].isspace()):
            comment = True
        elif char in ";|&\n":
            if current:
                segments.append("".join(current))
                current = []
        else:
            current.append(char)
    if quote or escaped:
        return []
    if current:
        segments.append("".join(current))
    return segments


def tokens(segment):
    result = []
    value = []
    quoted_parts = []
    quoted = []
    quote = None
    escaped = False
    active = False

    def finish():
        nonlocal value, quoted_parts, quoted, active
        if active:
            if quoted:
                quoted_parts.append("".join(quoted))
            result.append(("".join(value), quoted_parts))
        value = []
        quoted_parts = []
        quoted = []
        active = False

    for char in segment:
        if escaped:
            value.append(char)
            if quote:
                quoted.append(char)
            escaped = False
            active = True
            continue
        if quote:
            if char == "\\" and quote == '"':
                escaped = True
            elif char == quote:
                quoted_parts.append("".join(quoted))
                quoted = []
                quote = None
            else:
                value.append(char)
                quoted.append(char)
            active = True
            continue
        if char in "'\"":
            quote = char
            active = True
        elif char == "\\":
            escaped = True
            active = True
        elif char.isspace():
            finish()
        else:
            value.append(char)
            active = True
    if quote or escaped:
        return []
    finish()
    return result


def quoted_values(argv, options):
    for index, (value, parts) in enumerate(argv):
        for option in options:
            if value == option and index + 1 < len(argv):
                yield from argv[index + 1][1]
            elif value.startswith(option + "="):
                yield from parts
            elif option in ("-m", "-F") and value.startswith(option) and value != option:
                yield from parts


def option_values(argv, options):
    for index, (value, _) in enumerate(argv):
        for option in options:
            if value == option and index + 1 < len(argv):
                yield argv[index + 1][0]
            elif value.startswith(option + "="):
                yield value[len(option) + 1:]
            elif option == "-F" and value.startswith(option) and value != option:
                yield value[len(option):]


def option_present(argv, options):
    for value, _ in argv:
        for option in options:
            if value == option or value.startswith(option + "="):
                return True
    return False


try:
    for segment in split_segments(strip_heredoc_bodies(command)):
        argv = tokens(segment)
        words = [item[0] for item in argv]
        if mode == "commit-trailer" and words[:2] == ["git", "commit"]:
            values = quoted_values(argv[2:], ("-m", "--message", "-F", "--file"))
            if any(re.search(r"Co-Authored-By", value, re.I) for value in values):
                sys.exit(0)
            for path in option_values(argv[2:], ("-F", "--file")):
                if path == "-":
                    continue
                try:
                    with open(path, "rb") as message_file:
                        content = message_file.read(1024 * 1024 + 1)
                    if re.search(br"Co-Authored-By", content, re.I):
                        sys.exit(0)
                except (OSError, ValueError):
                    pass
        elif mode == "pr-trailer" and words[:3] in (["gh", "pr", "create"], ["gh", "pr", "merge"]):
            values = quoted_values(argv[3:], ("--body", "--title", "--subject"))
            if any(re.search(r"Co-Authored-By", value, re.I) for value in values):
                sys.exit(0)
        elif mode == "merge-subject-number" and words[:3] == ["gh", "pr", "merge"]:
            values = quoted_values(argv[3:], ("--subject",))
            if any(re.search(r"#[0-9]+", value) for value in values):
                sys.exit(0)
        elif mode == "merge-missing-fields" and words[:3] == ["gh", "pr", "merge"]:
            has_subject = option_present(argv[3:], ("--subject",))
            has_body = option_present(argv[3:], ("--body", "--body-file"))
            if not has_subject or not has_body:
                sys.exit(0)
        elif mode == "create-missing-body" and words[:3] == ["gh", "pr", "create"]:
            has_fill = option_present(argv[3:], ("--fill", "--fill-first", "--fill-verbose"))
            has_body = option_present(argv[3:], ("--body", "--body-file"))
            if not has_fill and not has_body:
                sys.exit(0)
        elif mode == "create-fill" and words[:3] == ["gh", "pr", "create"]:
            if option_present(argv[3:], ("--fill", "--fill-first", "--fill-verbose")):
                sys.exit(0)
except Exception:
    pass

sys.exit(1)
PY
}

# 5〜10 は git commit / gh pr create / gh pr merge のときだけ詳細パースする。
# guard_match は python3 を起動するため、無関係なコマンドでは走らせない（hook の実行時間対策）。
if printf '%s' "$cmd" | grep -Eq -- '(git[[:space:]]+commit|gh[[:space:]]+pr[[:space:]]+(create|merge))'; then
  # 5. コミットメッセージへの Co-Authored-By 混入
  if guard_match commit-trailer; then
    block "コミットメッセージに Co-Authored-By を含めないでください"
  fi

  # 6. gh pr merge の件名と本文を明示する
  if fhas 'gh[[:space:]]+pr[[:space:]]+merge([[:space:]]|$)' \
     && guard_match merge-missing-fields; then
    block 'gh pr merge には --subject "merge(scope): 説明" と --body "" を明示してください（デフォルトの Merge pull request #N メッセージは禁止）'
  fi

  # 7. gh pr merge の件名に PR / Issue 番号を含めない
  if guard_match merge-subject-number; then
    block "gh pr merge の --subject に #番号を含めないでください"
  fi

  # 8. gh pr create の本文を明示する（--fill 系はチェック 9 だけで扱う）
  if fhas 'gh[[:space:]]+pr[[:space:]]+create([[:space:]]|$)' \
     && guard_match create-missing-body; then
    block 'gh pr create には --body "<本文>" または --body-file <file> で PR 本文を明示してください'
  fi

  # 9. gh pr create --fill 系を使わない
  if fhas 'gh[[:space:]]+pr[[:space:]]+create[^&|;]*--fill(-first|-verbose)?([[:space:]]|$)' \
     && guard_match create-fill; then
    block "gh pr create の --fill / --fill-first / --fill-verbose は使わず、--body または --body-file で本文を明示してください"
  fi

  # 10. PR 本文・タイトル・マージ件名への Co-Authored-By 混入
  if guard_match pr-trailer; then
    block "PR 本文・タイトル・マージ件名に Co-Authored-By を含めないでください"
  fi
fi

exit 0
