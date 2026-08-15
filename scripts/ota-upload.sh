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
# pio's own output is streamed live (via tee) rather than buffered and
# dumped after the fact, and every phase transition (attempt start,
# clean, retry countdown, give-up) gets its own timestamped line - so
# it's always clear what's happening, not just a silent wait.
#
# Usage: ./scripts/ota-upload.sh [-r|--retries N]
#   ./scripts/ota-upload.sh              # up to 5 attempts (default)
#   ./scripts/ota-upload.sh -r 10        # up to 10 attempts
#   ./scripts/ota-upload.sh --retries 1  # a single attempt, no retries

set -uo pipefail
cd "$(dirname "$0")/.."

MAX_ATTEMPTS=5
while [ $# -gt 0 ]; do
  case "$1" in
    -r|--retries)
      if [ $# -lt 2 ]; then
        echo "--retries requires a number (see --help)" >&2
        exit 1
      fi
      MAX_ATTEMPTS="$2"
      shift 2
      ;;
    -h|--help)
      sed -n '2,27p' "$0" | sed 's/^# \{0,1\}//'
      exit 0
      ;;
    *)
      echo "Unknown argument: $1 (see --help)" >&2
      exit 1
      ;;
  esac
done

if ! [[ "$MAX_ATTEMPTS" =~ ^[0-9]+$ ]] || [ "$MAX_ATTEMPTS" -lt 1 ]; then
  echo "--retries must be a positive integer, got: $MAX_ATTEMPTS" >&2
  exit 1
fi

ENV_NAME="esp-wrover-kit-ota"
LOG_FILE="$(mktemp)"
trap 'rm -f "$LOG_FILE"' EXIT

log() {
  echo "[ota-upload $(date '+%H:%M:%S')] $1"
}

for attempt in $(seq 1 "$MAX_ATTEMPTS"); do
  log "=== Attempt $attempt/$MAX_ATTEMPTS: pio run -e $ENV_NAME -t upload ==="
  pio run -e "$ENV_NAME" -t upload 2>&1 | tee "$LOG_FILE"
  status="${PIPESTATUS[0]}"

  if [ "$status" -eq 0 ]; then
    log "=== Succeeded on attempt $attempt ==="
    exit 0
  fi

  log "=== Attempt $attempt failed (pio exit code $status) ==="

  if grep -q "undefined reference" "$LOG_FILE"; then
    log "=== Detected a stale-build-cache link error - cleaning $ENV_NAME before retrying ==="
    pio run -e "$ENV_NAME" -t clean
  elif [ "$attempt" -lt "$MAX_ATTEMPTS" ]; then
    log "=== Retrying in 5s... ==="
    sleep 5
  fi
done

log "=== Gave up after $MAX_ATTEMPTS attempts ==="
exit 1
