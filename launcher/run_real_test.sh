#!/bin/bash
# PRIoTP Real System Test - launches sensors, server, clients, captures logs
set -e

BASEDIR="$(cd "$(dirname "$0")/.." && pwd)"
LAUNCHER="$BASEDIR/launcher"
APP="$BASEDIR/PRTP/application"
CONF="$BASEDIR/PRTP/conf/test.conf"
QTABLE="$LAUNCHER/q_agent_trained.csv"
SERVER_BIN="$APP/PRTP_server"
CLIENT_BIN="$APP/PRTP_client"

NUM_SENSORS=${1:-15}
NUM_CLIENTS=${2:-5}
SIM_TIME=${3:-60}
SENSOR_PORT=5004
CLIENT_PORT=5005

# Output logs
SERVER_LOG="/tmp/prtp_server.log"
SENSOR_LOG="/tmp/prtp_sensors.log"

echo "============================================================"
echo "PRIoTP Real System Test"
echo "============================================================"
echo "Sensors:  $NUM_SENSORS on port $SENSOR_PORT"
echo "Clients:  $NUM_CLIENTS on port $CLIENT_PORT"
echo "Duration: ${SIM_TIME}s"
echo "Server:   $SERVER_BIN"
echo "Client:   $CLIENT_BIN"
echo "Q-table:  $QTABLE"
echo "Config:   $CONF"
echo "============================================================"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Shutting down all processes..."
    kill $SERVER_PID 2>/dev/null || true
    for pid in "${CLIENT_PIDS[@]}"; do
        kill $pid 2>/dev/null || true
    done
    kill $SENSOR_PID 2>/dev/null || true
    sleep 2
    # Force kill stragglers
    pkill -f "PRTP_server|PRTP_client" 2>/dev/null || true
    echo "Done."
}
trap cleanup EXIT

# Clean client log dirs
for i in $(seq 1 $NUM_CLIENTS); do
    rm -rf "$APP/client${i}_sensor_log"
    mkdir -p "$APP/client${i}_sensor_log"
done

# ── Step 1: Launch sensors ────────────────────────────────────────────
cd "$LAUNCHER"
echo "[1/3] Starting $NUM_SENSORS sensors..."
python3 sensor-launcher.py localhost $SENSOR_PORT $((SIM_TIME + 30)) $NUM_SENSORS > "$SENSOR_LOG" 2>&1 &
SENSOR_PID=$!
sleep 3

SENSOR_COUNT=$(ps aux | grep "sensor\.py" | grep -v grep | wc -l)
echo "      $SENSOR_COUNT sensor processes running"

# ── Step 2: Launch server (capture ALL output to log) ─────────────────
echo "[2/3] Starting PRTP server..."

# Server needs sensor.list in its CWD - sensor-launcher.py wrote it in $LAUNCHER
# We launch the server binary directly so we control its CWD
cd "$LAUNCHER"
"$SERVER_BIN" \
    -ilocalhost \
    -p$SENSOR_PORT \
    -s$CLIENT_PORT \
    -l"$LAUNCHER/sensor.list" \
    -c"$CONF" \
    -q"$QTABLE" \
    > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 3

if kill -0 $SERVER_PID 2>/dev/null; then
    echo "      Server running (PID $SERVER_PID)"
else
    echo "      ERROR: Server failed to start! Check $SERVER_LOG"
    cat "$SERVER_LOG" | tail -20
    exit 1
fi

# ── Step 3: Launch clients ────────────────────────────────────────────
echo "[3/3] Starting $NUM_CLIENTS clients (staggered 1s apart)..."
CLIENT_PIDS=()

for i in $(seq 1 $NUM_CLIENTS); do
    LOG_DIR="$(realpath "$APP/client${i}_sensor_log")"
    mkdir -p "$LOG_DIR"

    "$CLIENT_BIN" \
        -slocalhost \
        -p$CLIENT_PORT \
        -a \
        -l"$LOG_DIR" \
        > "$LOG_DIR/client.out" 2>"$LOG_DIR/client.err" &
    CLIENT_PIDS+=($!)
    echo "      Client $i started (PID ${CLIENT_PIDS[-1]}) -> $LOG_DIR/"
    sleep 1
done

echo ""
echo "All processes running. Test in progress for ${SIM_TIME}s..."
echo "Server log: $SERVER_LOG"
echo ""

# ── Wait for test duration ────────────────────────────────────────────
ELAPSED=0
while [ $ELAPSED -lt $SIM_TIME ]; do
    sleep 10
    ELAPSED=$((ELAPSED + 10))
    # Count server log lines for Q-decisions
    Q_COUNT=$(grep -c "Q-Decision" "$SERVER_LOG" 2>/dev/null || echo "0")
    ACK_COUNT=$(grep -ci "acknowledgement\|acknowledgment" "$SERVER_LOG" 2>/dev/null || echo "0")
    echo "  [${ELAPSED}s/${SIM_TIME}s] Q-decisions: $Q_COUNT | ACK/NACK events: $ACK_COUNT"
done

echo ""
echo "Test duration reached. Stopping clients..."

# Stop clients first
for pid in "${CLIENT_PIDS[@]}"; do
    kill -INT $pid 2>/dev/null || true
done
sleep 3

# Stop server
echo "Stopping server..."
kill -INT $SERVER_PID 2>/dev/null || true
sleep 2

# Stop sensors
kill $SENSOR_PID 2>/dev/null || true

echo ""
echo "============================================================"
echo "TEST COMPLETE"
echo "============================================================"
echo ""

# Summary
echo "Server log: $SERVER_LOG ($(wc -l < "$SERVER_LOG") lines)"
echo "Q-Decisions: $(grep -c 'Q-Decision' "$SERVER_LOG" 2>/dev/null || echo 0)"
echo "ACK events:  $(grep -ci 'update acknowledgement\|update acknowledgment' "$SERVER_LOG" 2>/dev/null || echo 0)"
echo "NACK events: $(grep -ci 'negative update\|UPDATE_NACK' "$SERVER_LOG" 2>/dev/null || echo 0)"
echo "DROP events: $(grep -c 'DROPPED' "$SERVER_LOG" 2>/dev/null || echo 0)"
echo ""

# Client log summary
for i in $(seq 1 $NUM_CLIENTS); do
    LOG_DIR="$APP/client${i}_sensor_log"
    LOG_COUNT=$(find "$LOG_DIR" -name "*.log" | wc -l)
    LINE_COUNT=0
    for f in "$LOG_DIR"/*.log 2>/dev/null; do
        [ -f "$f" ] && LINE_COUNT=$((LINE_COUNT + $(wc -l < "$f")))
    done
    echo "Client $i: $LOG_COUNT sensor log(s), $LINE_COUNT lines"
done

echo ""
echo "Server log saved to: $SERVER_LOG"
echo "Run analyzer:  python3 analyze_uack.py --server-log $SERVER_LOG --client-logs $APP --output-dir ./uack_real_results"
