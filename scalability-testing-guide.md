# PRTP Scalability Testing Guide (1000+ Clients)

## Problem with Current Approach
Your current launcher spawns individual Python processes for each client. Issues:
- **Process overhead**: Each Python process consumes ~10-100MB memory
- **File descriptor limits**: Default Linux limit is ~1024 FDs per process
- **Context switching**: 1000 processes causes excessive CPU overhead
- **Maximum practical limit**: ~100-200 clients per single launcher machine

---

## Recommended Approaches

### Option 1: Threaded Client Simulator (Fastest to Implement)
**Pros**: Low overhead, flexible, easy to measure
**Cons**: Single machine limitation, Python GIL limits true parallelism

Create a Python script that spawns client connections as threads instead of processes:

```python
# scalability_test.py
import threading
import socket
import time
from concurrent.futures import ThreadPoolExecutor
import argparse

def client_thread(client_id, server_ip, server_port, duration):
    """Simulated client that connects and sends updates"""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        server_addr = (server_ip, int(server_port))
        
        start_time = time.time()
        update_count = 0
        
        while time.time() - start_time < duration:
            # Send PRTP UPDATE message (simulate sensor data)
            msg = f"UPDATE_client_{client_id}_seq_{update_count}"
            sock.sendto(msg.encode('utf-8'), server_addr)
            update_count += 1
            time.sleep(0.1)  # Send every 100ms
        
        sock.close()
        print(f"[Client {client_id}] Completed {update_count} updates")
    except Exception as e:
        print(f"[Client {client_id}] Error: {e}")

def main():
    parser = argparse.ArgumentParser(description='PRTP Multi-Client Scalability Test')
    parser.add_argument('--server-ip', default='localhost', help='Server IP')
    parser.add_argument('--server-port', default='5000', help='Server port')
    parser.add_argument('--num-clients', type=int, default=100, help='Number of concurrent clients')
    parser.add_argument('--duration', type=int, default=60, help='Test duration in seconds')
    parser.add_argument('--max-workers', type=int, default=100, help='Thread pool size')
    
    args = parser.parse_args()
    
    print(f"Starting {args.num_clients} clients for {args.duration}s...")
    start = time.time()
    
    with ThreadPoolExecutor(max_workers=args.max_workers) as executor:
        futures = [
            executor.submit(client_thread, i, args.server_ip, args.server_port, args.duration)
            for i in range(args.num_clients)
        ]
        
        # Wait for all to complete
        for future in futures:
            future.result()
    
    elapsed = time.time() - start
    print(f"Test completed in {elapsed:.2f}s")
    print(f"Throughput: {args.num_clients / elapsed:.2f} clients/sec")

if __name__ == '__main__':
    main()
```

**Test it**:
```bash
python3 scalability_test.py --server-ip localhost --server-port 5000 --num-clients 1000 --duration 60
```

---

### Option 2: C-based Load Generator (Best Performance)
**Pros**: Native performance, can handle 10,000+ concurrent clients
**Cons**: More complex to implement

A compiled C program using non-blocking I/O (`select`, `epoll`, or `libuv`) can simulate thousands of concurrent clients from a single machine. This is ideal for stress testing.

Basic pattern:
```c
// Load generator using epoll for 1000s of concurrent connections
// Use libuv or raw epoll to manage concurrent sockets efficiently
```

---

### Option 3: Distributed Load Testing (Production-Grade)
**Pros**: Realistic conditions, can exceed 10,000+ clients
**Cons**: Requires setup on multiple machines

#### A. Using Apache JMeter with PRTP Plugin
- Create JMeter test plan for your UDP/PRTP protocol
- Configure distributed testing across 10+ machines
- Each machine spawns 100+ concurrent virtual users

#### B. Using Locust (Python-based, distributed)
```python
# locustfile.py
from locust import HttpUser, task, between
# Adapt for UDP/PRTP protocol
```

#### C. Container-based Approach (Kubernetes)
Deploy client containers in Kubernetes:
- Each pod runs a lightweight client (10-100 clients per pod)
- Scale horizontally: `kubectl scale deployment prtp-client --replicas 100`
- Pods interact with your server on a shared network

---

### Option 4: Protocol Simulation with Realistic Message Generation
**Modify your test to include**:

1. **Mixed message types**: LIST, SUBSCRIBE, UPDATE, SUBSCRIBE_ACK
2. **Realistic data patterns**: Variable inter-arrival times, payload sizes
3. **Subscription management**: Some clients subscribe, some unsubscribe
4. **Failure scenarios**: Packet loss, timeouts, retries

Example enhanced test:

