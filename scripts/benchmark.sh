#!/usr/bin/env bash
# =============================================================================
# benchmark.sh — PRIoTP CPU Utilization Benchmarking Script
# =============================================================================
# Usage:  ./benchmark.sh <num_users>
# Example: ./benchmark.sh 5
#
# Description:
#   Launches <num_users> PRTP_client processes against a *pre-running* server,
#   then for 120 seconds records per-second:
#     • system-wide CPU utilisation  (via /proc/stat — accurate, no external
#       tools required; preferred over top/mpstat)
#     • per-process CPU sum for all PRTP_client PIDs  (via /proc/<pid>/stat)
#     • system-wide memory used in MB      (via /proc/meminfo)
#     • aggregate RSS of PRTP_client PIDs  (via /proc/<pid>/status VmRSS)
#     • throughput proxy: packets logged to client*.out files per interval
#   Output: results/cpu_usage_<N>_users.csv
#
# Assumptions:
#   • The PRIoTP server (PRTP_server, started via STGen_server.py) is already
#     running in a separate terminal.
#   • The sensor-launcher.py is also already running in that terminal.
#   • pidstat (sysstat) is optional — the script falls back gracefully.
#   • The sensor list (PRTP/application/sensor.list) already has ≥1 sensor.
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# 0.  Auto-detect project structure (do not hardcode)
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BASE_DIR="$(dirname "$SCRIPT_DIR")"          # project root
APP_DIR="$BASE_DIR/PRTP/application"         # where PRTP_client lives

CLIENT_BINARY="$APP_DIR/PRTP_client"
RESULTS_DIR="$BASE_DIR/results"

SERVER_IP="127.0.0.1"
PORT_CLIENT=5005
DURATION=600          # seconds
WARMUP=5              # seconds to ignore at the start
INTERVAL=1            # sample every N seconds
NUM_SENSORS=4         # number of sensors to register per client

# ---------------------------------------------------------------------------
# 1.  Validate input
# ---------------------------------------------------------------------------
if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <num_users>" >&2
    echo "  Example: $0 5" >&2
    exit 1
fi

NUM_USERS="$1"

if ! [[ "$NUM_USERS" =~ ^[1-9][0-9]*$ ]]; then
    echo "[ERROR] <num_users> must be a positive integer." >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# 2.  Sanity checks
# ---------------------------------------------------------------------------
if [[ ! -x "$CLIENT_BINARY" ]]; then
    echo "[ERROR] PRTP_client not found or not executable at: $CLIENT_BINARY" >&2
    echo "        Run 'make' inside PRTP/ first." >&2
    exit 1
fi

# Check whether the server appears to be running (soft-check)
if ! pgrep -x "PRTP_server" > /dev/null 2>&1; then
    echo "[WARNING] PRTP_server process not detected. Make sure the server" >&2
    echo "          and sensor-launcher are running in a separate terminal:" >&2
    echo ""
    echo "    Terminal 1 (server):"
    echo "      cd $BASE_DIR/launcher"
    echo "      python3 sensor-launcher.py localhost 5004 9999 $NUM_SENSORS &"
    echo "      python3 STGen_server.py ../PRTP/conf/test.conf localhost 5004 $PORT_CLIENT 9999"
    echo ""
    echo "    Press ENTER to continue anyway, or Ctrl-C to abort."
    read -r
fi

mkdir -p "$RESULTS_DIR"
OUTPUT_CSV="$RESULTS_DIR/throughput_cpu_memory_usage_${NUM_USERS}_users.csv"

# ---------------------------------------------------------------------------
# 3.  Helper: read total CPU jiffies from /proc/stat  (no external tool)
# ---------------------------------------------------------------------------
# Returns: "<user> <nice> <system> <idle> <iowait> <irq> <softirq> <steal>"
read_cpu_stat() {
    awk '/^cpu / {print $2,$3,$4,$5,$6,$7,$8,$9}' /proc/stat
}

