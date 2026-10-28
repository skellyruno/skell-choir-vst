#!/usr/bin/env bash
# Builds and runs every JUCE-free test with a plain C++17 compiler. No CMake, no JUCE, no internet.
#   scripts/run_tests.sh              all tests
#   scripts/run_tests.sh balance      only tests whose name contains "balance"
#   SANITIZE=1 scripts/run_tests.sh   AddressSanitizer + UndefinedBehaviorSanitizer (slower)
#   CXX=clang++ scripts/run_tests.sh  pick the compiler (default: c++, g++ or clang++, whichever exists)
# The same tests run under CTest (cmake -DOMNICHOIR_NATIVE_ONLY=ON, then ctest); this script is for machines without CMake.
set -u
export PYTHONDONTWRITEBYTECODE=1
cd "$(dirname "$0")/.."
ROOT=$PWD
CXX=${CXX:-$(command -v c++ || command -v g++ || command -v clang++ || true)}
[ -n "$CXX" ] || { echo "no C++ compiler found (install g++ or clang++)"; exit 2; }
OUT=${OUT:-build-native}; mkdir -p "$OUT"
FLAGS="-std=c++17 -O2 -Wall -Wextra -ISource"
[ "${SANITIZE:-0}" = 1 ] && FLAGS="-std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -ISource"
ENGINE="Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp"
STAGED="Source/StagedChoir.cpp $ENGINE"

# name | sources | arguments given to the test
TESTS=(
  "dsp_tests|tests/dsp_tests.cpp Source/ChoirDSP.cpp|"
  "fx_tests|tests/fx_tests.cpp $ENGINE|"
  "tracker_tests|tests/tracker_tests.cpp Source/Offline/FullTakeScan.cpp $ENGINE|"
  "staged_tests|tests/staged_tests.cpp $STAGED|"
  "formant_tests|tests/formant_tests.cpp $ENGINE|"
  "convolver_tests|tests/convolver_tests.cpp|"
  "features_tests|tests/features_tests.cpp $STAGED|"
  "preset_tests|tests/preset_tests.cpp $STAGED|"
  "stem_mixer_tests|tests/stem_mixer_tests.cpp Source/Offline/ExternalStemMixer.cpp|"
  "fulltake_tests|tests/fulltake_tests.cpp Source/Offline/FullTakeScan.cpp $ENGINE|"
  "balance_tests|tests/balance_tests.cpp $STAGED|$ROOT"
  "real_vocal_tests|tests/real_vocal_tests.cpp $STAGED|$ROOT/tests/data"
)

filter=${1:-}; pass=0; fail=0; failed=()
for entry in "${TESTS[@]}"; do
  IFS='|' read -r name sources args <<< "$entry"
  [[ -n "$filter" && "$name" != *"$filter"* ]] && continue
  printf '%-18s ' "$name"
  if ! $CXX $FLAGS -o "$OUT/$name" $sources 2> "$OUT/$name.build.log"; then
    echo "BUILD FAILED  (see $OUT/$name.build.log)"; fail=$((fail+1)); failed+=("$name"); continue
  fi
  if timeout 900 "$OUT/$name" $args > "$OUT/$name.log" 2>&1; then echo "ok   $(tail -n1 "$OUT/$name.log")"; pass=$((pass+1))
  else echo "FAILED (see $OUT/$name.log)"; tail -n 4 "$OUT/$name.log" | sed 's/^/                     /'; fail=$((fail+1)); failed+=("$name"); fi
done
if [[ -z "$filter" ]] && command -v python3 > /dev/null; then
  printf '%-18s ' "python_interop"
  if python3 -m unittest discover -s tests -p test_interop_tools.py > "$OUT/python_interop.log" 2>&1; then echo "ok   $(tail -n1 "$OUT/python_interop.log")"; pass=$((pass+1))
  else echo "FAILED (see $OUT/python_interop.log)"; fail=$((fail+1)); failed+=(python_interop); fi
fi
if [[ -z "$filter" ]]; then
  printf '%-18s ' "ui_syntax"
  if bash scripts/check_ui_syntax.sh > "$OUT/ui_syntax.log" 2>&1; then echo "ok   plugin glue type-checks against the JUCE stub, UI strings are ASCII"; pass=$((pass+1))
  else echo "FAILED (see $OUT/ui_syntax.log)"; tail -n 6 "$OUT/ui_syntax.log" | sed 's/^/                     /'; fail=$((fail+1)); failed+=(ui_syntax); fi
fi
echo; echo "$pass passed, $fail failed${failed:+: ${failed[*]}}"
[ "$fail" -eq 0 ]
