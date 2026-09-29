#!/usr/bin/env bash
# macOS 用インストーラー（.pkg）作成スクリプト。VST3 と AU を 1 つのパッケージで /Library/Audio/Plug-Ins/ へ入れる。
#
# 使い方:
#   ./build_installer_mac.sh   # ビルドしてから build_mac/installer/Atafuta09Leveler-<version>-mac.pkg を作成
#
# 注: Developer ID で署名・公証していないため、配布先では初回に Gatekeeper の許可が必要（MANUAL.md 参照）。
set -euo pipefail

cd "$(dirname "$0")"

./build_plugin_mac.sh

VERSION="$(sed -nE 's/^project\(Atafuta09Leveler VERSION ([0-9.]+).*/\1/p' CMakeLists.txt)"
BUILD_DIR=build_mac
command -v ninja >/dev/null 2>&1 && BUILD_DIR=build_mac_ninja
ARTEFACTS="${BUILD_DIR}/AutoLeveler_artefacts/Release"
WORK="${BUILD_DIR}/installer"
BUNDLE_ID=com.atafuta09.atafuta09leveler
OUT="${WORK}/Atafuta09Leveler-${VERSION}-mac.pkg"

rm -rf "${WORK}"
mkdir -p "${WORK}/root_vst3" "${WORK}/root_au" "${WORK}/scripts_au"
ditto "${ARTEFACTS}/VST3/Atafuta09Leveler.vst3" "${WORK}/root_vst3/Atafuta09Leveler.vst3"
ditto "${ARTEFACTS}/AU/Atafuta09Leveler.component" "${WORK}/root_au/Atafuta09Leveler.component"
# quarantine などの拡張属性を配布物に持ち込まない（com.apple.provenance は OS 保護のため残るが無害）
xattr -cr "${WORK}/root_vst3" "${WORK}/root_au" 2>/dev/null || true

# AU をすぐ DAW に認識させるため、AU キャッシュを持つプロセスを再起動する
cat > "${WORK}/scripts_au/postinstall" <<'EOF'
#!/bin/sh
killall -9 AudioComponentRegistrar 2>/dev/null
exit 0
EOF
chmod +x "${WORK}/scripts_au/postinstall"

# pkgbuild <名前> <インストール先> [追加引数...]
component_pkg() {
    local name=$1 location=$2
    shift 2
    local plist="${WORK}/${name}.plist"
    pkgbuild --analyze --root "${WORK}/root_${name}" "${plist}" >/dev/null
    # 既存の同名バンドルが別の場所にあっても、そちらを上書きせず必ず指定先へ入れる
    plutil -replace 0.BundleIsRelocatable -bool NO "${plist}"
    pkgbuild --root "${WORK}/root_${name}" \
        --component-plist "${plist}" \
        --identifier "${BUNDLE_ID}.${name}" \
        --version "${VERSION}" \
        --install-location "${location}" \
        "$@" \
        "${WORK}/${name}.pkg"
}

echo "=== Building installer ==="
component_pkg vst3 "/Library/Audio/Plug-Ins/VST3"
component_pkg au   "/Library/Audio/Plug-Ins/Components" --scripts "${WORK}/scripts_au"

productbuild --identifier "${BUNDLE_ID}.installer" --version "${VERSION}" \
    --package "${WORK}/vst3.pkg" --package "${WORK}/au.pkg" "${OUT}"

echo
echo "=== Installer finished ==="
echo "${OUT}"
