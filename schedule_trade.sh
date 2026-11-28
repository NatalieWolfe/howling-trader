#!/bin/bash
set -e

# ANSI escape codes for colorized output.
COLOR_RESET="\033[0m"
BOLD="\033[1m"
DIM="\033[2m"
HIGHLIGHT="\033[36m"

# Returns the target epoch time for 9:00 AM ET (30 mins before opening).
# Accepts an optional reference epoch timestamp (defaults to current time).
get_next_start_time() {
  # Perform calculations in NY time.
  local ny_tz="America/New_York"
  local now_epoch="${1:-$(date +%s)}"

  local current_date
  current_date=$(TZ="$ny_tz" date -d "@$now_epoch" +%Y-%m-%d)
  local current_hour
  current_hour=$(TZ="$ny_tz" date -d "@$now_epoch" +%H)
  local current_min
  current_min=$(TZ="$ny_tz" date -d "@$now_epoch" +%M)
  local current_wday
  current_wday=$(TZ="$ny_tz" date -d "@$now_epoch" +%u) # 1=Mon, 7=Sun

  # Target is 09:00 AM ET.
  local target_time="09:00"
  local days_ahead=0

  if [ "$current_wday" -eq 6 ]; then
    # Saturday -> Monday (+2 days).
    days_ahead=2
  elif [ "$current_wday" -eq 7 ]; then
    # Sunday -> Monday (+1 day).
    days_ahead=1
  elif [ "$current_hour" -gt 9 ] || \
    ([ "$current_hour" -eq 9 ] && [ "$current_min" -ge 0 ]); then
    # It is currently at or after 09:00 AM ET.
    if [ "$current_wday" -eq 5 ]; then
      # Friday -> Next Monday (+3 days).
      days_ahead=3
    else
      # Mon-Thu -> Tomorrow (+1 day).
      days_ahead=1
    fi
  else
    # It is currently before 09:00 AM ET on a weekday -> Today.
    days_ahead=0
  fi

  local target_date
  target_date=$(TZ="$ny_tz" date -d "$current_date +$days_ahead days" +%Y-%m-%d)
  TZ="$ny_tz" date -d "$target_date $target_time" +%s
}

# Returns the auth_refresh epoch time: 8:00 PM PT the night before the
# scheduled trading run.
get_auth_refresh_time() {
  local pt_tz="America/Los_Angeles"
  local target_epoch="$1"

  local target_date_pt
  target_date_pt=$(TZ="$pt_tz" date -d "@$target_epoch" +%Y-%m-%d)
  local prev_date_pt
  prev_date_pt=$(TZ="$pt_tz" date -d "$target_date_pt -1 day" +%Y-%m-%d)
  TZ="$pt_tz" date -d "$prev_date_pt 20:00" +%s
}

# Returns 0 if auth_refresh should execute immediately (current time is at or
# past the scheduled 8:00 PM PT auth_refresh time), 1 otherwise.
should_run_auth_refresh_immediately() {
  local now_epoch="$1"
  local auth_refresh_epoch="$2"
  [ "$now_epoch" -ge "$auth_refresh_epoch" ]
}

# Sets an RTC wake alarm (when possible) and waits until target_epoch.
wait_until() {
  local target_epoch="$1"
  local label="$2"
  local allow_sudo_prompt="${3:-true}"

  local now_epoch
  now_epoch=$(date +%s)
  if [ "$now_epoch" -ge "$target_epoch" ]; then
    return 0
  fi

  local rtc_flag="-u"
  if timedatectl status | grep -q "RTC in local TZ: yes"; then
    rtc_flag="-l"
  fi

  if [ "$allow_sudo_prompt" = "true" ]; then
    echo "Setting RTC wake alarm for ${label} (requires sudo)..."
    sudo rtcwake "$rtc_flag" -m no -t "$target_epoch" > /dev/null 2>&1
    echo -e \
      "Hardware wake alarm set." \
      "${BOLD}You can now safely suspend your computer.${COLOR_RESET}"
  else
    if sudo -n rtcwake "$rtc_flag" -m no -t "$target_epoch" > /dev/null 2>&1; then
      echo -e \
        "Hardware wake alarm updated for ${label}." \
        "${BOLD}You can now safely suspend your computer.${COLOR_RESET}"
    else
      echo "Warning: could not update RTC wake alarm for ${label} without" \
        "sudo credentials; keep machine awake until ${label}."
    fi
  fi

  printf "Waiting for %s..." "$label"
  while true; do
    now_epoch=$(date +%s)
    if [ "$now_epoch" -ge "$target_epoch" ]; then
      break
    fi
    local remaining=$((target_epoch - now_epoch))
    local sleep_chunk=60
    if [ "$remaining" -lt "$sleep_chunk" ]; then
      sleep_chunk="$remaining"
    fi
    printf "${DIM}z${COLOR_RESET}"
    sleep "$sleep_chunk"
  done
  printf "\n"
}

