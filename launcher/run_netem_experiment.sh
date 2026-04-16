#!/bin/bash
# =============================================================================
# PRIoTP Network Emulation Experiment Suite
# =============================================================================
# Uses Linux tc/netem to inject controlled latency and packet loss on the
# loopback interface, then runs the PRTP benchmark under each condition.
# This forces the Q-learning agent into the HIGH RTT state (>150ms) and
# allows observation of its adaptation behaviour.
#
# Experiment matrix:
#   Condition 0: Baseline       — 0ms delay, 0% loss  (control)
#   Condition 1: Moderate delay  — 100ms delay, 0% loss
#   Condition 2: High delay     — 200ms delay, 0% loss  (triggers HIGH RTT)
#   Condition 3: Moderate loss  — 0ms delay, 5% loss
#   Condition 4: High loss      — 0ms delay, 15% loss
#   Condition 5: Combined       — 200ms delay, 10% loss (worst case)
#
# Each condition runs a full benchmark: sensors + server + clients + capture.
# All results are collected under a single experiment directory for
# cross-condition comparison.
#
# Requires: sudo (for tc qdisc), tcpdump, pidstat
# Usage:
#   sudo ./run_netem_experiment.sh [OPTIONS]
#
# Options:
#   --duration <sec>     Per-condition test duration  (default: 60)
#   --num-sensors <n>    Number of sensors            (default: 4)
#   --num-clients <n>    Number of clients            (default: 3)
#   --output-dir <dir>   Custom output directory
#   --conditions <list>  Comma-separated condition indices to run (default: all)
#                        e.g. --conditions 0,2,5
# =============================================================================

set -euo pipefail

# ─── Defaults ────────────────────────────────────────────────────────────────
DURATION=60
NUM_SENSORS=4
NUM_CLIENTS=3
OUTPUT_DIR=""
CONDITIONS_FILTER=""

# ─── Parse arguments ─────────────────────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --duration)     DURATION="$2";     shift 2 ;;
        --num-sensors)  NUM_SENSORS="$2";  shift 2 ;;
        --num-clients)  NUM_CLIENTS="$2";  shift 2 ;;
        --output-dir)   OUTPUT_DIR="$2";   shift 2 ;;
        --conditions)   CONDITIONS_FILTER="$2"; shift 2 ;;
        -h|--help)
            echo "Usage: sudo ./run_netem_experiment.sh [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --duration <sec>     Per-condition duration  (default: 60)"
            echo "  --num-sensors <n>    Number of sensors       (default: 4)"
            echo "  --num-clients <n>    Number of clients       (default: 3)"
            echo "  --output-dir <dir>   Custom output directory"
            echo "  --conditions <list>  Comma-separated indices (default: all)"
            echo ""
            echo "Conditions: 0=baseline, 1=100ms, 2=200ms, 3=5%loss, 4=15%loss, 5=200ms+10%loss"
            exit 0 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Require root for tc and tcpdump
if [[ $EUID -ne 0 ]]; then
    echo "ERROR: This script must be run with sudo (needed for tc qdisc and tcpdump)."
    exit 1
fi

# Verify tc is available
if ! command -v tc &>/dev/null; then
    echo "ERROR: 'tc' command not found. Install iproute2: sudo apt install iproute2"
    exit 1
fi

# ─── Experiment condition matrix ─────────────────────────────────────────────
# Format: "label:delay_ms:loss_pct"
declare -a CONDITION_DEFS=(
    "baseline:0:0"
    "delay_100ms:100:0"
    "delay_200ms:200:0"
    "loss_5pct:0:5"
    "loss_15pct:0:15"
    "delay_200ms_loss_10pct:200:10"
)

# Filter conditions if specified
if [[ -n "$CONDITIONS_FILTER" ]]; then
    IFS=',' read -ra SELECTED <<< "$CONDITIONS_FILTER"
else
    SELECTED=()
    for i in "${!CONDITION_DEFS[@]}"; do
        SELECTED+=("$i")
    done
