#!/bin/bash
set -e

# Extract all symbols from data/stock.proto, skipping unspecified and comments.
DEFAULT_STOCKS=$(grep -E '^[[:space:]]+[A-Z0-9]+[[:space:]]+=[[:space:]]+[0-9]+;' \
  data/stock.proto | grep -v "UNSPECIFIED" | awk '{print $1}' | paste -sd "," -)

STOCKS=$DEFAULT_STOCKS
ANALYZER="howling"
ACCOUNT="Primary"
LOCAL_TZ=$(timedatectl show --property=Timezone --value 2>/dev/null || \
  timedatectl | grep "Time zone:" | awk '{print $3}')

# ANSI escape codes for colorized output
COLOR_RESET="\033[0m"
BOLD="\033[1m"
DIM="\033[2m"
HIGHLIGHT="\033[36m"

# Function to get the target epoch time for 9:00 AM ET (30 mins before opening)
get_next_start_time() {
  # Perform calculations in NY time
  local NY_TZ="America/New_York"

  local current_hour=$(TZ="$NY_TZ" date +%H)
  local current_min=$(TZ="$NY_TZ" date +%M)
  local current_wday=$(TZ="$NY_TZ" date +%u) # 1=Mon, 7=Sun

  # Target is 09:00 AM ET
  local target_time="09:00"

  if [ "$current_wday" -ge 6 ]; then
    # Saturday (6) or Sunday (7) -> Next Monday
    TZ="$NY_TZ" date -d "next monday $target_time" +%s
  elif [ "$current_hour" -gt 9 ] || \
    ([ "$current_hour" -eq 9 ] && [ "$current_min" -ge 0 ]); then
    # It is currently after 09:00 AM ET
    if [ "$current_wday" -eq 5 ]; then
      # Friday -> Next Monday
      TZ="$NY_TZ" date -d "next monday $target_time" +%s
    else
      # Mon-Thu -> Tomorrow
      TZ="$NY_TZ" date -d "tomorrow $target_time" +%s
    fi
  else
    # It is currently before 09:00 AM ET on a weekday -> Today
    TZ="$NY_TZ" date -d "today $target_time" +%s
  fi
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

# Allow overriding via command line arguments
for arg in "$@"; do
  case $arg in
    --stocks=*)
      STOCKS="${arg#*=}"
      ;;
    --account=*)
      ACCOUNT="${arg#*=}"
      ;;
    --analyzer=*)
      ANALYZER="${arg#*=}"
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

# Calculate time until the market opens.
TARGET_EPOCH=$(get_next_start_time)
NOW_EPOCH=$(date +%s)
SLEEP_SECONDS=$((TARGET_EPOCH - NOW_EPOCH))
TARGET_READABLE=$(TZ="$LOCAL_TZ" date -d @$TARGET_EPOCH)

echo "=================================================="
echo "          Trading Scheduler"
echo "=================================================="
echo -e " --stocks:   ${HIGHLIGHT}$STOCKS${COLOR_RESET}"
echo -e " --analyzer: ${HIGHLIGHT}$ANALYZER${COLOR_RESET}"
echo -e " --account:  ${HIGHLIGHT}$ACCOUNT${COLOR_RESET}"
echo "--------------------------------------------------"
echo "Current Time:  $(TZ="$LOCAL_TZ" date)"
echo "Start Time:    $TARGET_READABLE"
HOURS=$(($SLEEP_SECONDS / 3600))
MINUTES=$(($SLEEP_SECONDS % 3600 / 60))
echo -e "Sleeping for:  ${HIGHLIGHT}${HOURS}h${MINUTES}m${COLOR_RESET}"
echo "=================================================="

# Set wake alarm
RTC_FLAG="-u"
if timedatectl status | grep -q "RTC in local TZ: yes"; then
  RTC_FLAG="-l"
fi
echo "Setting RTC wake alarm (requires sudo)..."
sudo rtcwake "$RTC_FLAG" -m no -t "$TARGET_EPOCH" > /dev/null 2>&1
echo -e \
  "Hardware wake alarm set." \
  "${BOLD}You can now safely suspend your computer.${COLOR_RESET}"

# Wait until target time
printf "Waiting for start time..."
while [ $(date +%s) -lt $TARGET_EPOCH ]; do
  printf "${DIM}z${COLOR_RESET}"
  sleep 60
done
printf "\n"

# Execute and capture output for post-execution analysis.
RUN_TIMESTAMP=$(date +%Y%m%d_%H%M%S)
RAW_LOG="/tmp/howling_trade_${RUN_TIMESTAMP}.raw.log"
EXEC_LOG="/tmp/howling_trade_${RUN_TIMESTAMP}.log"

set +e
(
  set -e
  echo "Starting infrastructure services..."
  bazel run //infrastructure:services-up

  echo "Building trading bot..."
  bazel build //howling_tools:execute

  echo "Starting trading bot..."
  ./bazel-bin/howling_tools/execute \
    --stocks="$STOCKS" \
    --analyzer="$ANALYZER" \
    --account="$ACCOUNT" \
    --headless \
    --database=postgres \
    --bao_token=root \
    --logging_always_flush
) 2>&1 | tee -i "$RAW_LOG"
EXEC_EXIT_CODE=${PIPESTATUS[0]}
set -e

# Strip intermediate carriage-return redraws from the captured terminal output.
sed -E $'s/\r+$//; s/.*\r//' "$RAW_LOG" > "$EXEC_LOG"
rm -f "$RAW_LOG"

echo "=================================================="
echo "Trading session finished (exit code: $EXEC_EXIT_CODE)."
echo "Log saved to: $EXEC_LOG"
echo "Launching post-execution analysis..."
echo "=================================================="

AGY_INIT_PROMPT="The scheduled trading session just finished with exit code \
${EXEC_EXIT_CODE} (stocks=${STOCKS}, analyzer=${ANALYZER}, \
account=${ACCOUNT}).
Read the captured execution output at ${EXEC_LOG} and briefly acknowledge that \
you have read it. Do not run any commands, modify any files, create any plans, \
or produce the full report yet — wait for my next message."

AGY_FOLLOWUP_PROMPT="Now complete the post-execution analysis based on the log \
you just read:
1. Check the logs of any relevant Docker services (e.g. auth_service / docker \
compose services).
2. Produce a clear summary report of the day's trading activities, metrics, \
and service behavior.
3. If any errors, warnings, or anomalies occurred, investigate the codebase \
and logs to identify the root cause and propose a fix.
4. IMPORTANT: Do not take any actions, modify any files, or restart any \
services. Only perform read-only inspection and produce the report for me to \
read."

AGY_BIN="$HOME/.gemini/bin/agy"
if [ ! -x "$AGY_BIN" ]; then
  AGY_BIN="agy"
fi
PROJECT_ID="37064d7b-9449-451c-859a-208b6ecc1e80"

# Preload the execution log in a fresh conversation first (read-only, no shell
# permissions needed), then resume it interactively with --remote-control so
# permission prompts (e.g. Docker logs) can be approved remotely and the
# -i + --remote-control startup race is avoided.
"$AGY_BIN" \
  --app_data_dir=antigravity \
  --project="$PROJECT_ID" \
  --add-dir /tmp \
  --disable-slash-commands \
  -p "$AGY_INIT_PROMPT" > /dev/null

"$AGY_BIN" \
  --app_data_dir=antigravity \
  --project="$PROJECT_ID" \
  --add-dir /tmp \
  --remote-control \
  -c \
  -i "$AGY_FOLLOWUP_PROMPT"
