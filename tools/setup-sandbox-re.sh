#!/usr/bin/env bash
# REZengineered — sandbox (egress-restricted) RE environment setup.
# Installs into /opt/rea-toolkit (outside the repo/workspace):
#   - Python venv with pefile, capstone, yara-python (via PyPI)
#   - REA CLI (via npm; native backends need Ghidra/Hopper elsewhere)
# Plus system binutils (objdump/readelf/strings) already present.
# NOT buildable in-sandbox (blocked release-asset hosts, no system -dev
# headers, meson wrap downloads): Rizin, radare2, Ghidra, JDK.
# Full environment (Ghidra+JDK) needs unrestricted internet:
#   see tools/setup-re-env.sh (run it on a normal machine).
# Usage: sudo ./tools/setup-sandbox-re.sh   (idempotent)
set -euo pipefail

PREFIX="/opt/rea-toolkit"
TARGET_USER="${SUDO_USER:-$(whoami)}"

[ "$(id -u)" = "0" ] || { echo "run with sudo"; exit 1; }
mkdir -p "$PREFIX"
chown -R "$TARGET_USER:$TARGET_USER" "$PREFIX"
U="sudo -u $TARGET_USER"

echo "==> [1/4] Python venv + PyPI packages (pefile, capstone, yara-python)"
if [ ! -x "$PREFIX/pyenv/bin/python" ]; then
  $U python3 -m venv "$PREFIX/pyenv"
fi
$U "$PREFIX/pyenv/bin/pip" install -q -U pip pefile capstone yara-python
$U "$PREFIX/pyenv/bin/python" -c \
  "import pefile, capstone, yara; print('pefile', pefile.__version__, '| capstone', capstone.__version__, '| yara OK')"

echo "==> [2/4] REA CLI (npm)"
npm install -g "rea-agents@latest" 2>&1 | tail -2
($U rea --version 2>&1 | head -3) || true

echo "==> [3/4] env file"
cat > "$PREFIX/env.sh" <<EOF
# source this: source /opt/rea-toolkit/env.sh
export RE_PREFIX="$PREFIX"
export PATH="$PREFIX/pyenv/bin:\$PATH"
# Full env (machines with unrestricted internet): tools/setup-re-env.sh adds
#   JAVA_HOME, GHIDRA_HOME
EOF
chown "$TARGET_USER:$TARGET_USER" "$PREFIX/env.sh"

echo "==> [4/4] self-test: capstone x86 disasm + binutils presence"
# shellcheck disable=SC1091
. "$PREFIX/env.sh"
sudo -u "$TARGET_USER" "$PREFIX/pyenv/bin/python" - <<'EOF'
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
code = b"\x55\x8b\xec\x83\xec\x10\xb8\x01\x00\x00\x00\x5d\xc3"  # push ebp; mov ebp,esp; ...
for i in Cs(CS_ARCH_X86, CS_MODE_32).disasm(code, 0x401000):
    print(f"  0x{i.address:x}  {i.mnemonic:8s} {i.op_str}")
print("capstone self-test OK")
EOF
command -v objdump readelf strings rea
echo "==> DONE. Use: source $PREFIX/env.sh"
