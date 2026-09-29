#!/usr/bin/env bash
# macOS 用ビルドスクリプト（VST3 + Audio Unit、Apple Silicon / Intel ユニバーサル）
#
# 使い方:
#   ./build_plugin_mac.sh            # ビルドのみ（成果物は build_mac/AutoLeveler_artefacts/Release/、Ninja 使用時は build_mac_ninja/）
#   ./build_plugin_mac.sh --install  # ビルド後に ~/Library/Audio/Plug-Ins/ へコピー
#
# 必要なもの: Xcode（Command Line Tools 含む）、CMake 3.22 以上
set -euo pipefail

cd "$(dirname "$0")"

usage() {
    echo "使い方: $0 [--install]" >&2
}

INSTALL=FALSE
case "${1:-}" in
    "") ;;
    --install) INSTALL=TRUE ;;
    -h|--help) usage; exit 0 ;;
    *) usage; exit 1 ;;
esac

# 生成器ごとにビルドディレクトリを分ける（同じ場所だと CMake が generator 不一致で失敗する）
BUILD_DIR=build_mac
GENERATOR=Xcode
if command -v ninja >/dev/null 2>&1; then
    GENERATOR=Ninja
    BUILD_DIR=build_mac_ninja
fi

echo "=== Configuring CMake (${GENERATOR}) ==="
cmake -B "${BUILD_DIR}" -G "${GENERATOR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCOPY_PLUGIN_AFTER_BUILD="${INSTALL}"

echo "=== Building AutoLeveler_VST3 / AutoLeveler_AU ==="
cmake --build "${BUILD_DIR}" --config Release --target AutoLeveler_VST3 AutoLeveler_AU --parallel

ARTEFACTS="${BUILD_DIR}/AutoLeveler_artefacts/Release"
echo
echo "=== Build finished ==="
echo "VST3: ${ARTEFACTS}/VST3/Atafuta09Leveler.vst3"
echo "AU  : ${ARTEFACTS}/AU/Atafuta09Leveler.component"
if [[ "${INSTALL}" == "TRUE" ]]; then
    echo "Installed to ~/Library/Audio/Plug-Ins/VST3 and ~/Library/Audio/Plug-Ins/Components"
    echo "AU が DAW に表示されない場合は 'killall -9 AudioComponentRegistrar' を実行してから DAW を再起動してください。"
fi
