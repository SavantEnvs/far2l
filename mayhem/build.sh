#!/usr/bin/env bash
# mayhem/build.sh — build far2l's fuzz harnesses (see mayhem/CMakeLists.txt).
# Runs in the commit image as `mayhem` in /mayhem; CC/CXX/SANITIZER_FLAGS/LIB_FUZZING_ENGINE/
# STANDALONE_FUZZ_MAIN come from the base image. Re-runnable and offline: every dependency
# (libarchive) is an apt package installed by mayhem/Dockerfile.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
export CC CXX

cd "$SRC"
# (b) upstream's headless unit test (transformer ConfigTest) with normal flags: no sanitizers, no
# SanCov, no -DNDEBUG (it checks with assert). mayhem/test.sh runs it through ctest.
cmake -S mayhem -B mayhem/build-tests -G Ninja -DMAYHEM_TESTS=ON \
    -DCMAKE_C_FLAGS="-O1 $DEBUG_FLAGS ${COVERAGE_FLAGS:-}" \
    -DCMAKE_CXX_FLAGS="-O1 $DEBUG_FLAGS ${COVERAGE_FLAGS:-}" \
    -DCMAKE_EXE_LINKER_FLAGS="${COVERAGE_FLAGS:-}"
cmake --build mayhem/build-tests -j"$MAYHEM_JOBS"

# SanCov on all project code (not only the harness TUs), unconditionally.
FLAGS="-O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS"

cmake -S mayhem -B mayhem/build -G Ninja \
    -DCMAKE_C_FLAGS="$FLAGS" \
    -DCMAKE_CXX_FLAGS="$FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$SANITIZER_FLAGS" \
    -DFUZZ_ENGINE="$LIB_FUZZING_ENGINE" \
    -DSTANDALONE_MAIN="$STANDALONE_FUZZ_MAIN" \
    -DLSAN_OFF_SRC="$SRC/mayhem/lsan_off.cc" \
    -DOUT_DIR="$SRC/mayhem/out"
cmake --build mayhem/build -j"$MAYHEM_JOBS"

ls -l "$SRC"/mayhem/out/