```python
def realistic_client_thread(client_id, server_ip, server_port, duration):
    """More realistic client with subscription lifecycle"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_addr = (server_ip, int(server_port))
    
    # Phase 1: Send SUBSCRIBE (10% of message volume)
    subscribe_msg = f"SUBSCRIBE_client_{client_id}"
    sock.sendto(subscribe_msg.encode(), server_addr)
    time.sleep(random.uniform(0.05, 0.5))
    
    # Phase 2: Send UPDATEs continuously (90% of message volume)
    start = time.time()
    update_count = 0
    while time.time() - start < duration:
        payload = f"UPDATE_client_{client_id}_seq_{update_count}_" + "X" * random.randint(50, 500)
        sock.sendto(payload.encode(), server_addr)
        update_count += 1
        time.sleep(random.expovariate(1/0.1))  # Exponential distribution
    
    # Phase 3: Unsubscribe
    unsub_msg = f"UNSUBSCRIBE_client_{client_id}"
    sock.sendto(unsub_msg.encode(), server_addr)
    sock.close()
```

---

## System Tuning for High Scale

### 1. Increase File Descriptors
```bash
# Check current limit
ulimit -n

# Temporarily increase
ulimit -n 100000

# Permanently (edit /etc/security/limits.conf)
*    soft    nofile    1000000
*    hard    nofile    1000000
```

### 2. Network Buffer Settings
```bash
# Increase UDP buffer sizes
sysctl -w net.core.rmem_max=134217728
sysctl -w net.core.wmem_max=134217728
sysctl -w net.ipv4.udp_mem="134217728 134217728 134217728"
```

### 3. Server-side Tuning
- Increase `SO_RCVBUF` socket buffer size
- Use non-blocking I/O (epoll/select) instead of blocking calls
- Consider multiple threads/processes on server handling different client ranges
- Profile with `htop`, `iotop`, `netstat` to identify bottlenecks

### 4. Network Interface Tuning
```bash
# Enable multiqueue NIC (if supported)
ethtool -L eth0 combined 16

# Increase RX ring size
ethtool -G eth0 rx 4096
```

---

## Metrics to Collect

Create a monitoring framework:

```python
# Metrics to track during load test
metrics = {
    'total_messages_sent': 0,
    'total_messages_received': 0,
    'connection_errors': 0,
    'min_latency_ms': float('inf'),
    'max_latency_ms': 0,
    'avg_latency_ms': 0,
    'messages_per_second': 0,
    'client_success_rate': 0.0,
    'server_cpu_usage': 0.0,
    'server_memory_usage': 0.0,
}
```

---

## Test Execution Plan

### Phase 1: Baseline (4 Clients - Current)
```bash
python3 launcher/sensor-launcher.py localhost 5000 60 4
# Collect: latency, throughput, server resource usage
```

### Phase 2: Linear Scaling (10, 50, 100 Clients)
```bash
python3 scalability_test.py --num-clients 10 --duration 60
python3 scalability_test.py --num-clients 50 --duration 60
python3 scalability_test.py --num-clients 100 --duration 60
# Graph: Latency vs Client Count, Throughput vs Client Count
```

### Phase 3: Break-Point Testing (200, 500, 1000 Clients)
```bash
python3 scalability_test.py --num-clients 200 --duration 60
python3 scalability_test.py --num-clients 500 --duration 60
python3 scalability_test.py --num-clients 1000 --duration 60
# Identify max sustainable load
```

### Phase 4: Stress Testing (Exceed Limits)
```bash
# Push beyond expected limits to find failure modes
python3 scalability_test.py --num-clients 2000 --duration 30
```

---

## Expected Results

Document in your evaluation:
1. **Scalability curve**: How does throughput/latency change with client count?
2. **Maximum load**: At what point does server performance degrade?
3. **Bottleneck identification**: CPU? Memory? Network? I/O?
4. **Connection limits**: Max concurrent clients before failure
5. **Message loss**: Any dropped messages at scale?
6. **Comparison with state-of-the-art**: How does PRTP scale vs. MQTT, CoAP, AMQP?

---

## Tools & Resources

| Tool | Use Case | Scale |
|------|----------|-------|
| Custom Python threads | Quick testing, <1000 clients | Single machine |
| C + epoll/libuv | High performance, <10K clients | Single machine |
| Apache JMeter | Protocol simulation, metrics | Distributed |
| Locust | Real load testing, reporting | Distributed (Python) |
| Gatling | High-performance, reports | Distributed |
| wrk / wrk2 | HTTP benchmarking base | Single machine |
| Kubernetes + containers | Production-like, 10K+ clients | Multi-machine |

