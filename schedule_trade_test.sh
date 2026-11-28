#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCHEDULE_SCRIPT="${SCRIPT_DIR}/schedule_trade.sh"

# Verify bash syntax first.
bash -n "$SCHEDULE_SCRIPT"

# Source the scheduling helper functions.
# shellcheck source=schedule_trade.sh
source "$SCHEDULE_SCRIPT"

failures=0

assert_eq() {
  local label="$1"
  local expected="$2"
  local actual="$3"
  if [ "$expected" != "$actual" ]; then
    echo "FAIL [$label]: expected '$expected', got '$actual'" >&2
    failures=$((failures + 1))
  else
    echo "PASS [$label]"
  fi
}

check_case() {
  local name="$1"
  local now_tz="$2"
  local now_str="$3"
  local expected_target_et="$4"
  local expected_refresh_pt="$5"
  local expected_immediate="$6" # "yes" or "no"

  local now_epoch
  now_epoch=$(TZ="$now_tz" date -d "$now_str" +%s)
  local expected_target_epoch
  expected_target_epoch=$(
    TZ="America/New_York" date -d "$expected_target_et" +%s
  )
  local expected_refresh_epoch
  expected_refresh_epoch=$(
    TZ="America/Los_Angeles" date -d "$expected_refresh_pt" +%s
  )

  local actual_target
  actual_target=$(get_next_start_time "$now_epoch")
  assert_eq "${name} (target)" "$expected_target_epoch" "$actual_target"

  local actual_refresh
  actual_refresh=$(get_auth_refresh_time "$actual_target")
  assert_eq "${name} (auth_refresh)" "$expected_refresh_epoch" "$actual_refresh"

  local actual_immediate="no"
  if should_run_auth_refresh_immediately "$now_epoch" "$actual_refresh"; then
    actual_immediate="yes"
  fi
  assert_eq "${name} (immediate)" "$expected_immediate" "$actual_immediate"
}

# Weekday afternoon before 8pm PT -> wait until 8pm PT tonight.
check_case \
  "Tue 2pm PT" \
  "America/Los_Angeles" "2026-11-24 14:00" \
  "2026-11-25 09:00" "2026-11-24 20:00" "no"

# Weekday at exact 8pm PT -> immediate.
check_case \
  "Tue 8pm PT exact" \
  "America/Los_Angeles" "2026-11-24 20:00" \
  "2026-11-25 09:00" "2026-11-24 20:00" "yes"

# Weekday at 10pm PT (after 8pm PT, next calendar day in ET) -> immediate.
check_case \
  "Tue 10pm PT" \
  "America/Los_Angeles" "2026-11-24 22:00" \
  "2026-11-25 09:00" "2026-11-24 20:00" "yes"

# Weekday early morning before 6am PT (9am ET) -> immediate.
check_case \
  "Wed 5am PT" \
  "America/Los_Angeles" "2026-11-25 05:00" \
  "2026-11-25 09:00" "2026-11-24 20:00" "yes"

# Friday afternoon -> target Monday 9am ET, auth_refresh Sunday 8pm PT.
check_case \
  "Fri 3pm PT" \
  "America/Los_Angeles" "2026-11-27 15:00" \
  "2026-11-30 09:00" "2026-11-29 20:00" "no"

# Saturday midday -> target Monday 9am ET, auth_refresh Sunday 8pm PT.
check_case \
  "Sat 12pm PT" \
  "America/Los_Angeles" "2026-11-28 12:00" \
  "2026-11-30 09:00" "2026-11-29 20:00" "no"

# Sunday afternoon -> target Monday 9am ET, auth_refresh Sunday 8pm PT.
check_case \
  "Sun 3pm PT" \
  "America/Los_Angeles" "2026-11-29 15:00" \
  "2026-11-30 09:00" "2026-11-29 20:00" "no"

# Sunday 10pm PT -> target Monday 9am ET, auth_refresh Sunday 8pm PT -> immediate.
check_case \
  "Sun 10pm PT" \
  "America/Los_Angeles" "2026-11-29 22:00" \
  "2026-11-30 09:00" "2026-11-29 20:00" "yes"

# DST spring-forward weekend (Mar 8 2026).
check_case \
  "DST spring-forward Sat" \
  "America/Los_Angeles" "2026-03-07 12:00" \
  "2026-03-09 09:00" "2026-03-08 20:00" "no"

# DST fall-back weekend (Nov 1 2026).
check_case \
  "DST fall-back Sat" \
  "America/Los_Angeles" "2026-10-31 12:00" \
  "2026-11-02 09:00" "2026-11-01 20:00" "no"

# Year boundary (Dec 31 2026).
check_case \
  "Year boundary 2pm PT" \
  "America/Los_Angeles" "2026-12-31 14:00" \
  "2027-01-01 09:00" "2026-12-31 20:00" "no"

check_case \
  "Year boundary 10pm PT" \
  "America/Los_Angeles" "2026-12-31 22:00" \
  "2027-01-01 09:00" "2026-12-31 20:00" "yes"

if [ "$failures" -ne 0 ]; then
  echo "$failures test(s) failed." >&2
  exit 1
fi

echo "All schedule_trade tests passed."
