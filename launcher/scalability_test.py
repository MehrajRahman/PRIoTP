#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PRTP Scalability Testing Tool
Simulates 1000+ concurrent clients sending PRTP messages to the server
"""

import threading
import socket
import time
import argparse
import sys
import random
from concurrent.futures import ThreadPoolExecutor
from collections import defaultdict
import signal

# Global metrics
metrics = {
    'total_messages_sent': 0,
    'total_messages_received': 0,
    'connection_errors': 0,
    'client_errors': defaultdict(int),
    'start_time': None,
    'latencies': [],
    'lock': threading.Lock(),
}

class PRTPClientSimulator:
    """Simulates a single PRTP client"""
    
    def __init__(self, client_id, server_ip, server_port):
        self.client_id = client_id
        self.server_ip = server_ip
        self.server_port = int(server_port)
        self.sock = None
        self.messages_sent = 0
        self.messages_failed = 0
        
    def connect(self):
        """Create UDP socket (no actual connection for UDP)"""
        try:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            # Set non-blocking with timeout
            self.sock.settimeout(5.0)
            return True
        except Exception as e:
            with metrics['lock']:
                metrics['connection_errors'] += 1
            return False
    
    def send_subscribe(self):
        """Send SUBSCRIBE message"""
        try:
            sensor_id = f"sensor_{self.client_id % 100:03d}"
            msg = f"SUBSCRIBE|{sensor_id}|reliable=1"
            
            send_time = time.time()
            self.sock.sendto(msg.encode('utf-8'), (self.server_ip, self.server_port))
            
            with metrics['lock']:
                metrics['total_messages_sent'] += 1
            
            self.messages_sent += 1
            return True
        except Exception as e:
            with metrics['lock']:
                metrics['client_errors'][f'subscribe_error'] += 1
            self.messages_failed += 1
            return False
    
    def send_update(self, seq_no):
        """Send UPDATE message with simulated sensor data"""
        try:
            sensor_type = ['temp', 'device', 'gps', 'camera'][self.client_id % 4]
            sensor_id = f"sensor_{self.client_id % 100:03d}"
            payload_size = random.randint(50, 500)  # Variable payload
            payload = 'X' * payload_size
            
            msg = f"UPDATE|{sensor_id}|seqno={seq_no}|type={sensor_type}|data={payload}"
            
            send_time = time.time()
            self.sock.sendto(msg.encode('utf-8'), (self.server_ip, self.server_port))
            
            with metrics['lock']:
                metrics['total_messages_sent'] += 1
            
            self.messages_sent += 1
            return True
        except Exception as e:
            with metrics['lock']:
                metrics['client_errors'][f'update_error'] += 1
            self.messages_failed += 1
            return False
    
    def run(self, duration):
        """Run the client for specified duration"""
        if not self.connect():
            return
        
        # Send initial SUBSCRIBE
        self.send_subscribe()
        time.sleep(random.uniform(0.01, 0.1))
        
        # Send periodic UPDATEs
        start_time = time.time()
        seq_no = 0
        
        while time.time() - start_time < duration:
            # Send update with exponential backoff
            self.send_update(seq_no)
            seq_no += 1
            
            # Random inter-arrival time (poisson-like)
            inter_arrival = random.expovariate(1.0 / 0.2)  # Mean 200ms
            time.sleep(inter_arrival)
        
        # Clean up
        try:
            self.sock.close()
        except:
            pass


def client_thread_wrapper(client_id, server_ip, server_port, duration):
    """Wrapper to run a client simulator in a thread"""
    client = PRTPClientSimulator(client_id, server_ip, server_port)
    client.run(duration)
    return client.messages_sent


def monitor_thread(num_clients, duration):
    """Monitor and print real-time metrics"""
    start = time.time()
    last_count = 0
    
    while time.time() - start < duration + 5:
        time.sleep(5)
        
        with metrics['lock']:
            elapsed = time.time() - metrics['start_time']
            total_sent = metrics['total_messages_sent']
            throughput = (total_sent - last_count) / 5.0  # msgs/sec
            last_count = total_sent
        
        print(f"\n[{elapsed:.1f}s] Status:")
        print(f"  Messages sent: {total_sent}")
        print(f"  Throughput: {throughput:.1f} msgs/sec")
        print(f"  Connection errors: {metrics['connection_errors']}")
        if metrics['client_errors']:
            print(f"  Client errors: {dict(metrics['client_errors'])}")


def main():
    parser = argparse.ArgumentParser(
        description='PRTP Scalability Load Generator',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Test with 100 clients for 60 seconds
  ./scalability_test.py --num-clients 100 --duration 60
  
  # Test with 1000 clients against remote server
  ./scalability_test.py --server-ip 192.168.1.100 --server-port 5000 \\
                        --num-clients 1000 --duration 120
  
  # Stress test with 2000 clients
  ./scalability_test.py --num-clients 2000 --duration 30
        """
    )
    
    parser.add_argument('--server-ip', default='localhost',
                       help='Server IP address (default: localhost)')
    parser.add_argument('--server-port', default='5000',
                       help='Server port (default: 5000)')
    parser.add_argument('--num-clients', type=int, default=100,
                       help='Number of concurrent clients (default: 100)')
    parser.add_argument('--duration', type=int, default=60,
                       help='Test duration in seconds (default: 60)')
    parser.add_argument('--max-workers', type=int, default=None,
                       help='Max worker threads (default: min(32, num_clients))')
    parser.add_argument('--warmup', type=int, default=5,
                       help='Warmup time before starting metrics (seconds)')
    parser.add_argument('--no-monitor', action='store_true',
                       help='Disable real-time monitoring')
    
    args = parser.parse_args()
    
    # Validate arguments
    if args.num_clients < 1:
        print("Error: num-clients must be >= 1", file=sys.stderr)
        sys.exit(1)
    
    if args.duration < 1:
        print("Error: duration must be >= 1", file=sys.stderr)
        sys.exit(1)
    
    max_workers = args.max_workers or min(32, args.num_clients)
    
    print("=" * 70)
    print("PRTP Scalability Load Generator")
    print("=" * 70)
    print(f"Server: {args.server_ip}:{args.server_port}")
    print(f"Clients: {args.num_clients}")
    print(f"Duration: {args.duration}s")
    print(f"Worker threads: {max_workers}")
    print("=" * 70)
    print()
    
    # Initialize metrics
    metrics['start_time'] = time.time()
    
    # Handle Ctrl+C gracefully
    def signal_handler(sig, frame):
        print("\n\nTest interrupted by user")
        sys.exit(0)
    
    signal.signal(signal.SIGINT, signal_handler)
    
    # Start monitor thread if enabled
    monitor = None
    if not args.no_monitor:
        monitor = threading.Thread(
            target=monitor_thread,
            args=(args.num_clients, args.duration),
            daemon=True
        )
        monitor.start()
        print(f"[Monitor] Metrics logged every 5 seconds")
        print()
    
    print(f"Starting {args.num_clients} clients...")
    start_time = time.time()
    
    try:
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = [
                executor.submit(
                    client_thread_wrapper,
                    i,
                    args.server_ip,
                    args.server_port,
                    args.duration
                )
                for i in range(args.num_clients)
            ]
            
            # Wait for all clients to complete
            completed = 0
            for future in futures:
                try:
                    future.result()
                    completed += 1
                except Exception as e:
                    print(f"Client error: {e}", file=sys.stderr)
            
            print(f"\nAll {completed}/{args.num_clients} clients completed")
    
    except KeyboardInterrupt:
        print("\nTest interrupted")
        sys.exit(1)
    
    elapsed = time.time() - start_time
    
    # Print final statistics
    print()
    print("=" * 70)
    print("FINAL STATISTICS")
    print("=" * 70)
    print(f"Test duration: {elapsed:.2f} seconds")
    print(f"Total messages sent: {metrics['total_messages_sent']}")
    print(f"Messages per second: {metrics['total_messages_sent'] / elapsed:.2f}")
    print(f"Connection errors: {metrics['connection_errors']}")
    
    if metrics['client_errors']:
        print(f"Client errors:")
        for error_type, count in sorted(metrics['client_errors'].items()):
            print(f"  - {error_type}: {count}")
    
    success_rate = ((metrics['total_messages_sent'] - metrics['connection_errors']) 
                    / max(1, metrics['total_messages_sent']) * 100)
    print(f"Success rate: {success_rate:.1f}%")
    print("=" * 70)


if __name__ == '__main__':
    main()