# Given two stat snapshots, compute % busy (0-100)
calc_cpu_percent() {
    local prev="$1" cur="$2"
    local -a p=($prev) c=($cur)
    local prev_idle=$((p[3] + p[4]))   # idle + iowait
    local cur_idle=$((c[3] + c[4]))
    local prev_total=0 cur_total=0
    for v in "${p[@]}"; do ((prev_total += v)); done
    for v in "${c[@]}"; do ((cur_total  += v)); done
    local delta_total=$((cur_total  - prev_total))
    local delta_idle=$((cur_idle   - prev_idle))
    if [[ $delta_total -eq 0 ]]; then
        echo "0.00"
    else
        awk "BEGIN {printf \"%.2f\", 100*(1 - $delta_idle/$delta_total)}"
    fi
}

# ---------------------------------------------------------------------------
# 4.  Helper: sum CPU% for a list of PIDs from /proc/<pid>/stat
#     Returns aggregate as a floating-point string
# ---------------------------------------------------------------------------
declare -A PREV_PROC_TIMES   # keyed by PID

read_proc_jiffies() {
    local pid="$1"
    # fields 14+15 = utime+stime (in jiffies)
    if [[ -r "/proc/$pid/stat" ]]; then
        awk '{print $14+$15}' /proc/$pid/stat 2>/dev/null || echo "0"
    else
        echo "0"
    fi
}

calc_proc_cpu_percent() {
    local -n _pids=$1   # nameref to an array of PIDs
    local delta_wall=$2  # wall-clock seconds between samples (integer)
    local hz
    hz=$(getconf CLK_TCK 2>/dev/null || echo 100)
    local total_cpu=0
    for pid in "${_pids[@]}"; do
        local cur_j
        cur_j=$(read_proc_jiffies "$pid")
        local prev_j="${PREV_PROC_TIMES[$pid]:-0}"
        PREV_PROC_TIMES[$pid]=$cur_j
        local delta_j=$(( cur_j - prev_j ))
        # cpu% = delta_jiffies / (delta_wall * hz) * 100
        local pct
        pct=$(awk "BEGIN {printf \"%.2f\", $delta_j / ($delta_wall * $hz) * 100}")
        total_cpu=$(awk "BEGIN {printf \"%.2f\", $total_cpu + $pct}")
    done
    echo "$total_cpu"
}

# ---------------------------------------------------------------------------
# 5.  Helper: count NEW lines written to client*.out since last check
# ---------------------------------------------------------------------------
declare -A PREV_LINE_COUNTS   # keyed by client index

get_throughput_delta() {
    local -n _num=$1   # nameref to NUM_USERS
    local total=0
    for i in $(seq 1 "$_num"); do
        local f="$APP_DIR/client${i}.out"
        local cur=0
        if [[ -f "$f" ]]; then
            cur=$(wc -l < "$f" 2>/dev/null || echo 0)
        fi
        local prev="${PREV_LINE_COUNTS[$i]:-0}"
        PREV_LINE_COUNTS[$i]=$cur
        total=$(( total + cur - prev ))
    done
    echo "$total"
}

# ---------------------------------------------------------------------------
# 5b. Helper: system-wide memory used in MB  (MemTotal - MemAvailable)
#     Uses /proc/meminfo — always available, no external tools
# ---------------------------------------------------------------------------
get_system_mem_mb() {
    awk '
        /^MemTotal:/     { total=$2 }
        /^MemAvailable:/ { avail=$2 }
        END { printf "%.2f", (total - avail) / 1024 }
    ' /proc/meminfo
}

# ---------------------------------------------------------------------------
# 5c. Helper: sum of VmRSS (resident set size) across alive PRTP_client PIDs
#     Returns total RSS in MB
# ---------------------------------------------------------------------------
get_clients_mem_mb() {
    local -n _mpids=$1   # nameref to array of alive PIDs
    local total_kb=0
    for pid in "${_mpids[@]}"; do
        if [[ -r "/proc/$pid/status" ]]; then
            local rss_kb
            rss_kb=$(awk '/^VmRSS:/ {print $2}' /proc/$pid/status 2>/dev/null || echo 0)
            total_kb=$(( total_kb + rss_kb ))
        fi
    done
    awk "BEGIN {printf \"%.2f\", $total_kb / 1024}"
}

