#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PRIoTP Localhost Benchmark — Post-Experiment Analysis
=====================================================
Reads all artefacts produced by run_localhost_benchmark.sh and computes
research-grade KPIs: throughput, latency, CPU efficiency, protocol overhead,
error rates, and socket buffer utilisation.

Outputs:
  - Console tables suitable for inclusion in LaTeX/academic reports
  - CSV files for further processing in R, pandas, or pgfplots

Usage:
    python3 analyze_localhost_results.py <results_dir>

Dependencies:
    - Python 3.6+  (standard library only; matplotlib optional for figures)
"""

import argparse
import csv
import json
import math
import os
import re
import statistics
import struct
import sys
from collections import defaultdict, Counter
from pathlib import Path

# Optional: matplotlib for figures
try:
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False


# =============================================================================
# Utility
# =============================================================================

def safe_stat(values, label="values"):
    """Return dict of descriptive statistics. Empty-safe."""
    if not values:
        return {"n": 0, "mean": float('nan'), "median": float('nan'),
                "stdev": float('nan'), "min": float('nan'), "max": float('nan'),
                "p5": float('nan'), "p25": float('nan'),
                "p75": float('nan'), "p95": float('nan'), "p99": float('nan')}
    s = sorted(values)
    n = len(s)
    def percentile(p):
        k = (n - 1) * p / 100.0
        f = math.floor(k)
        c = math.ceil(k)
        if f == c:
            return s[int(k)]
        return s[f] * (c - k) + s[c] * (k - f)

    return {
        "n": n,
        "mean": statistics.mean(s),
        "median": statistics.median(s),
        "stdev": statistics.stdev(s) if n > 1 else 0.0,
        "min": s[0],
        "max": s[-1],
        "p5":  percentile(5),
        "p25": percentile(25),
        "p75": percentile(75),
        "p95": percentile(95),
        "p99": percentile(99),
    }


def fmt(v, decimals=3):
    """Format a numeric value, handling NaN."""
    if isinstance(v, float) and math.isnan(v):
        return "N/A"
    if isinstance(v, float):
        return f"{v:.{decimals}f}"
    return str(v)


def print_table(title, headers, rows, col_widths=None):
    """Print a formatted ASCII table."""
    if col_widths is None:
        col_widths = [max(len(str(h)), max((len(str(r[i])) for r in rows), default=0)) + 2
                      for i, h in enumerate(headers)]
    sep = "+" + "+".join("-" * w for w in col_widths) + "+"

    print(f"\n{'=' * (sum(col_widths) + len(col_widths) + 1)}")
    print(f"  {title}")
    print(f"{'=' * (sum(col_widths) + len(col_widths) + 1)}")
    print(sep)
    print("|" + "|".join(str(h).center(w) for h, w in zip(headers, col_widths)) + "|")
    print(sep)
    for row in rows:
        print("|" + "|".join(str(row[i]).rjust(w - 1) + " " for i, w in enumerate(col_widths)) + "|")
    print(sep)


def write_csv(path, headers, rows):
    """Write rows to CSV."""
    with open(path, 'w', newline='') as f:
        w = csv.writer(f)
        w.writerow(headers)
        w.writerows(rows)


# =============================================================================
# KPI 1: Loopback Latency (ping baseline)
# =============================================================================

def analyse_ping(results_dir):
    """Parse ping_baseline.txt and extract RTT statistics."""
    path = os.path.join(results_dir, "ping_baseline.txt")
    if not os.path.isfile(path):
        print("\n  [SKIP] ping_baseline.txt not found")
        return None

    rtts = []
    summary = {}
    with open(path) as f:
        for line in f:
            # Match individual ping lines: time=X.XX ms
            m = re.search(r'time[=<](\d+\.?\d*)\s*ms', line)
            if m:
                rtts.append(float(m.group(1)))
            # Match summary: min/avg/max/mdev = X/X/X/X ms
            m2 = re.search(r'(\d+\.?\d*)/(\d+\.?\d*)/(\d+\.?\d*)/(\d+\.?\d*)\s*ms', line)
            if m2:
                summary = {
                    "ping_min_ms": float(m2.group(1)),
                    "ping_avg_ms": float(m2.group(2)),
                    "ping_max_ms": float(m2.group(3)),
                    "ping_mdev_ms": float(m2.group(4)),
                }
            # Match packet loss
            m3 = re.search(r'(\d+)% packet loss', line)
            if m3:
                summary["ping_loss_pct"] = float(m3.group(1))

    if rtts:
        stats = safe_stat(rtts, "ping_rtt")
        summary["rtt_stats"] = stats

    print_table(
        "KPI 1: Loopback Latency (ping 127.0.0.1)",
        ["Metric", "Value"],
        [
            ["Packets sent", summary.get("rtt_stats", {}).get("n", "N/A")],
            ["Min RTT (ms)", fmt(summary.get("ping_min_ms", float('nan')))],
            ["Avg RTT (ms)", fmt(summary.get("ping_avg_ms", float('nan')))],
            ["Max RTT (ms)", fmt(summary.get("ping_max_ms", float('nan')))],
            ["Mdev (ms)",    fmt(summary.get("ping_mdev_ms", float('nan')))],
            ["P95 RTT (ms)", fmt(summary.get("rtt_stats", {}).get("p95", float('nan')))],
            ["P99 RTT (ms)", fmt(summary.get("rtt_stats", {}).get("p99", float('nan')))],
            ["Packet loss",  f"{summary.get('ping_loss_pct', 'N/A')}%"],
        ]
    )
    return summary


# =============================================================================
# KPI 2: UDP Throughput Baseline (iperf3)
# =============================================================================

def analyse_iperf3(results_dir):
    """Parse iperf3_udp_baseline.json for throughput and jitter."""
    path = os.path.join(results_dir, "iperf3_udp_baseline.json")
    if not os.path.isfile(path):
        print("\n  [SKIP] iperf3_udp_baseline.json not found (iperf3 not installed)")
        return None

    try:
        with open(path) as f:
            data = json.load(f)
    except (json.JSONDecodeError, IOError) as e:
        print(f"\n  [ERROR] Could not parse iperf3 JSON: {e}")
        return None

    end = data.get("end", {})
    udp_sum = end.get("sum", {})

    result = {
        "bits_per_second": udp_sum.get("bits_per_second", 0),
        "jitter_ms":       udp_sum.get("jitter_ms", 0),
        "lost_packets":    udp_sum.get("lost_packets", 0),
        "packets":         udp_sum.get("packets", 0),
        "lost_percent":    udp_sum.get("lost_percent", 0),
        "bytes":           udp_sum.get("bytes", 0),
        "seconds":         udp_sum.get("seconds", 0),
    }

    throughput_gbps = result["bits_per_second"] / 1e9
    throughput_mbps = result["bits_per_second"] / 1e6

    print_table(
        "KPI 2: Raw UDP Throughput Baseline (iperf3)",
        ["Metric", "Value"],
        [
            ["Throughput (Gbps)",  fmt(throughput_gbps)],
            ["Throughput (Mbps)",  fmt(throughput_mbps, 1)],
            ["Jitter (ms)",        fmt(result["jitter_ms"])],
            ["Total packets",      result["packets"]],
            ["Lost packets",       result["lost_packets"]],
            ["Loss (%)",           fmt(result["lost_percent"], 2)],
            ["Data transferred",   f"{result['bytes'] / (1024**2):.1f} MB"],
            ["Duration (s)",       fmt(result["seconds"], 1)],
        ]
    )
    return result


# =============================================================================
# KPI 3: CPU Utilisation (pidstat)
# =============================================================================

def parse_pidstat(path):
    """Parse pidstat -u -r -d output into per-sample CPU records.

    pidstat interleaves three record types per interval:
      CPU:    HH:MM:SS [AM|PM]  UID  PID  %usr %system %guest %wait %CPU  CPU  Command
      Memory: HH:MM:SS [AM|PM]  UID  PID  minflt/s majflt/s VSZ RSS %MEM  Command
      Disk:   HH:MM:SS [AM|PM]  UID  PID  kB_rd/s kB_wr/s kB_ccwr/s iodelay Command

    We only extract CPU records (lines containing %usr in the preceding header).
    The AM/PM token shifts all column indices by +1 vs 24-hour format.
    """
    if not os.path.isfile(path):
        return []

    records = []
    section = None  # 'cpu', 'mem', 'disk', or None

    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith('Linux') or line.startswith('Average'):
                continue

            # Detect header lines to determine current section
            if '%usr' in line and '%CPU' in line:
                section = 'cpu'
                continue
            elif 'minflt/s' in line or '%MEM' in line:
                section = 'mem'
                continue
            elif 'kB_rd/s' in line or 'iodelay' in line:
                section = 'disk'
                continue
            # Skip header line "Recv-Q" etc.
            if line.startswith('Recv-Q') or line.startswith('#'):
                continue

            if section != 'cpu':
                continue

            parts = line.split()
            if len(parts) < 9:
                continue

            # Determine AM/PM offset: if parts[1] is AM/PM, offset=1
            offset = 1 if len(parts) > 1 and parts[1] in ('AM', 'PM') else 0

            try:
                pid = int(parts[2 + offset])
                usr = float(parts[3 + offset])
                system = float(parts[4 + offset])
                cpu_total = float(parts[7 + offset])
                cmd = parts[-1]
                records.append({
                    "pid": pid,
                    "usr": usr,
                    "system": system,
                    "cpu_pct": cpu_total,
                    "command": cmd,
                })
            except (ValueError, IndexError):
                continue
    return records


def analyse_cpu(results_dir):
    """Analyse pidstat data for server and clients."""
    server_records = parse_pidstat(os.path.join(results_dir, "pidstat_server.csv"))
    client_records = parse_pidstat(os.path.join(results_dir, "pidstat_clients.csv"))

    results = {}

    if server_records:
        cpu_vals = [r["cpu_pct"] for r in server_records]
        usr_vals = [r["usr"] for r in server_records]
        sys_vals = [r["system"] for r in server_records]
        stats = safe_stat(cpu_vals)
        results["server"] = stats
        results["server_usr"] = safe_stat(usr_vals)
        results["server_sys"] = safe_stat(sys_vals)

        print_table(
            "KPI 3a: PRTP_server CPU Utilisation (pidstat)",
            ["Metric", "Value"],
            [
                ["Samples",          stats["n"]],
                ["Mean CPU%",        fmt(stats["mean"], 2)],
                ["Median CPU%",      fmt(stats["median"], 2)],
                ["P95 CPU%",         fmt(stats["p95"], 2)],
                ["Max CPU%",         fmt(stats["max"], 2)],
                ["Mean User%",       fmt(results["server_usr"]["mean"], 2)],
                ["Mean System%",     fmt(results["server_sys"]["mean"], 2)],
            ]
        )
    else:
        print("\n  [SKIP] pidstat_server.csv not found or empty")

    if client_records:
        # Aggregate across all client PIDs
        per_pid = defaultdict(list)
        for r in client_records:
            per_pid[r["pid"]].append(r["cpu_pct"])

        all_cpu = [r["cpu_pct"] for r in client_records]
        stats_all = safe_stat(all_cpu)
        results["clients_aggregate"] = stats_all

        rows = []
        for pid in sorted(per_pid.keys()):
            s = safe_stat(per_pid[pid])
            rows.append([pid, s["n"], fmt(s["mean"], 2), fmt(s["max"], 2)])

        rows.append(["ALL", stats_all["n"], fmt(stats_all["mean"], 2), fmt(stats_all["max"], 2)])

        print_table(
            "KPI 3b: PRTP_client CPU Utilisation (pidstat)",
            ["PID", "Samples", "Mean CPU%", "Max CPU%"],
            rows
        )
    else:
        print("\n  [SKIP] pidstat_clients.csv not found or empty")

    return results


# =============================================================================
# KPI 4: System-wide Metrics (monitor_server.py)
# =============================================================================

def analyse_monitor(results_dir):
    """Parse the monitor_server.py output for throughput time series."""
    path = os.path.join(results_dir, "monitor_metrics.txt")
    if not os.path.isfile(path):
        print("\n  [SKIP] monitor_metrics.txt not found")
        return None

    # Parse the summary section at the end
    result = {}
    in_summary = False
    prev_section = ""
    with open(path) as f:
        for line in f:
            if "SUMMARY STATISTICS" in line:
                in_summary = True
            if in_summary:
                m = re.match(r'\s+(Min|Max|Avg):\s+(\d+\.?\d*)%?', line)
                if m:
                    which = m.group(1).lower()
                    val = float(m.group(2))
                    if "CPU" in prev_section:
                        result[f"cpu_{which}"] = val
                    elif "Memory" in prev_section:
                        result[f"mem_{which}"] = val
                m2 = re.match(r'\s+Avg throughput:\s+(\d+\.?\d*)\s+Mbps', line)
                if m2:
                    result["avg_throughput_mbps"] = float(m2.group(1))
                m3 = re.match(r'\s+Packets received:\s+(\d+)', line)
                if m3:
                    result["total_packets_recv"] = int(m3.group(1))
                m4 = re.match(r'\s+Bytes received:\s+(\d+\.?\d*)\s+MB', line)
                if m4:
                    result["total_bytes_recv_mb"] = float(m4.group(1))
                m5 = re.match(r'\s+Avg packet size:\s+(\d+\.?\d*)\s+bytes', line)
                if m5:
                    result["avg_packet_size_bytes"] = float(m5.group(1))
            if ":" in line and not line.startswith(" "):
                prev_section = line.strip()

    if result:
        rows = [[k, fmt(v, 2) if isinstance(v, float) else str(v)] for k, v in sorted(result.items())]
        print_table("KPI 4: System-wide Metrics (monitor_server.py)", ["Metric", "Value"], rows)
    else:
        print("\n  [INFO] monitor_metrics.txt did not contain summary section")

    return result


# =============================================================================
# KPI 5: Packet Delivery — Loss, Duplication, Out-of-Order
# =============================================================================

def analyse_delivery(results_dir, metadata):
    """Replicate stat.R logic: compare sensor-side vs client-side logs."""
    sensor_log_dir = os.path.join(results_dir, "sensor_log")
    server_log_dir = os.path.join(results_dir, "server_sensor_log")

    if not os.path.isdir(sensor_log_dir) or not os.path.isdir(server_log_dir):
        print("\n  [SKIP] sensor_log/ or server_sensor_log/ not found")
        return None

    num_clients = metadata.get("parameters", {}).get("num_clients", 0)

    # Read sensor-side logs: (timestamp, seq_no, value)
    def read_log(path):
        entries = []
        if not os.path.isfile(path):
            return entries
        with open(path) as f:
            reader = csv.reader(f, delimiter='\t')
            for row in reader:
                if len(row) >= 2:
                    try:
                        entries.append((float(row[0]), int(row[1])))
                    except ValueError:
                        continue
        return entries

    # Discover sensors
    sensors = set()
    for fname in os.listdir(sensor_log_dir):
        if fname.endswith('.log'):
            sensors.add(fname.replace('.log', ''))

    all_results = []

    for sensor_id in sorted(sensors):
        sensor_entries = read_log(os.path.join(sensor_log_dir, f"{sensor_id}.log"))
        server_entries = read_log(os.path.join(server_log_dir, f"{sensor_id}.log"))

        sensor_seqs = [s[1] for s in sensor_entries]
        server_seqs = [s[1] for s in server_entries]

        sensor_total = len(sensor_seqs)
        server_total = len(server_seqs)

        # Server-side loss (sensor → server)
        server_set = set(server_seqs)
        sensor_set = set(sensor_seqs)
        sensor_to_server_loss = len(sensor_set - server_set)

        for c in range(1, num_clients + 1):
            client_dir = os.path.join(results_dir, f"client{c}_sensor_log")
            client_entries = read_log(os.path.join(client_dir, f"{sensor_id}.log"))

            if not client_entries:
                all_results.append({
                    "sensor": sensor_id,
                    "client": c,
                    "sensor_sent": sensor_total,
                    "server_got": server_total,
                    "client_got": 0,
                    "lost": server_total,
                    "loss_pct": 100.0,
                    "duplicated": 0,
                    "out_of_order": 0,
                    "delays": [],
                })
                continue

            client_seqs = [e[1] for e in client_entries]
            client_ts = {e[1]: e[0] for e in client_entries}
            server_ts = {e[1]: e[0] for e in server_entries}

            client_set = set(client_seqs)
            lost = server_set - client_set
            duplicated = len(client_seqs) - len(client_set)

            # Out-of-order: count entries where seq_no < max seen so far
            max_seen = -1
            ooo_count = 0
            for seq in client_seqs:
                if seq < max_seen:
                    ooo_count += 1
                if seq > max_seen:
                    max_seen = seq

            # Delay: client_time - server_time for delivered packets
            delays = []
            for seq in client_set & server_set:
                if seq in client_ts and seq in server_ts:
                    delay = client_ts[seq] - server_ts[seq]
                    if delay >= 0:
                        delays.append(delay)

            loss_pct = (len(lost) / max(1, server_total)) * 100.0

            all_results.append({
                "sensor": sensor_id,
                "client": c,
                "sensor_sent": sensor_total,
                "server_got": server_total,
                "client_got": len(client_seqs),
                "lost": len(lost),
                "loss_pct": loss_pct,
                "duplicated": duplicated,
                "out_of_order": ooo_count,
                "delays": delays,
            })

    if not all_results:
        print("\n  [SKIP] No delivery data found")
        return None

    # Summary table
    rows = []
    for r in all_results:
        delay_stats = safe_stat(r["delays"])
        rows.append([
            r["sensor"], r["client"],
            r["sensor_sent"], r["server_got"], r["client_got"],
            r["lost"], fmt(r["loss_pct"], 1) + "%",
            r["duplicated"], r["out_of_order"],
            fmt(delay_stats["mean"] * 1000, 2) if delay_stats["n"] > 0 else "N/A",
            fmt(delay_stats["p95"] * 1000, 2) if delay_stats["n"] > 0 else "N/A",
        ])

    print_table(
        "KPI 5: Packet Delivery (sensor → server → client)",
        ["Sensor", "Client", "Sent", "Srv Got", "Cli Got",
         "Lost", "Loss%", "Dups", "OOO", "Mean Delay(ms)", "P95 Delay(ms)"],
        rows
    )

    # Aggregate statistics
    all_delays = []
    total_lost = 0
    total_delivered = 0
    total_dups = 0
    total_ooo = 0
    for r in all_results:
        all_delays.extend(r["delays"])
        total_lost += r["lost"]
        total_delivered += r["client_got"]
        total_dups += r["duplicated"]
        total_ooo += r["out_of_order"]

    agg_delay = safe_stat([d * 1000 for d in all_delays])  # Convert to ms
    total_pkts = total_lost + total_delivered

    print_table(
        "KPI 5 Aggregate: End-to-End Delivery Summary",
        ["Metric", "Value"],
        [
            ["Total packets (server→clients)",  total_pkts],
            ["Delivered",                        total_delivered],
            ["Lost",                             total_lost],
            ["Aggregate loss rate",              fmt(total_lost / max(1, total_pkts) * 100, 2) + "%"],
            ["Total duplicates",                 total_dups],
            ["Total out-of-order",               total_ooo],
            ["Mean delay (ms)",                  fmt(agg_delay["mean"], 3)],
            ["Median delay (ms)",                fmt(agg_delay["median"], 3)],
            ["P95 delay (ms)",                   fmt(agg_delay["p95"], 3)],
            ["P99 delay (ms)",                   fmt(agg_delay["p99"], 3)],
            ["Max delay (ms)",                   fmt(agg_delay["max"], 3)],
            ["Stdev delay (ms)",                 fmt(agg_delay["stdev"], 3)],
        ]
    )

    # Write CSV for R/pgfplots
    csv_path = os.path.join(results_dir, "delivery_kpi.csv")
    csv_rows = []
    for r in all_results:
        ds = safe_stat(r["delays"])
        csv_rows.append([
            r["sensor"], r["client"], r["sensor_sent"], r["server_got"],
            r["client_got"], r["lost"], f"{r['loss_pct']:.2f}",
            r["duplicated"], r["out_of_order"],
            f"{ds['mean']*1000:.3f}" if ds["n"] > 0 else "",
            f"{ds['p95']*1000:.3f}" if ds["n"] > 0 else "",
            f"{ds['p99']*1000:.3f}" if ds["n"] > 0 else "",
        ])
    write_csv(csv_path, [
        "sensor", "client", "sensor_sent", "server_got", "client_got",
        "lost", "loss_pct", "duplicated", "out_of_order",
        "mean_delay_ms", "p95_delay_ms", "p99_delay_ms"
    ], csv_rows)
    print(f"\n  [CSV] {csv_path}")

    return all_results


# =============================================================================
# KPI 6: Protocol Overhead (pcap analysis)
# =============================================================================

def analyse_pcap(results_dir):
    """Analyse pcap for protocol overhead statistics.

    We use tcpdump text output rather than requiring scapy/dpkt.
    """
    pcap_path = os.path.join(results_dir, "prtp_capture.pcap")
    if not os.path.isfile(pcap_path):
        print("\n  [SKIP] prtp_capture.pcap not found")
        return None

    # Use tcpdump to read the pcap
    import subprocess
    try:
        proc = subprocess.run(
            ["tcpdump", "-r", pcap_path, "-nn", "-tttt", "-v", "udp"],
            capture_output=True, text=True, timeout=60
        )
        lines = proc.stdout.strip().split('\n')
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        print(f"\n  [ERROR] Could not run tcpdump: {e}")
        return None

    if not lines or lines == ['']:
        print("\n  [INFO] pcap is empty (no packets captured)")
        return None

    # Count packets and parse lengths
    packet_count = 0
    lengths = []
    for line in lines:
        m = re.search(r'length\s+(\d+)', line)
        if m:
            lengths.append(int(m.group(1)))
            packet_count += 1

    if not lengths:
        print(f"\n  [INFO] Parsed {len(lines)} tcpdump lines but could not extract lengths")
        return None

    pkt_stats = safe_stat(lengths)

    # Estimate overhead: PRTP header is roughly 20-30 bytes (version+type+flags+seq+frag+timestamp)
    # UDP header: 8 bytes, IP header: 20 bytes
    IP_UDP_OVERHEAD = 28  # bytes
    PRTP_HEADER_EST = 24  # bytes (conservative estimate from messages.h)
    total_overhead_per_pkt = IP_UDP_OVERHEAD + PRTP_HEADER_EST

    total_bytes = sum(lengths)
    # UDP payload = length field; total on wire = length + IP+UDP headers
    total_wire_bytes = total_bytes + (packet_count * IP_UDP_OVERHEAD)

    # First/last timestamps for duration
    first_ts = None
    last_ts = None
    for line in lines:
        m = re.match(r'(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2}\.\d+)', line)
        if m:
            if first_ts is None:
                first_ts = m.group(1)
            last_ts = m.group(1)

    result = {
        "total_packets": packet_count,
        "total_udp_payload_bytes": total_bytes,
        "total_wire_bytes_est": total_wire_bytes,
        "pkt_size_stats": pkt_stats,
    }

    overhead_ratio = (total_overhead_per_pkt / max(1, pkt_stats["mean"])) * 100

    print_table(
        "KPI 6: Protocol Overhead (pcap analysis)",
        ["Metric", "Value"],
        [
            ["Total packets captured",         packet_count],
            ["Total UDP payload (MB)",          fmt(total_bytes / (1024**2), 2)],
            ["Est. total wire bytes (MB)",      fmt(total_wire_bytes / (1024**2), 2)],
            ["Mean UDP payload (bytes)",        fmt(pkt_stats["mean"], 1)],
            ["Median UDP payload (bytes)",      fmt(pkt_stats["median"], 1)],
            ["Min UDP payload (bytes)",         pkt_stats["min"]],
            ["Max UDP payload (bytes)",         pkt_stats["max"]],
            ["IP+UDP header overhead/pkt",      f"{IP_UDP_OVERHEAD} bytes"],
            ["Est. PRTP header overhead/pkt",   f"{PRTP_HEADER_EST} bytes"],
            ["Overhead ratio (headers/payload)", fmt(overhead_ratio, 1) + "%"],
            ["Capture timespan",                f"{first_ts or 'N/A'} → {last_ts or 'N/A'}"],
        ]
    )

    # Write packet size distribution CSV
    csv_path = os.path.join(results_dir, "packet_sizes.csv")
    write_csv(csv_path, ["udp_payload_bytes"], [[l] for l in lengths])
    print(f"\n  [CSV] {csv_path}")

    return result


# =============================================================================
# KPI 7: Socket Buffer Utilisation
# =============================================================================

def analyse_socket_buffers(results_dir):
    """Parse ss snapshots to track socket buffer fill levels.

    The benchmark script captures output of:
        ss -nlu sport = :5000 or sport = :5001
    which produces lines like:
        Recv-Q Send-Q  Local Address:Port  Peer Address:Port  Process
        0      0       127.0.0.1:5000      0.0.0.0:*

    It may also include skmem lines from the -m flag:
        skmem:(r<N>,rb<N>,t<N>,tb<N>,f<N>,w<N>,o<N>,bl<N>,d<N>)
    """
    path = os.path.join(results_dir, "socket_buffer_samples.txt")
    if not os.path.isfile(path):
        print("\n  [SKIP] socket_buffer_samples.txt not found")
        return None

    recv_q_values = []
    send_q_values = []
    skmem_rb_values = []  # receive buffer size from skmem
    skmem_tb_values = []  # transmit buffer size from skmem
    sample_count = 0

    with open(path) as f:
        for line in f:
            line = line.strip()
            if line.startswith("---"):
                sample_count += 1
                continue
            if not line or line.startswith("Recv-Q") or line.startswith("State"):
                continue

            # Parse skmem lines: skmem:(r0,rb425984,t0,tb58080,...)
            skmem_match = re.search(r'skmem:\(r(\d+),rb(\d+),t(\d+),tb(\d+)', line)
            if skmem_match:
                skmem_rb_values.append(int(skmem_match.group(2)))
                skmem_tb_values.append(int(skmem_match.group(4)))
                continue

            # Parse queue lines: first two numeric columns are Recv-Q and Send-Q
            parts = line.split()
            if len(parts) >= 4:
                # Skip state column if present (UNCONN, ESTAB, etc.)
                start = 0
                if parts[0] in ("UNCONN", "ESTAB", "LISTEN", "CLOSE-WAIT", "TIME-WAIT"):
                    start = 1
                try:
                    recv_q = int(parts[start])
                    send_q = int(parts[start + 1])
                    recv_q_values.append(recv_q)
                    send_q_values.append(send_q)
                except (ValueError, IndexError):
                    continue

    if not recv_q_values and not skmem_rb_values:
        print("\n  [INFO] No UDP socket queue data found in ss output")
        return None

    result = {"snapshots": sample_count}
    rows = []

    if recv_q_values:
        recv_stats = safe_stat(recv_q_values)
        send_stats = safe_stat(send_q_values)
        result["recv_q"] = recv_stats
        result["send_q"] = send_stats
        rows.extend([
            ["Recv-Q samples",  recv_stats["n"],   ""],
            ["Recv-Q mean",     fmt(recv_stats["mean"], 1), ""],
            ["Recv-Q max",      fmt(recv_stats["max"], 1),  ""],
            ["Send-Q samples",  send_stats["n"],   ""],
            ["Send-Q mean",     fmt(send_stats["mean"], 1), ""],
            ["Send-Q max",      fmt(send_stats["max"], 1),  ""],
        ])

    if skmem_rb_values:
        rb_stats = safe_stat(skmem_rb_values)
        tb_stats = safe_stat(skmem_tb_values)
        result["skmem_rb"] = rb_stats
        result["skmem_tb"] = tb_stats
        rows.extend([
            ["Recv buf (rb) mean", fmt(rb_stats["mean"] / 1024, 1) + " KB", ""],
            ["Recv buf (rb) max",  fmt(rb_stats["max"] / 1024, 1) + " KB",  ""],
            ["Send buf (tb) mean", fmt(tb_stats["mean"] / 1024, 1) + " KB", ""],
            ["Send buf (tb) max",  fmt(tb_stats["max"] / 1024, 1) + " KB",  ""],
        ])

    rows.append(["Snapshots", sample_count, ""])

    print_table(
        "KPI 7: Socket Buffer Utilisation (ss UDP)",
        ["Metric", "Value", ""],
        rows
    )

    return result


# =============================================================================
# KPI 8: PRTP server log — Q-learning decisions (reuses analyze_uack.py logic)
# =============================================================================

def analyse_q_decisions(results_dir):
    """Parse Q-Decision lines from the PRTP server log."""
    path = os.path.join(results_dir, "prtp_server.log")
    if not os.path.isfile(path):
        print("\n  [SKIP] prtp_server.log not found")
        return None

    pattern = re.compile(
        r'Q-Decision:\s+(\S+)\s+\|\s+RTT=(\d+)ms\(lvl=(\d)\)\s+\|\s+IMP=(\S+)\s+\|\s+ACTION=(\S+)\s+\|\s+Q=([\-\d.]+)'
    )

    decisions = []
    with open(path) as f:
        for line in f:
            m = pattern.search(line)
            if m:
                decisions.append({
                    'sensor': m.group(1),
                    'rtt_level': int(m.group(3)),
                    'importance': m.group(4),
                    'action': m.group(5),
                    'q_value': float(m.group(6)),
                })

    if not decisions:
        print("\n  [INFO] No Q-Decision entries found in server log")
        return None

    # Action distribution
    action_counts = Counter(d['action'] for d in decisions)
    total = len(decisions)

    rows = []
    for action in ['UNRELIABLE', 'RELIABLE', 'DROP']:
        c = action_counts.get(action, 0)
        rows.append([action, c, fmt(c / max(1, total) * 100, 1) + "%"])
    rows.append(["TOTAL", total, "100.0%"])

    print_table(
        "KPI 8: Q-Learning Reliability Decisions",
        ["Action", "Count", "Percentage"],
        rows
    )

    # By importance
    imp_action = defaultdict(Counter)
    for d in decisions:
        imp_action[d['importance']][d['action']] += 1

    imp_rows = []
    for imp in ['LOW', 'NORMAL', 'HIGH']:
        counts = imp_action.get(imp, Counter())
        t = sum(counts.values())
        imp_rows.append([
            imp, t,
            counts.get('UNRELIABLE', 0),
            counts.get('RELIABLE', 0),
            counts.get('DROP', 0),
            fmt(counts.get('RELIABLE', 0) / max(1, t) * 100, 1) + "%",
        ])

    if imp_rows:
        print_table(
            "KPI 8b: Q-Decisions by Importance Level",
            ["Importance", "Total", "Unreliable", "Reliable", "Drop", "Reliability%"],
            imp_rows
        )

    return {"decisions": len(decisions), "action_counts": dict(action_counts)}


# =============================================================================
# Throughput derived KPI
# =============================================================================

def compute_prtp_throughput(delivery_results, metadata, pcap_result):
    """Compute PRTP application-layer throughput from delivery data."""
    if not delivery_results or not metadata:
        return

    duration = metadata.get("parameters", {}).get("duration_seconds", 1)

    total_delivered = sum(r["client_got"] for r in delivery_results)
    total_sensors_sent = sum(r["sensor_sent"] for r in delivery_results if r["client"] == 1)

    msgs_per_sec = total_delivered / max(1, duration)

    rows = [
        ["Test duration (s)", duration],
        ["Total sensor msgs sent", total_sensors_sent],
        ["Total client msgs received", total_delivered],
        ["Goodput (msgs/sec)", fmt(msgs_per_sec, 2)],
    ]

    if pcap_result:
        total_bytes = pcap_result.get("total_udp_payload_bytes", 0)
        throughput_mbps = (total_bytes * 8) / (duration * 1e6)
        rows.append(["Wire throughput (Mbps)", fmt(throughput_mbps, 2)])

    print_table("PRTP Application Throughput", ["Metric", "Value"], rows)


# =============================================================================
# Main
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="PRIoTP Localhost Benchmark — Post-Experiment Analysis",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("results_dir", help="Path to benchmark results directory")
    args = parser.parse_args()

    results_dir = args.results_dir
    if not os.path.isdir(results_dir):
        print(f"ERROR: {results_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    # Load metadata
    meta_path = os.path.join(results_dir, "experiment_metadata.json")
    metadata = {}
    if os.path.isfile(meta_path):
        with open(meta_path) as f:
            metadata = json.load(f)

    print("=" * 72)
    print("  PRIoTP Localhost Benchmark — Analysis Report")
    print("=" * 72)
    print(f"  Results: {results_dir}")
    if metadata:
        print(f"  Experiment: {metadata.get('timestamp', 'N/A')}")
        params = metadata.get('parameters', {})
        print(f"  Duration: {params.get('duration_seconds', 'N/A')}s")
        print(f"  Sensors: {params.get('num_sensors', 'N/A')}")
        print(f"  Clients: {params.get('num_clients', 'N/A')}")
    print("=" * 72)

    # Run all analyses
    ping_result   = analyse_ping(results_dir)
    iperf_result  = analyse_iperf3(results_dir)
    cpu_result    = analyse_cpu(results_dir)
    monitor_result = analyse_monitor(results_dir)
    delivery_result = analyse_delivery(results_dir, metadata)
    pcap_result   = analyse_pcap(results_dir)
    socket_result = analyse_socket_buffers(results_dir)
    q_result      = analyse_q_decisions(results_dir)

    # Derived throughput
    compute_prtp_throughput(delivery_result, metadata, pcap_result)

    # Summary of available data
    print("\n" + "=" * 72)
    print("  Analysis Summary")
    print("=" * 72)
    checks = [
        ("Loopback latency (ping)",       ping_result is not None),
        ("UDP throughput baseline (iperf3)", iperf_result is not None),
        ("CPU utilisation (pidstat)",       cpu_result is not None and len(cpu_result) > 0),
        ("System metrics (monitor)",        monitor_result is not None),
        ("Packet delivery (logs)",          delivery_result is not None),
        ("Protocol overhead (pcap)",        pcap_result is not None),
        ("Socket buffers (ss)",             socket_result is not None),
        ("Q-learning decisions (log)",      q_result is not None),
    ]
    for label, ok in checks:
        status = "OK" if ok else "MISSING"
        print(f"  [{status:7s}] {label}")
    print("=" * 72)


if __name__ == "__main__":
    main()
