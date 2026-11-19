#!/usr/bin/env bash
# mayhem/test.sh — run far2l's own headless unit test, built by mayhem/build.sh into
# mayhem/build-tests with normal flags (asserts on). Do not compile here.
#
# transformer_config_test (plugins/transformer/tests/ConfigTest.cpp): loads the shipped
# plugins/transformer/configs/plug/formats presets and a known custom INI, asserting the parsed
# groups, item names, commands and output modes, and mask matching. ctest runs it as upstream
# registers it (add_test in plugins/transformer/CMakeLists.txt). Its checks are assert()s; the
# mayhem/tests/config_test_driver.cpp wrapper prints a pass line only once they all held.
#
# Not run: upstream's testing/ suite (Go runner + JS scripts driving an interactive far2l built
# with TESTING=yes; needs the whole TUI) and plugins/jumpword/tests (it tests private copies of
# the matching functions, not plugins/jumpword/src, and marks some cases as expected failures).
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

B="$SRC/mayhem/build-tests"
if [ ! -x "$B/transformer_config_test" ]; then
  echo "test.sh: $B/transformer_config_test missing — mayhem/build.sh did not build the tests" >&2
  emit_ctrf cmake-ctest 0 1
  exit 1
fi

EXPECTED=1
# ctest -V shows the test's stdout: count a test as passed only if ctest says Passed AND the
# driver printed its line (printed only after every upstream assert held).
log=$(cd "$B" && ctest -V --output-on-failure 2>&1); rc=$?
echo "$log"
passed=0
if [ "$rc" -eq 0 ] && grep -qE 'Test +#[0-9]+: transformer_config_test .* Passed' <<<"$log" \
   && grep -q 'transformer_config_test: all assertions passed' <<<"$log"; then
  passed=1
fi
failed=$(( EXPECTED - passed ))
emit_ctrf cmake-ctest "$passed" "$failed"