# ---------------------------------------------------------------------------
# 6.  Cleanup — trap SIGINT / SIGTERM / EXIT
# ---------------------------------------------------------------------------
CLIENT_PIDS=()
MONITOR_PID=""

cleanup() {
    echo ""
    echo "[*] Cleaning up..."

    # Kill all client processes
    local dead=0
    for pid in "${CLIENT_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null
            wait "$pid" 2>/dev/null || true
        else
            ((dead++)) || true
        fi
    done

    # Report any clients that already exited
    if [[ $dead -gt 0 ]]; then
        echo "[WARNING] $dead client(s) had already exited before cleanup."
    fi

    echo "[*] All client processes stopped."
    echo "[*] Output saved to: $OUTPUT_CSV"
}

trap cleanup EXIT INT TERM

# ---------------------------------------------------------------------------
# 7.  Prepare output log directory for clients
# ---------------------------------------------------------------------------
echo "[*] Preparing client log directories..."
for i in $(seq 1 "$NUM_USERS"); do
    mkdir -p "$APP_DIR/client${i}_sensor_log"
    # Truncate previous .out files so line-count throughput starts at 0
    > "$APP_DIR/client${i}.out"
done

# ---------------------------------------------------------------------------
# 8.  Launch clients
# ---------------------------------------------------------------------------
echo "[*] Launching $NUM_USERS clients..."
echo "    Binary : $CLIENT_BINARY"
echo "    Server : $SERVER_IP:$PORT_CLIENT"

cd "$APP_DIR"

for i in $(seq 1 "$NUM_USERS"); do
    ./PRTP_client \
        -l "./client${i}_sensor_log" \
        -s "$SERVER_IP" \
        -p "$PORT_CLIENT" \
        -A \
        > "client${i}.out" 2>&1 &
    CLIENT_PIDS+=($!)
    echo "  Launched client $i  (PID: ${CLIENT_PIDS[-1]})"
done

# Seed PREV_PROC_TIMES so first delta = 0 (not garbage)
for pid in "${CLIENT_PIDS[@]}"; do
    PREV_PROC_TIMES[$pid]=$(read_proc_jiffies "$pid")
done

# ---------------------------------------------------------------------------
# 9.  Warm-up phase
# ---------------------------------------------------------------------------
echo "[*] Warm-up: ${WARMUP}s (data collected but marked as warm-up)..."
sleep "$WARMUP"

# ---------------------------------------------------------------------------
# 10. Write CSV header
# ---------------------------------------------------------------------------
echo "timestamp,epoch,cpu_system_pct,cpu_clients_pct,mem_system_mb,mem_clients_mb,throughput_lines_per_sec,user_count,phase" \
    > "$OUTPUT_CSV"

echo "[*] Recording metrics for ${DURATION}s  →  $OUTPUT_CSV"
echo "    Columns: timestamp | epoch | cpu_system% | cpu_clients% | mem_system_MB | mem_clients_MB | throughput | users | phase"
echo "    (warm-up samples are included and labelled 'warmup')"

# ---------------------------------------------------------------------------
# 11. Metric collection loop
# ---------------------------------------------------------------------------
ELAPSED=0
PREV_CPU_STAT=$(read_cpu_stat)

# Seed line counts
get_throughput_delta NUM_USERS > /dev/null   # first call initialises PREV_LINE_COUNTS

# Re-baseline cpu after warmup
PREV_CPU_STAT=$(read_cpu_stat)
SAMPLE_START=$(date +%s%N)   # nanoseconds for accurate wall-clock deltas

