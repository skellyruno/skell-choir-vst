#!/usr/bin/env bash
# Renders the listening pack: your vocal through every feature, as WAV files, plus a README saying what to listen for.
#   scripts/build_listening_pack.sh                          the test excerpts, into ./listening-pack
#   scripts/build_listening_pack.sh my-vocal.wav             your vocal for everything
#   scripts/build_listening_pack.sh my-vocal.wav my-words.wav out-folder
#                                    (the second file, with clear s/t/k sounds and some dynamics, is used for the consonant, swell and pulse demos)
# Needs only a C++17 compiler. Run it from the project folder (the built-in rooms are read from Assets/Rooms).
set -eu
cd "$(dirname "$0")/.."
CXX=${CXX:-$(command -v c++ || command -v g++ || command -v clang++ || true)}
[ -n "$CXX" ] || { echo "no C++ compiler found"; exit 2; }
mkdir -p build-native
$CXX -std=c++17 -O2 -pthread -ISource -o build-native/render_listening_pack tools/render_listening_pack.cpp \
    Source/Offline/FullTakeScan.cpp Source/StagedChoir.cpp Source/ChoirFX.cpp Source/FormantChoir.cpp Source/ChoirDSP.cpp
FIRST=${1:-tests/data/vocal_raw_9s.wav}
if [ $# -ge 3 ]; then build-native/render_listening_pack "$FIRST" "$2" "$3"
elif [ $# -eq 2 ]; then build-native/render_listening_pack "$FIRST" "$FIRST" "$2"
else build-native/render_listening_pack "$FIRST" "${2:-tests/data/vocal_raw_hard_9s.wav}" "${3:-listening-pack}"; fi
