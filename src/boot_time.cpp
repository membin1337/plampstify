#include "boot_time.h"

#include <time.h>

void initBootTime() {
  // UTC, no DST - firmware reports timestamps in UTC and lets the client
  // convert to local time (same approach as every TIMESTAMPTZ column in
  // plampControlCenter's Postgres schema), rather than hardcoding this
  // device's own timezone. Safe to call before WiFi is up - configTime()
  // just queues the request and SNTP retries in the background once a
  // connection exists.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
}

String getBootTimeIso() {
  time_t now;
  time(&now);
  // Before NTP has actually synced, time() returns a small epoch value
  // (essentially 0, i.e. 1970) - treat anything before 2020 as "not
  // synced yet" rather than reporting a nonsense boot time.
  if (now < 1577836800) return ""; // 2020-01-01T00:00:00Z

  time_t bootTime = now - (millis() / 1000);
  struct tm timeinfo;
  gmtime_r(&bootTime, &timeinfo);
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
  return String(buf);
}
