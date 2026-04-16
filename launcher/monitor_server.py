#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Server-side monitoring script for PRTP scalability tests
Collects CPU, memory, network, and I/O metrics during load tests
"""

import psutil
import time
import argparse
import sys
import json
from datetime import datetime
from collections import deque

class ServerMonitor:
    """Monitor server resource usage during load tests"""
    
    def __init__(self, output_file=None, interval=1):
        self.output_file = output_file
        self.interval = interval
        self.metrics_history = deque(maxlen=10000)  # Keep last 10k samples
        self.start_time = time.time()
        
    def get_metrics(self):
        """Collect current system metrics"""
        timestamp = time.time()
        elapsed = timestamp - self.start_time
        
        # CPU metrics
        cpu_percent = psutil.cpu_percent(interval=0.1)
        cpu_count = psutil.cpu_count()
        
        # Memory metrics
        memory = psutil.virtual_memory()
        
        # Network metrics
        net_io = psutil.net_io_counters()
        
        # Disk I/O
        disk_io = psutil.disk_io_counters()
        
        # Process list (for any PRTP processes)
        prtp_processes = []
        for proc in psutil.process_iter(['pid', 'name', 'memory_percent', 'cpu_num']):
            try:
                if 'prtp' in proc.info['name'].lower() or 'server' in proc.info['name'].lower():
                    prtp_processes.append({
                        'pid': proc.info['pid'],
                        'name': proc.info['name'],
                        'memory%': proc.info['memory_percent'],
                    })
            except (psutil.NoSuchProcess, psutil.AccessDenied):
                pass
        
        metrics = {
            'timestamp': timestamp,
            'elapsed_seconds': elapsed,
            'cpu': {
                'percent': cpu_percent,
                'count': cpu_count,
                'avg_load': sum(psutil.getloadavg()) / 3,
            },
            'memory': {
                'percent': memory.percent,
                'used_mb': memory.used / (1024 * 1024),
                'available_mb': memory.available / (1024 * 1024),
                'total_mb': memory.total / (1024 * 1024),
            },
            'network': {
                'bytes_sent': net_io.bytes_sent,
                'bytes_recv': net_io.bytes_recv,
                'packets_sent': net_io.packets_sent,
                'packets_recv': net_io.packets_recv,
            },
            'disk_io': {
                'read_bytes': disk_io.read_bytes,
                'write_bytes': disk_io.write_bytes,
            },
            'prtp_processes': prtp_processes,
        }
        
        return metrics
    
    def run(self, duration=None):
        """Monitor continuously"""
        print("=" * 80)
        print("PRTP Server Monitor")
        print("=" * 80)
        print(f"Start time: {datetime.now()}")
        print(f"Sample interval: {self.interval}s")
        if duration:
            print(f"Duration: {duration}s")
        print()
        
        last_metrics = None
        sample_count = 0
        
        try:
            while True:
                metrics = self.get_metrics()
                self.metrics_history.append(metrics)
                sample_count += 1
                
                # Calculate deltas
                if last_metrics:
                    bytes_sent_delta = metrics['network']['bytes_sent'] - last_metrics['network']['bytes_sent']
                    bytes_recv_delta = metrics['network']['bytes_recv'] - last_metrics['network']['bytes_recv']
                    packets_sent_delta = metrics['network']['packets_sent'] - last_metrics['network']['packets_sent']
                    packets_recv_delta = metrics['network']['packets_recv'] - last_metrics['network']['packets_recv']
                    
                    throughput_mbps = (bytes_recv_delta / self.interval) / (1024 * 1024) * 8
                    packets_per_sec = (packets_recv_delta / self.interval)
                else:
                    throughput_mbps = 0
                    packets_per_sec = 0
                
                # Print formatted output
                self._print_metrics(metrics, throughput_mbps, packets_per_sec)
                
                last_metrics = metrics
                
                # Check duration
                if duration and metrics['elapsed_seconds'] > duration:
                    print("\nDuration complete")
                    break
                
                time.sleep(self.interval)
        
        except KeyboardInterrupt:
            print("\n\nMonitoring stopped")
        
        # Print summary
        self._print_summary()
    
    def _print_metrics(self, metrics, throughput_mbps, packets_per_sec):
        """Pretty print metrics"""
        elapsed = metrics['elapsed_seconds']
        
        # Color codes (if terminal supports)
        try:
            colors = {
                'header': '\033[95m',
                'okblue': '\033[94m',
                'okcyan': '\033[96m',
                'okgreen': '\033[92m',
                'warning': '\033[93m',
                'fail': '\033[91m',
                'end': '\033[0m',
                'bold': '\033[1m',
            }
        except:
            colors = {k: '' for k in ['header', 'okblue', 'okcyan', 'okgreen', 'warning', 'fail', 'end', 'bold']}
        
        output = f"\r[{elapsed:7.1f}s] "
        
        # CPU
        cpu = metrics['cpu']['percent']
        cpu_color = colors['fail'] if cpu > 80 else (colors['warning'] if cpu > 50 else colors['okgreen'])
        output += f"CPU: {cpu_color}{cpu:5.1f}%{colors['end']} | "
        
        # Memory
        mem = metrics['memory']['percent']
        mem_color = colors['fail'] if mem > 80 else (colors['warning'] if mem > 50 else colors['okgreen'])
        output += f"MEM: {mem_color}{mem:5.1f}%{colors['end']} ({metrics['memory']['used_mb']:.0f}MB) | "
        
        # Network
        output += f"NET: {throughput_mbps:7.1f}Mbps | PKT: {packets_per_sec:7.0f}pkt/s"
        
        # PRTP processes
        if metrics['prtp_processes']:
            output += f" | PRTP: {len(metrics['prtp_processes'])} proc(s)"
        
        print(output, end='', flush=True)
    
    def _print_summary(self):
        """Print test summary statistics"""
        if not self.metrics_history:
            return
        
        print("\n\n" + "=" * 80)
        print("SUMMARY STATISTICS")
        print("=" * 80)
        
        # CPU stats
        cpu_values = [m['cpu']['percent'] for m in self.metrics_history]
        print(f"\nCPU Usage:")
        print(f"  Min: {min(cpu_values):.1f}%")
        print(f"  Max: {max(cpu_values):.1f}%")
        print(f"  Avg: {sum(cpu_values) / len(cpu_values):.1f}%")
        
        # Memory stats
        mem_values = [m['memory']['percent'] for m in self.metrics_history]
        print(f"\nMemory Usage:")
        print(f"  Min: {min(mem_values):.1f}%")
        print(f"  Max: {max(mem_values):.1f}%")
        print(f"  Avg: {sum(mem_values) / len(mem_values):.1f}%")
        
        # Network stats
        first_net = self.metrics_history[0]['network']
        last_net = self.metrics_history[-1]['network']
        total_bytes_recv = last_net['bytes_recv'] - first_net['bytes_recv']
        total_packets_recv = last_net['packets_recv'] - first_net['packets_recv']
        
        print(f"\nNetwork (Total):")
        print(f"  Bytes received: {total_bytes_recv / (1024**2):.1f} MB")
        print(f"  Packets received: {total_packets_recv}")
        print(f"  Avg packet size: {(total_bytes_recv / max(1, total_packets_recv)):.1f} bytes")
        
        elapsed = self.metrics_history[-1]['elapsed_seconds'] - self.metrics_history[0]['elapsed_seconds']
        if elapsed > 0:
            avg_throughput = (total_bytes_recv / elapsed) / (1024**2) * 8
            print(f"  Avg throughput: {avg_throughput:.1f} Mbps")
        
        print("=" * 80)


def main():
    parser = argparse.ArgumentParser(
        description='Monitor PRTP server during scalability tests'
    )
    parser.add_argument('--output', '-o', help='Output JSON file for metrics')
    parser.add_argument('--interval', '-i', type=float, default=1.0,
                       help='Sample interval in seconds (default: 1.0)')
    parser.add_argument('--duration', '-d', type=int, default=None,
                       help='Monitor duration in seconds (default: infinite)')
    
    args = parser.parse_args()
    
    monitor = ServerMonitor(output_file=args.output, interval=args.interval)
    monitor.run(duration=args.duration)


if __name__ == '__main__':
    main()