# Starts required services and runs auth_refresh synchronously.
run_auth_refresh() {
  echo "Starting infrastructure services for auth refresh..."
  bazel run //infrastructure:services-up
  echo "Running auth refresh..."
  bazel run //infrastructure:auth-refresh
}

print_help() {
  echo "Usage: ./schedule_trade.sh [options]"
  echo "Options:"
  echo "  --stocks=SYMBOL,SYMBOL...  List of stocks to trade"
  echo "                             (default: all from stock.proto)"
  echo "  --account=NAME             Account to use (default: Primary)"
  echo "  --analyzer=NAME            Analyzer to use (default: howling)"
  echo "  --help                     Display this help message"
}

main() {
  # Extract all symbols from data/stock.proto, skipping unspecified and
  # comments.
  local default_stocks
  default_stocks=$(
    grep -E '^[[:space:]]+[A-Z0-9]+[[:space:]]+=[[:space:]]+[0-9]+;' \
      data/stock.proto | grep -v "UNSPECIFIED" | awk '{print $1}' | \
      paste -sd "," -
  )

  local stocks="$default_stocks"
  local analyzer="howling"
  local account="Primary"
  local local_tz
  local_tz=$(timedatectl show --property=Timezone --value 2>/dev/null || \
    timedatectl | grep "Time zone:" | awk '{print $3}')

  # Allow overriding via command line arguments.
  for arg in "$@"; do
    case $arg in
      --stocks=*)
        stocks="${arg#*=}"
        ;;
      --account=*)
        account="${arg#*=}"
        ;;
      --analyzer=*)
        analyzer="${arg#*=}"
        ;;
      --help)
        print_help
        exit 0
        ;;
      *)
        echo "Unknown argument: $arg"
        echo "Use --help for usage information."
        exit 1
        ;;
    esac
  done

  # Calculate time until auth refresh and market open.
  local now_epoch
  now_epoch=$(date +%s)
  local target_epoch
  target_epoch=$(get_next_start_time "$now_epoch")
  local auth_refresh_epoch
  auth_refresh_epoch=$(get_auth_refresh_time "$target_epoch")
  local sleep_seconds=$((target_epoch - now_epoch))
  local target_readable
  target_readable=$(TZ="$local_tz" date -d "@$target_epoch")
  local auth_refresh_readable
  auth_refresh_readable=$(TZ="$local_tz" date -d "@$auth_refresh_epoch")

  echo "=================================================="
  echo "          Trading Scheduler"
  echo "=================================================="
  echo -e " --stocks:   ${HIGHLIGHT}${stocks}${COLOR_RESET}"
  echo -e " --analyzer: ${HIGHLIGHT}${analyzer}${COLOR_RESET}"
  echo -e " --account:  ${HIGHLIGHT}${account}${COLOR_RESET}"
  echo "--------------------------------------------------"
  echo "Current Time:  $(TZ="$local_tz" date -d "@$now_epoch")"
  if should_run_auth_refresh_immediately "$now_epoch" "$auth_refresh_epoch"; then
    echo "Auth Refresh:  Immediate (scheduled $auth_refresh_readable passed)"
  else
    echo "Auth Refresh:  $auth_refresh_readable"
  fi
  echo "Start Time:    $target_readable"
  local hours=$((sleep_seconds / 3600))
  local minutes=$((sleep_seconds % 3600 / 60))
  echo -e "Sleeping for:  ${HIGHLIGHT}${hours}h${minutes}m${COLOR_RESET}"
  echo "=================================================="

  # Coordinate auth_refresh execution: run around 8:00 PM PT the night before
  # the trading run, or immediately if that time has already passed.
  if should_run_auth_refresh_immediately "$now_epoch" "$auth_refresh_epoch"; then
    echo "Auth refresh time (8:00 PM PT) has already passed; running" \
      "auth_refresh immediately..."
    run_auth_refresh
    wait_until "$target_epoch" "start time" "true"
  else
    wait_until "$auth_refresh_epoch" "auth refresh" "true"
    run_auth_refresh
    wait_until "$target_epoch" "start time" "false"
  fi

  # Execute and capture output for post-execution analysis.
  local run_timestamp
  run_timestamp=$(date +%Y%m%d_%H%M%S)
  local raw_log="/tmp/howling_trade_${run_timestamp}.raw.log"
  local exec_log="/tmp/howling_trade_${run_timestamp}.log"

  set +e
  (
    set -e
    echo "Starting infrastructure services..."
    bazel run //infrastructure:services-up

    echo "Building trading bot..."
    bazel build //howling_tools:execute

    echo "Starting trading bot..."
    ./bazel-bin/howling_tools/execute \
      --stocks="$stocks" \
      --analyzer="$analyzer" \
      --account="$account" \
      --headless \
      --database=postgres \
      --bao_token=root \
      --logging_always_flush
  ) 2>&1 | tee "$raw_log"
  local exec_exit_code=${PIPESTATUS[0]}
  set -e

  # Strip intermediate carriage-return redraws from the captured terminal
  # output.
  sed -E $'s/\r+$//; s/.*\r//' "$raw_log" > "$exec_log"
  rm -f "$raw_log"

  echo "=================================================="
  echo "Trading session finished (exit code: $exec_exit_code)."
  echo "Log saved to: $exec_log"
  echo "Launching post-execution analysis..."
  echo "=================================================="

  local agy_init_prompt="The scheduled trading session just finished with exit \
code ${exec_exit_code} (stocks=${stocks}, analyzer=${analyzer}, \
account=${account}).
Read the captured execution output at ${exec_log} and briefly acknowledge that \
you have read it. Do not run any commands, modify any files, create any plans, \
or produce the full report yet — wait for my next message."

  local agy_followup_prompt="Now complete the post-execution analysis based on \
the log you just read:
1. Check the logs of any relevant Docker services (e.g. auth_service / docker \
compose services).
2. Produce a clear summary report of the day's trading activities, metrics, \
and service behavior.
3. If any errors, warnings, or anomalies occurred, investigate the codebase \
and logs to identify the root cause and propose a fix.
4. IMPORTANT: Do not take any actions, modify any files, or restart any \
services. Only perform read-only inspection and produce the report for me to \
read."

  local agy_bin="$HOME/.gemini/bin/agy"
  if [ ! -x "$agy_bin" ]; then
    agy_bin="agy"
  fi
  local project_id="37064d7b-9449-451c-859a-208b6ecc1e80"

  # Preload the execution log in a fresh conversation first (read-only, no shell
  # permissions needed), then resume it interactively with --remote-control so
  # permission prompts (e.g. Docker logs) can be approved remotely and the
  # -i + --remote-control startup race is avoided.
  "$agy_bin" \
    --app_data_dir=antigravity \
    --project="$project_id" \
    --add-dir /tmp \
    --disable-slash-commands \
    -p "$agy_init_prompt" > /dev/null

  "$agy_bin" \
    --app_data_dir=antigravity \
    --project="$project_id" \
    --add-dir /tmp \
    --remote-control \
    -c \
    -i "$agy_followup_prompt"
}

if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
  main "$@"
fi
