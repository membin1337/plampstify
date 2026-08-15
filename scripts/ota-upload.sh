#!/usr/bin/env bash
# Retries the OTA upload (env: esp-wrover-kit-ota) until it succeeds -
# works around two distinct failure classes seen in practice:
#
#   1. A stale/incomplete ESPAsyncWebServer .a archive for this env's
#      build dir - link errors like "undefined reference to
#      AsyncWebServerRequest::getParam(...)" even though the .cpp files
#      themselves compile fine (confirmed via nm on the .o - the symbols
#      are there, they just don't make it into the archived .a). Nothing
#      about the source changes between retries, so PlatformIO's
#      incremental build keeps reusing the same broken archive forever -
#      a plain retry never fixes this, only a clean rebuild does.
#      Detected by grepping the build output for "undefined reference";
#      triggers `pio run -t clean` for this env before the next attempt.
#   2. A genuinely transient OTA-over-WiFi failure (dropped packets, the
#      device mid-poll-cycle and slow to answer the invitation, etc.) -
#      just needs a plain retry, no clean needed.
#
# Usage: ./scripts/ota-upload.sh [max-attempts]
#   ./scripts/ota-upload.sh          # up to 5 attempts (default)
#   ./scripts/ota-upload.sh 10       # up to 10 attempts

set -uo pipefail
cd "$(dirname "$0")/.."

MAX_ATTEMPTS="${1:-5}"
ENV_NAME="esp-wrover-kit-ota"

for attempt in $(seq 1 "$MAX_ATTEMPTS"); do
  echo "=== OTA upload attempt $attempt/$MAX_ATTEMPTS ==="
  output=$(pio run -e "$ENV_NAME" -t upload 2>&1)
  status=$?
  echo "$output"

  if [ "$status" -eq 0 ]; then
    echo "=== OTA upload succeeded on attempt $attempt ==="
    exit 0
  fi

  if echo "$output" | grep -q "undefined reference"; then
    echo "=== Link error detected (stale build cache) - cleaning $ENV_NAME before retrying ==="
    pio run -e "$ENV_NAME" -t clean >/dev/null 2>&1
  elif [ "$attempt" -lt "$MAX_ATTEMPTS" ]; then
    echo "=== Upload failed (attempt $attempt) - retrying in 5s ==="
    sleep 5
  fi
done

echo "=== Gave up after $MAX_ATTEMPTS attempts ==="
exit 1
