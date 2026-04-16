#!/bin/bash
# =============================================================================
# PRIoTP Localhost Benchmark Suite
# =============================================================================
# A rigorous, reproducible benchmarking framework for evaluating PRIoTP
# (Partial-Reliable Internet of Things Protocol) on the loopback interface.
#
# Methodology:
#   Phase 0 — System baseline: capture kernel/network stack parameters
#   Phase 1 — Network baseline: iperf3 UDP throughput + ping RTT on loopback
#   Phase 2 — PRTP experiment: server + sensors + clients under tcpdump capture
#             with concurrent pidstat and monitor_server.py sampling
#   Phase 3 — Post-processing: aggregate all artefacts for analysis
#
# Required tools: tcpdump, pidstat (sysstat), ping, ss
# Optional tools: iperf3 (for UDP throughput baseline), tshark (for header stats)
#
# Usage:
#   sudo ./run_localhost_benchmark.sh [OPTIONS]
#
# Options:
#   --duration <sec>    Test duration in seconds        (default: 60)
#   --num-sensors <n>   Number of sensor simulators     (default: 4)
#   --num-clients <n>   Number of PRTP_client instances (default: 3)
#   --server-ip <ip>    Bind address                    (default: 127.0.0.1)
#   --sensor-port <p>   Server sensor-publish port      (default: 5000)
#   --client-port <p>   Server client-subscribe port    (default: 5001)
#   --skip-baseline     Skip iperf3/ping baseline phase
#   --skip-capture      Skip tcpdump packet capture
#   --output-dir <dir>  Custom results directory
#
# Output artefacts (all under $RESULTS_DIR):
#   system_baseline.txt         — kernel, CPU, memory, socket buffer params
#   ping_baseline.txt           — loopback RTT statistics (100 packets)
#   iperf3_udp_baseline.json    — raw UDP throughput/jitter  (if iperf3 present)
#   prtp_server.log             — server stdout/stderr
#   prtp_capture.pcap           — full packet capture on lo  (if tcpdump)
#   pidstat_server.csv          — per-second CPU/memory for PRTP_server PID
#   pidstat_clients.csv         — per-second CPU/memory for PRTP_client PIDs
#   monitor_metrics.txt         — psutil-based system-wide metrics
#   socket_buffer_samples.txt   — periodic ss UDP socket queue snapshots
#   client_*/                   — per-client sensor delivery logs
#   sensor_log/                 — sensor-side sent logs
#   server_sensor_log/          — server-side received logs
#   experiment_metadata.json    — reproducibility record
# =============================================================================

set -euo pipefail

# ─── Defaults ────────────────────────────────────────────────────────────────
DURATION=60
NUM_SENSORS=4
NUM_CLIENTS=3
SERVER_IP="127.0.0.1"
SENSOR_PORT=5000
CLIENT_PORT=5001
SKIP_BASELINE=false
SKIP_CAPTURE=false
OUTPUT_DIR=""

# ─── Parse arguments ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration)       DURATION="$2";      shift 2 ;;
        --num-sensors)    NUM_SENSORS="$2";   shift 2 ;;
        --num-clients)    NUM_CLIENTS="$2";   shift 2 ;;
        --server-ip)      SERVER_IP="$2";     shift 2 ;;
        --sensor-port)    SENSOR_PORT="$2";   shift 2 ;;
        --client-port)    CLIENT_PORT="$2";   shift 2 ;;
        --skip-baseline)  SKIP_BASELINE=true; shift   ;;
        --skip-capture)   SKIP_CAPTURE=true;  shift   ;;
        --output-dir)     OUTPUT_DIR="$2";    shift 2 ;;
        -h|--help)
            echo "Usage: sudo ./run_localhost_benchmark.sh [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --duration <sec>    Test duration in seconds        (default: 60)"
            echo "  --num-sensors <n>   Number of sensor simulators     (default: 4)"
            echo "  --num-clients <n>   Number of PRTP_client instances (default: 3)"
            echo "  --server-ip <ip>    Bind address                    (default: 127.0.0.1)"
            echo "  --sensor-port <p>   Server sensor-publish port      (default: 5000)"
            echo "  --client-port <p>   Server client-subscribe port    (default: 5001)"
            echo "  --skip-baseline     Skip iperf3/ping baseline phase"
            echo "  --skip-capture      Skip tcpdump packet capture"
            echo "  --output-dir <dir>  Custom results directory"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

