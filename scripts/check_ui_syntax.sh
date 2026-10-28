#!/usr/bin/env bash
# Type-checks the JUCE-dependent plugin files against tests/juce_stub (not real JUCE) and checks that UI strings are plain ASCII.
# Fast, needs only a C++17 compiler. The authoritative check is the CI plugin job (real JUCE + pluginval).
set -u
cd "$(dirname "$0")/.."
CXX=${CXX:-$(command -v c++ || command -v g++ || command -v clang++ || true)}
[ -n "$CXX" ] || { echo "no C++ compiler found"; exit 2; }
fail=0

# juce::String asserts on non-ASCII char literals, and a stray curly quote or middle dot shows up as garbage in the UI
if command -v python3 > /dev/null; then
  if ! python3 - <<'PY'
import glob,sys
bad=[]
for f in glob.glob('Source/Plugin*.cpp')+glob.glob('Source/Plugin*.h')+glob.glob('Source/UI/*.h')+glob.glob('Source/Offline/TakeWorker.*'):
    for n,line in enumerate(open(f,encoding='utf-8').read().splitlines(),1):
        if any(ord(c)>127 for c in line): bad.append(f"{f}:{n}: {line.strip()[:70]}")
if bad: print("non-ASCII characters in UI source (use plain ASCII, or juce::CharPointer_UTF8):"); print("\n".join(bad)); sys.exit(1)
PY
  then fail=1; else echo "ASCII-only UI strings   ok"; fi
fi

for f in Source/PluginEditor.cpp Source/PluginProcessor.cpp; do
  printf '%-30s ' "$f"
  out=$($CXX -std=c++17 -fsyntax-only -isystem tests/juce_stub -ISource "$f" 2>&1)
  if echo "$out" | grep -q "error"; then echo "ERRORS"; echo "$out" | grep -E "error" | head -15 | sed 's/^/    /'; fail=1; else echo "ok (against the stub)"; fi
done
exit $fail
