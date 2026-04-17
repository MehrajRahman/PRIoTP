#!/bin/bash

CLIENTS=50
CLIENT_DIR="$HOME/CS/PRIoTP/PRTP/application"
SERVER_IP="127.0.0.1"
PORT=5005

CPU_LOG="$HOME/CS/PRIoTP/CPU_log"
RAM_LOG="$HOME/CS/PRIoTP/RAM_log"

CHECK_INTERVAL=1        # seconds between samples
TIMEOUT=10              # seconds of inactivity before killing

cd "$CLIENT_DIR" || exit 1

# Clean old logs
> "$CPU_LOG"
> "$RAM_LOG"

echo "Starting clients..."

# Launch clients
for ((i=1; i<=CLIENTS; i++))
do
  ./PRTP_client -l./client${i}_sensor_log -s$SERVER_IP -rtemp_${i} -p$PORT -A > client${i}.out 2>&1 &
  PIDS[$i]=$!
  echo "Launched client $i (PID: ${PIDS[$i]})"
done

echo "Starting monitoring..."

last_activity=$(date +%s)

monitor() {
  while true
  do
    timestamp=$(date "+%Y-%m-%d %H:%M:%S")

    # CPU usage (total)
    cpu=$(top -bn1 | grep "Cpu(s)" | awk '{print 100 - $8}')

    # RAM usage (%)
    ram=$(free | awk '/Mem:/ {printf("%.2f"), $3/$2 * 100.0}')

    echo "$timestamp CPU: $cpu%" >> "$CPU_LOG"
    echo "$timestamp RAM: $ram%" >> "$RAM_LOG"

    # Check if any client produced output recently
    recent_output=$(find . -name "client*.out" -mmin -0.1)

    if [ -n "$recent_output" ]; then
      last_activity=$(date +%s)
    fi

    now=$(date +%s)
    elapsed=$((now - last_activity))

    if [ "$elapsed" -ge "$TIMEOUT" ]; then
      echo "No activity for $TIMEOUT seconds. Killing clients..."
      kill "${PIDS[@]}" 2>/dev/null
      break
    fi

    sleep "$CHECK_INTERVAL"
  done
}

monitor &

MONITOR_PID=$!

# Wait for clients
wait "${PIDS[@]}"

# Stop monitor after clients finish
kill "$MONITOR_PID" 2>/dev/null

echo "All clients finished"