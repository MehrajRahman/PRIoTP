#!/bin/bash
# Run systematic scalability tests with progressively increasing client counts

set -e

# Configuration
SERVER_IP="${1:-localhost}"
SERVER_PORT="${2:-5000}"
TEST_DURATION=60
RESULTS_DIR="scalability_results_$(date +%Y%m%d_%H%M%S)"

# Client counts for each phase
PHASE_1_COUNTS=(10 50)          # Small scale
PHASE_2_COUNTS=(100 200 500)    # Medium scale  
PHASE_3_COUNTS=(1000 2000)      # High scale (stress)

# Create results directory
mkdir -p "$RESULTS_DIR"

echo "========================================================================"
echo "PRTP Scalability Testing Suite"
echo "========================================================================"
echo "Server: $SERVER_IP:$SERVER_PORT"
echo "Test duration: $TEST_DURATION seconds"
echo "Results directory: $RESULTS_DIR"
echo "========================================================================"
echo ""

# Function to run a single test
run_test() {
    local num_clients=$1
    local test_name="${SERVER_IP}_${num_clients}clients"
    local results_file="$RESULTS_DIR/${test_name}_${TEST_DURATION}s.txt"
    
    echo "---"
    echo "Starting test: $num_clients clients"
    echo "Results: $results_file"
    echo ""
    
    # Run the scalability test and capture output
    python3 scalability_test.py \
        --server-ip "$SERVER_IP" \
        --server-port "$SERVER_PORT" \
        --num-clients "$num_clients" \
        --duration "$TEST_DURATION" \
        2>&1 | tee "$results_file"
    
    echo ""
    echo "Completed: $num_clients clients"
    echo ""
    
    # Save server metrics if available
    if command -v pidstat &> /dev/null; then
        echo "Server metrics:" >> "$results_file"
        top -b -n 1 | grep -E "^top|load|cpu|mem" >> "$results_file" 2>/dev/null || true
    fi
}

# Function to collect server metrics
collect_server_metrics() {
    echo "Collecting baseline server metrics..."
    
    local metrics_file="$RESULTS_DIR/server_baseline.txt"
    {
        echo "=== Server Baseline Metrics ==="
        echo "Timestamp: $(date)"
        echo ""
        
        echo "System Info:"
        uname -a
        echo ""
        
        echo "File descriptor limit:"
        ulimit -n
        echo ""
        
        echo "Network buffer settings:"
        sysctl net.core.rmem_max 2>/dev/null || echo "N/A"
        sysctl net.core.wmem_max 2>/dev/null || echo "N/A"
        echo ""
        
        echo "CPU Info:"
        grep -c "^processor" /proc/cpuinfo 2>/dev/null || sysctl hw.ncpu 2>/dev/null || echo "N/A"
        echo ""
        
        echo "Memory Info:"
        free -h 2>/dev/null || vm_stat
        echo ""
        
    } | tee "$metrics_file"
}

# Main test execution

# Baseline
collect_server_metrics

# Phase 1: Small scale tests
echo ""
echo "======== PHASE 1: Small Scale Tests (10-50 clients) ========"
echo ""
for count in "${PHASE_1_COUNTS[@]}"; do
    run_test "$count"
    sleep 2  # Cool down between tests
done

# Phase 2: Medium scale tests
echo ""
echo "======== PHASE 2: Medium Scale Tests (100-500 clients) ========"
echo ""
for count in "${PHASE_2_COUNTS[@]}"; do
    run_test "$count"
    sleep 2  # Cool down between tests
done

# Phase 3: High scale / stress tests
echo ""
echo "======== PHASE 3: High Scale Tests (1000+ clients) ========"
echo ""
echo "WARNING: These tests push server to limits"
echo "Press Ctrl+C to skip Phase 3"
echo ""
sleep 3

for count in "${PHASE_3_COUNTS[@]}"; do
    run_test "$count"
    sleep 5  # Longer cool down for stress tests
done

# Generate summary report
echo ""
echo "========================================================================"
echo "Test Suite Complete!"
echo "========================================================================"
echo "Results saved to: $RESULTS_DIR"
echo ""
echo "To analyze results:"
echo "  grep 'Messages per second' $RESULTS_DIR/*.txt"
echo "  grep 'Success rate' $RESULTS_DIR/*.txt"
echo ""

# Create a summary CSV
{
    echo "clients,mps,errors,success_rate"
    for file in "$RESULTS_DIR"/*.txt; do
        if [[ "$file" == *"baseline"* ]]; then
            continue
        fi
        
        clients=$(basename "$file" | grep -oP '(?<=_)\d+(?=clients)' || echo "N/A")
        mps=$(grep "Messages per second" "$file" | tail -1 | awk '{print $NF}' || echo "N/A")
        errors=$(grep "Connection errors:" "$file" | tail -1 | awk '{print $NF}' || echo "0")
        
        echo "$clients,$mps,$errors,N/A"
    done
} > "$RESULTS_DIR/summary.csv"

echo "Summary CSV: $RESULTS_DIR/summary.csv"
echo ""