fi

# Results directory
if [[ -z "$OUTPUT_DIR" ]]; then
    EXPERIMENT_DIR="$SCRIPT_DIR/netem_experiment_$(date +%Y%m%d_%H%M%S)"
else
    if [[ "$OUTPUT_DIR" = /* ]]; then
        EXPERIMENT_DIR="$OUTPUT_DIR"
    else
        EXPERIMENT_DIR="$SCRIPT_DIR/$OUTPUT_DIR"
    fi
fi
mkdir -p "$EXPERIMENT_DIR"

# ─── Safety: always clean up netem on exit ───────────────────────────────────
cleanup_netem() {
    echo ""
    echo "[netem] Removing all netem rules from lo..."
    tc qdisc del dev lo root 2>/dev/null || true
    echo "[netem] Loopback interface restored to normal."
}
trap cleanup_netem EXIT

# ─── Banner ──────────────────────────────────────────────────────────────────
echo "========================================================================"
echo "  PRIoTP Network Emulation Experiment Suite"
echo "========================================================================"
echo "  Date            : $(date --iso-8601=seconds)"
echo "  Duration/cond   : ${DURATION}s"
echo "  Sensors         : $NUM_SENSORS"
echo "  Clients         : $NUM_CLIENTS"
echo "  Conditions      : ${#SELECTED[@]}"
echo "  Output          : $EXPERIMENT_DIR"
echo "========================================================================"
echo ""

# ─── Write experiment manifest ───────────────────────────────────────────────
{
    echo "{"
    echo "  \"experiment\": \"PRIoTP NetEm Experiment\","
    echo "  \"timestamp\": \"$(date --iso-8601=seconds)\","
    echo "  \"duration_per_condition\": $DURATION,"
    echo "  \"num_sensors\": $NUM_SENSORS,"
    echo "  \"num_clients\": $NUM_CLIENTS,"
    echo "  \"conditions\": ["
    first=true
    for idx in "${SELECTED[@]}"; do
        IFS=':' read -r label delay loss <<< "${CONDITION_DEFS[$idx]}"
        if [[ "$first" == true ]]; then first=false; else echo ","; fi
        printf "    {\"index\": %d, \"label\": \"%s\", \"delay_ms\": %s, \"loss_pct\": %s}" \
            "$idx" "$label" "$delay" "$loss"
    done
    echo ""
    echo "  ]"
    echo "}"
} > "$EXPERIMENT_DIR/experiment_manifest.json"

# ─── Run each condition ──────────────────────────────────────────────────────
CONDITION_NUM=0
TOTAL_CONDITIONS=${#SELECTED[@]}
FAILED_CONDITIONS=()

for idx in "${SELECTED[@]}"; do
    CONDITION_NUM=$((CONDITION_NUM + 1))
    IFS=':' read -r LABEL DELAY_MS LOSS_PCT <<< "${CONDITION_DEFS[$idx]}"
    TC_ARGS="none"

    COND_DIR="$EXPERIMENT_DIR/condition_${idx}_${LABEL}"
    mkdir -p "$COND_DIR"

    echo ""
    echo "════════════════════════════════════════════════════════════════════════"
    echo "  Condition $CONDITION_NUM/$TOTAL_CONDITIONS: $LABEL"
    echo "    Delay: ${DELAY_MS}ms    Loss: ${LOSS_PCT}%"
    echo "════════════════════════════════════════════════════════════════════════"

    # ── Apply netem rules ────────────────────────────────────────────────────
    # First remove any existing rules
    tc qdisc del dev lo root 2>/dev/null || true

    if [[ "$DELAY_MS" -gt 0 ]] || [[ "$LOSS_PCT" -gt 0 ]]; then
        TC_ARGS="netem"
        if [[ "$DELAY_MS" -gt 0 ]]; then
            # Add jitter of 10% of delay for realism
            JITTER=$((DELAY_MS / 10))
            TC_ARGS="$TC_ARGS delay ${DELAY_MS}ms ${JITTER}ms distribution normal"
        fi
        if [[ "$LOSS_PCT" -gt 0 ]]; then
            TC_ARGS="$TC_ARGS loss ${LOSS_PCT}%"
        fi
        echo "  [tc] tc qdisc add dev lo root $TC_ARGS"
        tc qdisc add dev lo root $TC_ARGS
    else
        echo "  [tc] No impairment (baseline)"
    fi

    # Verify netem is applied
    echo "  [tc] Current qdisc:"
    tc qdisc show dev lo | head -3
    echo ""

    # Record direct kernel-level RTT sanity check for this condition.
    # This validates that netem impairment is actually active even if
    # protocol-internal RTT estimators remain in LOW state.
    set +e
    ping -c 10 -i 0.1 127.0.0.1 > "$COND_DIR/netem_ping.txt" 2>&1
    PING_RC=$?
    set -e
    if [[ "$PING_RC" -eq 0 ]]; then
        echo "  [OK] netem_ping.txt captured"
    else
        echo "  [WARN] ping sanity check failed for condition '$LABEL'"
    fi

    # ── Run the benchmark for this condition ─────────────────────────────────
    # Use the existing benchmark script with --output-dir pointed at our condition dir
    set +e
    "$SCRIPT_DIR/run_localhost_benchmark.sh" \
        --duration "$DURATION" \
        --num-sensors "$NUM_SENSORS" \
        --num-clients "$NUM_CLIENTS" \
        --skip-baseline \
        --output-dir "$COND_DIR" \
        2>&1 | tee "$COND_DIR.log"
    BENCH_RC=${PIPESTATUS[0]}
    set -e

    if [[ "$BENCH_RC" -ne 0 ]]; then
        echo "  [WARN] Condition '$LABEL' benchmark failed with exit code $BENCH_RC"
        FAILED_CONDITIONS+=("$idx:$LABEL:$BENCH_RC")
    else
        echo "  [OK] Condition '$LABEL' benchmark completed"
    fi

    # ── Record the netem config in the condition results ─────────────────────
    cat > "$COND_DIR/netem_config.json" <<NETEM_EOF
{
    "condition_index": $idx,
    "label": "$LABEL",
    "delay_ms": $DELAY_MS,
    "loss_pct": $LOSS_PCT,
    "tc_command": "tc qdisc add dev lo root $TC_ARGS"
}
NETEM_EOF

    # ── Remove netem rules between conditions ────────────────────────────────
    tc qdisc del dev lo root 2>/dev/null || true
    echo "  [tc] Netem removed. Cooling down 5s..."
    sleep 5
done

# ─── Final cleanup ──────────────────────────────────────────────────────────
tc qdisc del dev lo root 2>/dev/null || true

# Fix ownership: sudo creates root-owned files; hand back to the invoking user
if [[ -n "${SUDO_USER:-}" ]]; then
    chown -R "$SUDO_USER:$SUDO_USER" "$EXPERIMENT_DIR"
fi

echo ""
echo "========================================================================"
echo "  All Conditions Complete"
echo "========================================================================"
echo "  Results: $EXPERIMENT_DIR"
if [[ "${#FAILED_CONDITIONS[@]}" -gt 0 ]]; then
    echo ""
    echo "  Failed conditions:"
    for item in "${FAILED_CONDITIONS[@]}"; do
        IFS=':' read -r failed_idx failed_label failed_rc <<< "$item"
        echo "    condition_${failed_idx}_${failed_label} (exit=$failed_rc)"
    done
fi
echo ""
echo "  Condition directories:"
for idx in "${SELECTED[@]}"; do
    IFS=':' read -r label delay loss <<< "${CONDITION_DEFS[$idx]}"
    echo "    condition_${idx}_${label}/"
done
echo ""
echo "  Next steps:"
echo "    python3 plot_netem_results.py $EXPERIMENT_DIR"
echo "========================================================================"
