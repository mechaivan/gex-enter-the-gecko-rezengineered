#!/usr/bin/env bash
# REZengineered — reproducible RE environment setup (Ghidra + JDK + REA CLI).
# Installs OUTSIDE the git workspace (default /opt/rea-toolkit) so bulky
# binaries never pollute the repo or the Arena patchset. Re-run to restore.
# Usage: sudo ./tools/setup-re-env.sh [PREFIX]
set -euo pipefail

PREFIX="${1:-/opt/rea-toolkit}"
GHIDRA_TAG="${GHIDRA_TAG:-Ghidra_12.1.4_build}"
JDK_MAJOR="${JDK_MAJOR:-21}"

echo "==> PREFIX=$PREFIX  GHIDRA=$GHIDRA_TAG  JDK=$JDK_MAJOR"
mkdir -p "$PREFIX"
cd "$PREFIX"

need() { command -v "$1" >/dev/null || { echo "missing: $1"; exit 1; }; }
need curl; need unzip; need tar; need python3

# --- 1. Eclipse Temurin JDK (from GitHub releases, Adoptium binaries repo) ---
echo "==> Resolving Temurin $JDK_MAJOR (linux x64 hotspot jdk)..."
JDK_URL=$(curl -sf "https://api.github.com/repos/adoptium/temurin${JDK_MAJOR}-binaries/releases/latest" \
  | python3 -c "
import json,sys
d=json.load(sys.stdin)
for a in d.get('assets',[]):
    n=a['name']
    if n.startswith('OpenJDK${JDK_MAJOR}U-jdk_x64_linux_hotspot_') and n.endswith('.tar.gz'):
        print(a['browser_download_url']); break
")
[ -n "$JDK_URL" ] || { echo "JDK asset not found"; exit 1; }
echo "==> JDK: $JDK_URL"
curl -fSL --retry 3 -o jdk.tar.gz "$JDK_URL"
rm -rf jdk && mkdir jdk
tar -xzf jdk.tar.gz -C jdk --strip-components=1
rm -f jdk.tar.gz
export JAVA_HOME="$PREFIX/jdk"
"$JAVA_HOME/bin/java" -version

# --- 2. Ghidra (from GitHub releases) ---
echo "==> Downloading Ghidra $GHIDRA_TAG ..."
GHIDRA_ZIP="ghidra_$(echo "$GHIDRA_TAG" | sed 's/Ghidra_//;s/_build//')_PUBLIC_*.zip"
# Resolve exact asset name via API
GHIDRA_URL=$(curl -sf "https://api.github.com/repos/NationalSecurityAgency/ghidra/releases/tags/$GHIDRA_TAG" \
  | python3 -c "
import json,sys,fnmatch
d=json.load(sys.stdin)
for a in d.get('assets',[]):
    if fnmatch.fnmatch(a['name'],'ghidra_*_PUBLIC_*.zip'):
        print(a['browser_download_url']); break
")
[ -n "$GHIDRA_URL" ] || { echo "Ghidra asset not found"; exit 1; }
echo "==> Ghidra: $GHIDRA_URL"
curl -fSL --retry 3 -o ghidra.zip "$GHIDRA_URL"
rm -rf ghidra && mkdir ghidra
unzip -q ghidra.zip -d ghidra-tmp
mv ghidra-tmp/ghidra_* ghidra/app
rm -rf ghidra-tmp ghidra.zip
export GHIDRA_HOME="$PREFIX/ghidra/app"
echo "==> Ghidra at $GHIDRA_HOME"

# --- 3. env file ---
cat > "$PREFIX/env.sh" <<EOF
# source this: . /opt/rea-toolkit/env.sh
export JAVA_HOME="$PREFIX/jdk"
export GHIDRA_HOME="$PREFIX/ghidra/app"
export PATH="\$JAVA_HOME/bin:\$GHIDRA_HOME:\$PATH"
EOF
echo "==> Wrote $PREFIX/env.sh"

# --- 4. Verify headless analyzer ---
# shellcheck disable=SC1091
. "$PREFIX/env.sh"
analyzeHeadless 2>&1 | head -5 || true

# --- 5. REA CLI (npm, global) ---
echo "==> Installing rea-agents CLI ..."
npm install -g "rea-agents@latest" 2>&1 | tail -3
command -v rea && rea --version 2>&1 | head -3 || npx --yes rea-agents@latest --version | head -3

echo "==> DONE. Use: source $PREFIX/env.sh"
