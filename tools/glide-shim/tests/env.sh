# env.sh — environment detection for the glide-shim test-suite (sourced).
# Host stages here (c-syntax/behaviour) are LINUX host checks; they NEVER
# validate the Win32 build (that is build-win32.sh: V-1/V-1b). When the
# environment cannot run a stage, callers SKIP loudly instead of PASS/FAIL.

# Prints a working python command name, or nothing (return 1).
# Each candidate must actually RUN (defeats the Microsoft Store alias,
# which exists as python3/python but fails) and be >= 3.8.
find_python() {
  for c in python3 python py; do
    if command -v "$c" >/dev/null 2>&1 \
       && "$c" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
      echo "$c"; return 0
    fi
  done
  return 1
}

# find_objdump <cc>: print a working objdump paired with toolchain <cc>.
# Tries, in order: <cc-dir>/i686-w64-mingw32-objdump, <cc-dir>/objdump,
# then plain objdump in PATH. Each candidate must exist AND run
# (--version rc=0); prints the path (return 0) or nothing (return 1).
# Never presumes the triplet name exists: MSYS2 MINGW32 only guarantees
# plain objdump, so the triplet is preferred only when it runs.
find_objdump() {
  # NOTE: ${_p%/*} instead of $(dirname) — builtins only, so pairing
  # works even with a minimal PATH (and stays hermetic under test).
  _dir=""
  _p=$(command -v "$1" 2>/dev/null) && _dir=${_p%/*}
  if [ -n "$_dir" ]; then
    for _c in "$_dir/i686-w64-mingw32-objdump" "$_dir/objdump"; do
      if [ -x "$_c" ] && "$_c" --version >/dev/null 2>&1; then
        echo "$_c"; return 0
      fi
    done
  fi
  if command -v objdump >/dev/null 2>&1 \
     && objdump --version >/dev/null 2>&1; then
    command -v objdump; return 0
  fi
  return 1
}

# host_run_ok: can this machine compile AND run gshim.c as a host binary?
# Prints the reason when not (return 1). Used to SKIP (never PASS) the
# behaviour stage outside Linux with a native compiler.
host_run_ok() {
  u=$(uname -s 2>/dev/null) || u="unknown"
  if [ "$u" != "Linux" ]; then
    echo "host is $u, not Linux (behaviour is a Linux host stage; Win32 validation is build-win32.sh)"
    return 1
  fi
  if ! command -v gcc >/dev/null 2>&1; then
    echo "no gcc"
    return 1
  fi
  m=$(gcc -dumpmachine 2>/dev/null) || m="unknown"
  case "$m" in
    x86_64-*linux*|i?86-*linux*) return 0 ;;
    *) echo "gcc targets $m (need a native Linux gcc; Win32 validation is build-win32.sh)"; return 1 ;;
  esac
}

# win32_build_ok: is an i686-w64-mingw32 toolchain available to BUILD the
# native Win32 harness? Prints the reason when not (return 1). Build-only:
# running the behaviour phase needs Windows too (see win32_run_ok), so a
# capable-but-not-Windows box still validates compile+exports honestly.
win32_build_ok() {
  cc="${W32CC:-i686-w64-mingw32-gcc}"
  if ! command -v "$cc" >/dev/null 2>&1; then
    echo "no $cc (MSYS2 MINGW32 shell: pacman -S mingw-w64-i686-gcc)"
    return 1
  fi
  m=$("$cc" -dumpmachine 2>/dev/null) || m="unknown"
  if [ "$m" != "i686-w64-mingw32" ]; then
    echo "$cc targets $m (need i686-w64-mingw32)"
    return 1
  fi
  return 0
}

# win32_run_ok: can this machine RUN Win32 binaries (the behaviour phase
# of the native harness)? Prints the reason when not (return 1).
win32_run_ok() {
  u=$(uname -s 2>/dev/null) || u="unknown"
  case "$u" in
    MINGW*|MSYS*|CYGWIN*) return 0 ;;
    *) echo "host is $u (win32 behaviour needs Windows; the build + export checks above still stand)"; return 1 ;;
  esac
}

# classify_csyntax_fail <logfile>: prints SKIP when the build log shows the
# shim's own 32-bit guard tripping (environment), else FAIL (real regression).
classify_csyntax_fail() {
  if grep -q "must be built 32-bit" "$1" 2>/dev/null; then
    echo "SKIP"
  else
    echo "FAIL"
  fi
}