while [[ $ELAPSED -lt $DURATION ]]; do
    LOOP_START=$(date +%s%N)

    TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S")
    EPOCH=$(date +%s)

    # ---- System-wide CPU% ----
    CUR_CPU_STAT=$(read_cpu_stat)
    CPU_SYS=$(calc_cpu_percent "$PREV_CPU_STAT" "$CUR_CPU_STAT")
    PREV_CPU_STAT="$CUR_CPU_STAT"

    # ---- Per-client CPU% sum ----
    ALIVE_PIDS=()
    DEAD_COUNT=0
    for pid in "${CLIENT_PIDS[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            ALIVE_PIDS+=("$pid")
        else
            ((DEAD_COUNT++)) || true
        fi
    done

    if [[ $DEAD_COUNT -gt 0 ]]; then
        echo "[WARNING] $DEAD_COUNT client(s) exited unexpectedly at ${TIMESTAMP}" >&2
    fi

    # If all clients are dead, bail early
    if [[ ${#ALIVE_PIDS[@]} -eq 0 ]]; then
        echo "[ERROR] All clients have exited. Aborting benchmark." >&2
        break
    fi

    CPU_CLIENTS=$(calc_proc_cpu_percent ALIVE_PIDS "$INTERVAL")

    # ---- Memory: system-wide and per-client RSS ----
    MEM_SYS=$(get_system_mem_mb)
    MEM_CLIENTS=$(get_clients_mem_mb ALIVE_PIDS)

    # ---- Throughput proxy ----
    THROUGHPUT=$(get_throughput_delta NUM_USERS)

    # ---- Write row ----
    echo "$TIMESTAMP,$EPOCH,$CPU_SYS,$CPU_CLIENTS,$MEM_SYS,$MEM_CLIENTS,$THROUGHPUT,$NUM_USERS,active" \
        >> "$OUTPUT_CSV"

    # Echo live summary to terminal
    printf "  [t=%3ds] sys=%-6s%%  cli=%-6s%%  mem_sys=%-8s MB  mem_cli=%-8s MB  lines/s=%-6s  alive=%d/%d\n" \
        "$ELAPSED" "$CPU_SYS" "$CPU_CLIENTS" "$MEM_SYS" "$MEM_CLIENTS" "$THROUGHPUT" \
        "${#ALIVE_PIDS[@]}" "$NUM_USERS"

    # ---- Accurate sleep: compensate for processing time ----
    LOOP_END=$(date +%s%N)
    LOOP_DUR_MS=$(( (LOOP_END - LOOP_START) / 1000000 ))
    SLEEP_MS=$(( INTERVAL * 1000 - LOOP_DUR_MS ))
    if [[ $SLEEP_MS -gt 0 ]]; then
        sleep "$(awk "BEGIN {printf \"%.3f\", $SLEEP_MS/1000}")"
    fi

    ((ELAPSED += INTERVAL)) || true
done

# ---------------------------------------------------------------------------
# 12. Summary
# ---------------------------------------------------------------------------
TOTAL_ROWS=$(( $(wc -l < "$OUTPUT_CSV") - 1 ))
echo ""
echo "===================================================================="
echo " Benchmark complete"
echo "===================================================================="
echo "  Users    : $NUM_USERS"
echo "  Duration : ${DURATION}s  (warm-up: ${WARMUP}s excluded from label)"
echo "  Samples  : $TOTAL_ROWS rows"
echo "  CSV file : $OUTPUT_CSV"
echo "===================================================================="
echo ""
echo " Quick plot suggestion (Python):"
echo "   import pandas as pd, matplotlib.pyplot as plt"
echo "   df = pd.read_csv('$OUTPUT_CSV')"
echo "   fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14,5))"
echo "   df[['cpu_system_pct','cpu_clients_pct']].plot(ax=ax1, title='PRIoTP CPU – $NUM_USERS users')"
echo "   df[['mem_system_mb','mem_clients_mb']].plot(ax=ax2, title='PRIoTP Memory – $NUM_USERS users')"
echo "   ax1.set_ylabel('CPU %'); ax2.set_ylabel('Memory (MB')"
echo "   plt.tight_layout(); plt.show()"
