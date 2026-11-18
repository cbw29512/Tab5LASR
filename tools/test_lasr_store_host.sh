#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC=${CC:-gcc}
"$CC" -D_POSIX_C_SOURCE=200809L -std=c11 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer -g \
  -I"$ROOT/onboard/main" \
  "$ROOT/onboard/main/lasr_session_io.c" \
  "$ROOT/onboard/main/lasr_session_metadata.c" \
  "$ROOT/onboard/main/lasr_session_store.c" \
  "$ROOT/onboard/main/lasr_session_finish.c" \
  "$ROOT/onboard/tests/test_lasr_session_store.c" \
  -lm -o /tmp/lasr_session_tests
ASAN_OPTIONS=detect_leaks=1 /tmp/lasr_session_tests