# ─── Paths ───────────────────────────────────────────────────────────────────
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PRTP_APP_DIR="$(cd "$SCRIPT_DIR/../PRTP/application" && pwd)"
PRTP_SRC_DIR="$(cd "$SCRIPT_DIR/../PRTP/src" && pwd)"
PRTP_CONF_DIR="$(cd "$SCRIPT_DIR/../PRTP/conf" && pwd)"

SERVER_BIN="$PRTP_APP_DIR/PRTP_server"
CLIENT_BIN="$PRTP_APP_DIR/PRTP_client"

if [[ ! -x "$SERVER_BIN" ]]; then
    echo "ERROR: PRTP_server binary not found at $SERVER_BIN"
    echo "Run 'make' in the PRTP directory first."
    exit 1
fi
if [[ ! -x "$CLIENT_BIN" ]]; then
    echo "ERROR: PRTP_client binary not found at $CLIENT_BIN"
    exit 1
fi

# Results directory
if [[ -z "$OUTPUT_DIR" ]]; then
    RESULTS_DIR="$SCRIPT_DIR/benchmark_results_$(date +%Y%m%d_%H%M%S)"
else
    if [[ "$OUTPUT_DIR" = /* ]]; then
        RESULTS_DIR="$OUTPUT_DIR"
    else
        RESULTS_DIR="$(pwd)/$OUTPUT_DIR"
    fi
fi
mkdir -p "$RESULTS_DIR"

# ─── Cleanup handler ────────────────────────────────────────────────────────
PIDS_TO_KILL=()

cleanup() {
    echo ""
    echo "[Cleanup] Terminating background processes..."
    for pid in "${PIDS_TO_KILL[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -SIGINT "$pid" 2>/dev/null || true
        fi
    done
    # Give processes time to flush logs
    sleep 2
    for pid in "${PIDS_TO_KILL[@]}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill -9 "$pid" 2>/dev/null || true
        fi
    done
    echo "[Cleanup] Done."
}
trap cleanup EXIT

# ─── Helper: check if command exists ─────────────────────────────────────────
has_cmd() { command -v "$1" &>/dev/null; }

# ─── Banner ──────────────────────────────────────────────────────────────────
echo "========================================================================"
echo "  PRIoTP Localhost Benchmark Suite"
echo "========================================================================"
echo "  Date            : $(date --iso-8601=seconds)"
echo "  Server IP       : $SERVER_IP"
echo "  Sensor port     : $SENSOR_PORT"
echo "  Client port     : $CLIENT_PORT"
echo "  Duration        : ${DURATION}s"
echo "  Sensors         : $NUM_SENSORS"
echo "  Clients         : $NUM_CLIENTS"
echo "  Results         : $RESULTS_DIR"
echo "  Skip baseline   : $SKIP_BASELINE"
echo "  Skip capture    : $SKIP_CAPTURE"
echo "========================================================================"
echo ""

# =============================================================================
# PHASE 0: System Baseline
# =============================================================================
echo "──── Phase 0: System Baseline ────────────────────────────────────────"

{
    echo "=== System Baseline ==="
    echo "Timestamp: $(date --iso-8601=seconds)"
    echo ""

    echo "--- Kernel ---"
    uname -a
    echo ""

    echo "--- CPU ---"
    lscpu | grep -E "^(Architecture|CPU\(s\)|Model name|CPU MHz|Thread|Core)" 2>/dev/null || true
    echo ""

    echo "--- Memory ---"
    free -h
    echo ""

    echo "--- File Descriptor Limit ---"
    ulimit -n
    echo ""

    echo "--- Network Stack Parameters ---"
    echo "net.core.rmem_max = $(sysctl -n net.core.rmem_max 2>/dev/null || echo N/A)"
    echo "net.core.wmem_max = $(sysctl -n net.core.wmem_max 2>/dev/null || echo N/A)"
    echo "net.core.rmem_default = $(sysctl -n net.core.rmem_default 2>/dev/null || echo N/A)"
    echo "net.core.wmem_default = $(sysctl -n net.core.wmem_default 2>/dev/null || echo N/A)"
    echo "net.core.netdev_max_backlog = $(sysctl -n net.core.netdev_max_backlog 2>/dev/null || echo N/A)"
    echo "net.ipv4.udp_rmem_min = $(sysctl -n net.ipv4.udp_rmem_min 2>/dev/null || echo N/A)"
    echo "net.ipv4.udp_wmem_min = $(sysctl -n net.ipv4.udp_wmem_min 2>/dev/null || echo N/A)"
    echo "net.ipv4.udp_mem = $(sysctl -n net.ipv4.udp_mem 2>/dev/null || echo N/A)"
    echo ""

    echo "--- Loopback Interface MTU ---"
    ip link show lo 2>/dev/null | head -2 || ifconfig lo 2>/dev/null | head -2
    echo ""

    echo "--- Installed Tool Versions ---"
    for tool in tcpdump pidstat ping iperf3 tshark ss; do
        if has_cmd "$tool"; then
            ver=$("$tool" --version 2>&1 | head -1 || echo "present")
            echo "$tool: $ver"
        else
            echo "$tool: NOT INSTALLED"
        fi
    done

} > "$RESULTS_DIR/system_baseline.txt" 2>&1

echo "  [OK] system_baseline.txt"

# =============================================================================
# PHASE 1: Network Baseline (iperf3 + ping)
# =============================================================================
if [[ "$SKIP_BASELINE" == false ]]; then
    echo ""
    echo "──── Phase 1: Network Baseline ──────────────────────────────────────"

    # --- Ping baseline ---
    echo "  Running ping baseline (100 packets)..."
    ping -c 100 -i 0.01 "$SERVER_IP" > "$RESULTS_DIR/ping_baseline.txt" 2>&1 || true
    # Extract summary line
    tail -2 "$RESULTS_DIR/ping_baseline.txt"
    echo "  [OK] ping_baseline.txt"

    # --- iperf3 UDP baseline ---
    if has_cmd iperf3; then
        echo "  Running iperf3 UDP baseline (10s, target 1Gbps)..."

        # Start iperf3 server in background
        iperf3 -s -1 -D -p 5199 2>/dev/null || true
        sleep 1

        # Run client: UDP mode, 1Gbps target, 10 seconds, JSON output
        iperf3 -c "$SERVER_IP" -u -b 1G -t 10 -p 5199 \
            --json > "$RESULTS_DIR/iperf3_udp_baseline.json" 2>&1 || true

        # Kill iperf3 server if still running
        pkill -f "iperf3 -s.*5199" 2>/dev/null || true

        echo "  [OK] iperf3_udp_baseline.json"
    else
        echo "  [SKIP] iperf3 not installed — install with: sudo apt install iperf3"
        echo "  (UDP throughput baseline will not be available for comparison)"
    fi
else
    echo ""
    echo "──── Phase 1: SKIPPED (--skip-baseline) ─────────────────────────────"
fi

# =============================================================================
# PHASE 2: PRTP Experiment
# =============================================================================
echo ""
echo "──── Phase 2: PRTP Experiment ───────────────────────────────────────"

# --- Pre-experiment: clean old logs ---
echo "  Cleaning previous log directories..."
rm -rf "$PRTP_SRC_DIR/sensor_log" "$PRTP_SRC_DIR/server_sensor_log" 2>/dev/null || true
for i in $(seq 1 "$NUM_CLIENTS"); do
    rm -rf "$PRTP_APP_DIR/client${i}_sensor_log" 2>/dev/null || true
done
# Server writes per-sensor .log files in its CWD (PRTP_APP_DIR) when
# init_sensor_logger's chdir() to ../PRTP/src/server_sensor_log fails.
rm -f "$PRTP_APP_DIR"/device_*.log "$PRTP_APP_DIR"/temp_*.log \
      "$PRTP_APP_DIR"/gps_*.log "$PRTP_APP_DIR"/camera_*.log \
      "$PRTP_APP_DIR"/camera_*.data 2>/dev/null || true

# --- Prepare sensor.list ---
SENSOR_LIST_FILE="$SCRIPT_DIR/sensor.list"
SENSOR_TYPES=("device" "temp" "gps" "camera")
> "$SENSOR_LIST_FILE"
for i in $(seq 0 $((NUM_SENSORS - 1))); do
    stype="${SENSOR_TYPES[$((i % 4))]}"
    echo "${stype}_${i}" >> "$SENSOR_LIST_FILE"
done
echo "  Generated sensor.list with $NUM_SENSORS sensors"

# --- Find Q-table ---
Q_TABLE_ARG=""
for qpath in \
    "$SCRIPT_DIR/q_agent_trained.csv" \
    "$PRTP_APP_DIR/q_agent_trained.csv"; do
    if [[ -f "$qpath" ]]; then
        Q_TABLE_ARG="-q${qpath}"
        echo "  Q-table: $qpath"
        break
    fi
done
if [[ -z "$Q_TABLE_ARG" ]]; then
    echo "  WARNING: q_agent_trained.csv not found, Q-agent uses default values"
fi

# --- Start tcpdump capture ---
TCPDUMP_PID=""
if [[ "$SKIP_CAPTURE" == false ]] && has_cmd tcpdump; then
    echo "  Starting tcpdump on lo interface..."
    # Capture only UDP traffic on the sensor and client ports
    tcpdump -i lo -w "$RESULTS_DIR/prtp_capture.pcap" \
        "udp and (port $SENSOR_PORT or port $CLIENT_PORT)" \
        -s 0 &>/dev/null &
    TCPDUMP_PID=$!
    PIDS_TO_KILL+=("$TCPDUMP_PID")
    echo "  [OK] tcpdump PID=$TCPDUMP_PID"
    sleep 1
elif [[ "$SKIP_CAPTURE" == true ]]; then
    echo "  [SKIP] tcpdump (--skip-capture)"
else
    echo "  [SKIP] tcpdump not installed"
fi

# --- Start socket buffer monitor ---
echo "  Starting socket buffer monitor (ss snapshots every 2s)..."
(
    while true; do
        echo "--- $(date +%s.%N) ---"
        # Filter to PRTP ports only; -m shows skmem buffer details
        ss -unm "sport = :${SENSOR_PORT} or sport = :${CLIENT_PORT} or dport = :${SENSOR_PORT} or dport = :${CLIENT_PORT}" 2>/dev/null || true
        sleep 2
    done
) > "$RESULTS_DIR/socket_buffer_samples.txt" 2>&1 &
SS_MONITOR_PID=$!
PIDS_TO_KILL+=("$SS_MONITOR_PID")

# --- Start PRTP_server ---
echo "  Starting PRTP_server..."

# Build server command
SERVER_CMD=(
    "$SERVER_BIN"
    "-i${SERVER_IP}"
    "-p${SENSOR_PORT}"
    "-s${CLIENT_PORT}"
    "-l${SENSOR_LIST_FILE}"
    "-c${PRTP_CONF_DIR}/test.conf"
)
if [[ -n "$Q_TABLE_ARG" ]]; then
    SERVER_CMD+=("$Q_TABLE_ARG")
fi

cd "$PRTP_APP_DIR"
"${SERVER_CMD[@]}" > "$RESULTS_DIR/prtp_server.log" 2>&1 &
SERVER_PID=$!
PIDS_TO_KILL+=("$SERVER_PID")
echo "  [OK] PRTP_server PID=$SERVER_PID"
cd "$SCRIPT_DIR"

# Wait for server to initialise
sleep 2

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "  ERROR: PRTP_server exited prematurely. Check $RESULTS_DIR/prtp_server.log"
    cat "$RESULTS_DIR/prtp_server.log"
    exit 1
fi

# --- Start pidstat for server ---
echo "  Starting pidstat for PRTP_server (PID=$SERVER_PID)..."
pidstat -p "$SERVER_PID" -u -r -d 1 > "$RESULTS_DIR/pidstat_server.csv" 2>&1 &
PIDSTAT_SERVER_PID=$!
PIDS_TO_KILL+=("$PIDSTAT_SERVER_PID")

# --- Start monitor_server.py ---
echo "  Starting monitor_server.py..."
python3 "$SCRIPT_DIR/monitor_server.py" \
    --interval 1 \
    --duration $((DURATION + 20)) \
    > "$RESULTS_DIR/monitor_metrics.txt" 2>&1 &
MONITOR_PID=$!
PIDS_TO_KILL+=("$MONITOR_PID")

# --- Start sensors ---
echo "  Starting $NUM_SENSORS sensor simulators..."
SENSOR_PIDS=()
i=0
while IFS= read -r sensor_entry; do
    stype="${sensor_entry%%_*}"
    sid="${sensor_entry#*_}"
    python3 "$SCRIPT_DIR/sensor.py" "$stype" "$SERVER_IP" "$SENSOR_PORT" "$sid" &
    SENSOR_PIDS+=($!)
    PIDS_TO_KILL+=($!)
    ((i++)) || true
done < "$SENSOR_LIST_FILE"
echo "  [OK] $i sensors started"

# Wait for sensors to register
sleep 3

# --- Start PRTP_client instances ---
echo "  Starting $NUM_CLIENTS PRTP_client instances..."
CLIENT_PIDS=()
for c in $(seq 1 "$NUM_CLIENTS"); do
    LOG_DIR="$PRTP_APP_DIR/client${c}_sensor_log"
    mkdir -p "$LOG_DIR"

    "$CLIENT_BIN" \
        -s "$SERVER_IP" \
        -p "$CLIENT_PORT" \
        -l "$LOG_DIR" \
        -a -N \
        > "$RESULTS_DIR/prtp_client${c}.log" 2>&1 &
    cpid=$!
    CLIENT_PIDS+=("$cpid")
    PIDS_TO_KILL+=("$cpid")
    echo "    Client $c: PID=$cpid  log_dir=$LOG_DIR"

    # Stagger client starts to avoid subscribe storm
    sleep 1
done

# --- Start pidstat for all clients ---
CLIENT_PID_LIST=$(IFS=,; echo "${CLIENT_PIDS[*]}")
if [[ -n "$CLIENT_PID_LIST" ]]; then
    # pidstat -p takes comma-separated PIDs
    pidstat -p "$(echo "${CLIENT_PIDS[@]}" | tr ' ' ',')" -u -r -d 1 \
        > "$RESULTS_DIR/pidstat_clients.csv" 2>&1 &
    PIDSTAT_CLIENT_PID=$!
    PIDS_TO_KILL+=("$PIDSTAT_CLIENT_PID")
fi

# --- Wait for experiment duration ---
echo ""
echo "  ════════════════════════════════════════════════════════════════════"
echo "  Experiment running for ${DURATION}s ..."
echo "  Start: $(date --iso-8601=seconds)"
echo "  ════════════════════════════════════════════════════════════════════"

ELAPSED=0
REPORT_INTERVAL=10
while [[ $ELAPSED -lt $DURATION ]]; do
    sleep "$REPORT_INTERVAL"
    ELAPSED=$((ELAPSED + REPORT_INTERVAL))
    if [[ $ELAPSED -le $DURATION ]]; then
        echo "  [${ELAPSED}/${DURATION}s] Server alive=$(kill -0 $SERVER_PID 2>/dev/null && echo yes || echo NO)"
    fi
done

echo ""
echo "  Duration complete. Stopping processes..."

# =============================================================================
# PHASE 3: Shutdown and collect artefacts
# =============================================================================
echo ""
echo "──── Phase 3: Collect Artefacts ─────────────────────────────────────"

# Stop clients first (graceful)
for cpid in "${CLIENT_PIDS[@]}"; do
    kill -SIGINT "$cpid" 2>/dev/null || true
done
sleep 2

# Stop sensors
for spid in "${SENSOR_PIDS[@]}"; do
    kill -SIGINT "$spid" 2>/dev/null || true
done
sleep 1

# Stop server
kill -SIGINT "$SERVER_PID" 2>/dev/null || true
sleep 2

# Stop monitoring
kill -SIGINT "$SS_MONITOR_PID" 2>/dev/null || true
kill -SIGINT "$PIDSTAT_SERVER_PID" 2>/dev/null || true
kill -SIGINT "${PIDSTAT_CLIENT_PID:-0}" 2>/dev/null || true
kill -SIGINT "$MONITOR_PID" 2>/dev/null || true

if [[ -n "$TCPDUMP_PID" ]]; then
    kill -SIGINT "$TCPDUMP_PID" 2>/dev/null || true
    sleep 1
fi

# --- Copy log directories ---
echo "  Copying log artefacts..."
if [[ -d "$PRTP_SRC_DIR/sensor_log" ]]; then
    cp -r "$PRTP_SRC_DIR/sensor_log" "$RESULTS_DIR/sensor_log"
    echo "    [OK] sensor_log/"
fi
if [[ -d "$PRTP_SRC_DIR/server_sensor_log" ]]; then
    cp -r "$PRTP_SRC_DIR/server_sensor_log" "$RESULTS_DIR/server_sensor_log"
    echo "    [OK] server_sensor_log/ (from src/)"
else
    # Server writes .log files to its CWD when chdir to server_sensor_log fails
    mkdir -p "$RESULTS_DIR/server_sensor_log"
    found_server_logs=false
    for pattern in device temp gps camera; do
        for logf in "$PRTP_APP_DIR"/${pattern}_*.log; do
            if [[ -f "$logf" ]]; then
                cp "$logf" "$RESULTS_DIR/server_sensor_log/"
                found_server_logs=true
            fi
        done
    done
    if [[ "$found_server_logs" == true ]]; then
        echo "    [OK] server_sensor_log/ (from application/)"
    else
        rmdir "$RESULTS_DIR/server_sensor_log" 2>/dev/null || true
        echo "    [WARN] server_sensor_log not found"
    fi
fi
for c in $(seq 1 "$NUM_CLIENTS"); do
    cdir="$PRTP_APP_DIR/client${c}_sensor_log"
    if [[ -d "$cdir" ]]; then
        cp -r "$cdir" "$RESULTS_DIR/client${c}_sensor_log"
        echo "    [OK] client${c}_sensor_log/"
    fi
done

# --- Write experiment metadata ---
cat > "$RESULTS_DIR/experiment_metadata.json" <<EOF
{
    "experiment": "PRIoTP Localhost Benchmark",
    "timestamp": "$(date --iso-8601=seconds)",
    "parameters": {
        "server_ip": "$SERVER_IP",
        "sensor_port": $SENSOR_PORT,
        "client_port": $CLIENT_PORT,
        "duration_seconds": $DURATION,
        "num_sensors": $NUM_SENSORS,
        "num_clients": $NUM_CLIENTS
    },
    "pids": {
        "server": $SERVER_PID,
        "clients": [$(echo "${CLIENT_PIDS[@]}" | tr ' ' ',')],
        "sensors": [$(echo "${SENSOR_PIDS[@]}" | tr ' ' ',')]
    },
    "tools": {
        "tcpdump": $(has_cmd tcpdump && echo true || echo false),
        "iperf3":  $(has_cmd iperf3  && echo true || echo false),
        "pidstat": $(has_cmd pidstat  && echo true || echo false),
        "tshark":  $(has_cmd tshark  && echo true || echo false)
    },
    "kernel": "$(uname -r)",
    "hostname": "$(hostname)"
}
EOF
echo "  [OK] experiment_metadata.json"

# --- Summary ---
echo ""
echo "========================================================================"
echo "  Experiment Complete"
echo "========================================================================"
echo "  Results directory: $RESULTS_DIR"
echo ""
echo "  Artefacts:"
ls -lhS "$RESULTS_DIR" | tail -n +2 | awk '{printf "    %-40s %s\n", $NF, $5}'
echo ""
echo "  Next steps:"
echo "    1. Analyse PRTP delivery:   python3 analyze_localhost_results.py $RESULTS_DIR"
echo "    2. Analyse UACK decisions:  python3 analyze_uack.py --server-log $RESULTS_DIR/prtp_server.log"
echo "    3. Inspect pcap:            tcpdump -r $RESULTS_DIR/prtp_capture.pcap | head -50"
echo "    4. Run R stat analysis:     Rscript stat.R  (after copying logs to expected dirs)"
echo "========================================================================"
