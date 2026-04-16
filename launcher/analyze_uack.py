#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PRIoTP UACK Behavior Analyzer
Parses server/client logs and generates graphs + tables showing:
  1. Q-Decision distribution (reliable vs unreliable vs drop)
  2. ACK/NACK rates over time  
  3. Per-sensor-type reliability decisions
  4. Packet loss vs UACK response behavior

Usage:
  python3 analyze_uack.py --server-log <server_stderr> --client-logs <dir> [--output-dir <dir>]

If no logs provided, generates sample data for demonstration.
"""

import argparse
import re
import os
import sys
from collections import defaultdict, Counter

# Try importing matplotlib; fall back to text-only output
try:
    import matplotlib
    matplotlib.use('Agg')  # Non-interactive backend
    import matplotlib.pyplot as plt
    import matplotlib.ticker as ticker
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("Note: matplotlib not found. Will produce text tables only.")
    print("Install with: pip3 install matplotlib")


def parse_q_decisions(log_lines):
    """Parse Q-Decision lines from server log.
    Format: Q-Decision: <sensor_id> | RTT=<ms>ms(lvl=<n>) | IMP=<level> | ACTION=<action> | Q=<value>
    """
    decisions = []
    pattern = re.compile(
        r'Q-Decision:\s+(\S+)\s+\|\s+RTT=(\d+)ms\(lvl=(\d)\)\s+\|\s+IMP=(\S+)\s+\|\s+ACTION=(\S+)\s+\|\s+Q=([\-\d.]+)'
    )
    for line in log_lines:
        m = pattern.search(line)
        if m:
            decisions.append({
                'sensor': m.group(1),
                'rtt_ms': int(m.group(2)),
                'rtt_level': int(m.group(3)),
                'importance': m.group(4),
                'action': m.group(5),
                'q_value': float(m.group(6)),
            })
    return decisions


def parse_ack_nack(log_lines):
    """Parse ACK and NACK events from server log."""
    events = []
    ts_pattern = re.compile(r'\[(\d+\.\d+)\]')

    for line in log_lines:
        ts_match = ts_pattern.search(line)
        ts = float(ts_match.group(1)) if ts_match else 0

        if 'update acknowledgement' in line.lower() or 'Update acknowledgment' in line:
            events.append({'time': ts, 'type': 'ACK'})
        elif 'negative update acknowledgement' in line.lower() or 'UPDATE_NACK' in line:
            events.append({'time': ts, 'type': 'NACK'})
        elif 'DROPPED' in line:
            events.append({'time': ts, 'type': 'DROP'})
    return events


def parse_client_sensor_logs(log_dir):
    """Parse client sensor log files to count received updates per sensor."""
    sensor_counts = {}
    if not os.path.isdir(log_dir):
        return sensor_counts
    for fname in os.listdir(log_dir):
        if fname.endswith('.log'):
            sensor_id = fname.replace('.log', '')
            count = 0
            with open(os.path.join(log_dir, fname)) as f:
                for line in f:
                    if line.strip():
                        count += 1
            sensor_counts[sensor_id] = count
    return sensor_counts


def generate_sample_data():
    """Generate sample data for demonstration when no logs are available."""
    import random
    random.seed(42)

    decisions = []
    sensors = ['temp_1234', 'temp_5678', 'gps_9012', 'device_3456', 'camera_7890', 'camera_1111']
    importances = {'temp': 'NORMAL', 'gps': 'NORMAL', 'device': 'NORMAL', 'camera': 'HIGH'}
    actions = ['UNRELIABLE', 'RELIABLE', 'DROP']
    action_weights_by_imp = {
        'LOW':    [0.2, 0.1, 0.7],
        'NORMAL': [0.5, 0.4, 0.1],
        'HIGH':   [0.1, 0.8, 0.1],
    }

    for i in range(500):
        sensor = random.choice(sensors)
        stype = sensor.split('_')[0]
        imp = importances.get(stype, 'NORMAL')
        # Camera with motion = HIGH, camera idle = LOW
        if stype == 'camera':
            imp = random.choice(['HIGH', 'LOW'])
        weights = action_weights_by_imp[imp]
        action = random.choices(actions, weights=weights, k=1)[0]
        rtt = random.choice([50, 100, 200, 300])
        q_val = random.uniform(-5, 25)

        decisions.append({
            'sensor': sensor,
            'rtt_ms': rtt,
            'rtt_level': 0 if rtt <= 150 else 1,
            'importance': imp,
            'action': action,
            'q_value': q_val,
        })

    events = []
    for i in range(300):
        t = 1000 + i * 0.2
        etype = random.choices(['ACK', 'NACK', 'DROP'], weights=[0.7, 0.15, 0.15], k=1)[0]
        events.append({'time': t, 'type': etype})

    return decisions, events


# ──────────────────────────────────────────────────────────────────────────
# TABLE OUTPUT (always works, no dependencies)
# ──────────────────────────────────────────────────────────────────────────

def print_action_distribution_table(decisions):
    """Table 1: Q-Decision action distribution by importance level."""
    print("\n" + "=" * 70)
    print("TABLE 1: UACK Decision Distribution by Importance Level")
    print("=" * 70)

    imp_action = defaultdict(Counter)
    for d in decisions:
        imp_action[d['importance']][d['action']] += 1

    actions = ['UNRELIABLE', 'RELIABLE', 'DROP']
    header = f"{'Importance':<12} | {'UNRELIABLE':>12} | {'RELIABLE':>12} | {'DROP':>12} | {'Total':>8}"
    print(header)
    print("-" * len(header))

    for imp in ['LOW', 'NORMAL', 'HIGH']:
        counts = imp_action.get(imp, Counter())
        total = sum(counts.values())
        row = f"{imp:<12}"
        for a in actions:
            c = counts.get(a, 0)
            pct = (c / total * 100) if total > 0 else 0
            row += f" | {c:>5} ({pct:5.1f}%)"
        row += f" | {total:>8}"
        print(row)

    grand = sum(sum(c.values()) for c in imp_action.values())
    print("-" * len(header))
    print(f"{'TOTAL':<12}", end="")
    for a in actions:
        c = sum(imp_action[imp].get(a, 0) for imp in imp_action)
        pct = (c / grand * 100) if grand > 0 else 0
        print(f" | {c:>5} ({pct:5.1f}%)", end="")
    print(f" | {grand:>8}")


def print_rtt_impact_table(decisions):
    """Table 2: RTT level impact on reliability decisions."""
    print("\n" + "=" * 70)
    print("TABLE 2: RTT Impact on UACK Decisions")
    print("=" * 70)

    rtt_action = defaultdict(Counter)
    for d in decisions:
        label = f"LOW (0-150ms)" if d['rtt_level'] == 0 else f"HIGH (>150ms)"
        rtt_action[label][d['action']] += 1

    actions = ['UNRELIABLE', 'RELIABLE', 'DROP']
    header = f"{'RTT Level':<16} | {'UNRELIABLE':>12} | {'RELIABLE':>12} | {'DROP':>12} | {'Total':>8}"
    print(header)
    print("-" * len(header))

    for rtt_label in ['LOW (0-150ms)', 'HIGH (>150ms)']:
        counts = rtt_action.get(rtt_label, Counter())
        total = sum(counts.values())
        row = f"{rtt_label:<16}"
        for a in actions:
            c = counts.get(a, 0)
            pct = (c / total * 100) if total > 0 else 0
            row += f" | {c:>5} ({pct:5.1f}%)"
        row += f" | {total:>8}"
        print(row)


def print_sensor_type_table(decisions):
    """Table 3: Per-sensor-type behavior."""
    print("\n" + "=" * 70)
    print("TABLE 3: UACK Behavior by Sensor Type")
    print("=" * 70)

    stype_action = defaultdict(Counter)
    for d in decisions:
        stype = d['sensor'].split('_')[0].upper()
        stype_action[stype][d['action']] += 1

    actions = ['UNRELIABLE', 'RELIABLE', 'DROP']
    header = f"{'Sensor Type':<14} | {'UNRELIABLE':>12} | {'RELIABLE':>12} | {'DROP':>12} | {'Reliability%':>13}"
    print(header)
    print("-" * len(header))

    for stype in sorted(stype_action.keys()):
        counts = stype_action[stype]
        total = sum(counts.values())
        reliable_pct = (counts.get('RELIABLE', 0) / total * 100) if total > 0 else 0
        row = f"{stype:<14}"
        for a in actions:
            c = counts.get(a, 0)
            row += f" | {c:>12}"
        row += f" | {reliable_pct:>12.1f}%"
        print(row)


def print_ack_nack_summary(events):
    """Table 4: ACK/NACK/DROP event summary."""
    print("\n" + "=" * 70)
    print("TABLE 4: UACK Feedback Summary")
    print("=" * 70)

    counts = Counter(e['type'] for e in events)
    total = sum(counts.values())

    header = f"{'Event Type':<12} | {'Count':>8} | {'Percentage':>12}"
    print(header)
    print("-" * len(header))
    for etype in ['ACK', 'NACK', 'DROP']:
        c = counts.get(etype, 0)
        pct = (c / total * 100) if total > 0 else 0
        print(f"{etype:<12} | {c:>8} | {pct:>11.1f}%")
    print("-" * len(header))
    print(f"{'TOTAL':<12} | {total:>8} | {'100.0':>11}%")

    if counts.get('ACK', 0) + counts.get('NACK', 0) > 0:
        delivery_rate = counts.get('ACK', 0) / (counts.get('ACK', 0) + counts.get('NACK', 0)) * 100
        print(f"\nDelivery success rate (ACK / (ACK+NACK)): {delivery_rate:.1f}%")


# ──────────────────────────────────────────────────────────────────────────
# GRAPH OUTPUT (requires matplotlib)
# ──────────────────────────────────────────────────────────────────────────

def plot_action_distribution(decisions, output_dir):
    """Figure 1: Stacked bar chart of actions per importance level."""
    imp_action = defaultdict(Counter)
    for d in decisions:
        imp_action[d['importance']][d['action']] += 1

    imps = ['LOW', 'NORMAL', 'HIGH']
    actions = ['UNRELIABLE', 'RELIABLE', 'DROP']
    colors = ['#4CAF50', '#2196F3', '#F44336']

    fig, ax = plt.subplots(figsize=(8, 5))
    x = range(len(imps))
    bottom = [0] * len(imps)

    for i, action in enumerate(actions):
        values = [imp_action[imp].get(action, 0) for imp in imps]
        ax.bar(x, values, bottom=bottom, label=action, color=colors[i], width=0.5)
        bottom = [b + v for b, v in zip(bottom, values)]

    ax.set_xlabel('Data Importance Level', fontsize=12)
    ax.set_ylabel('Number of Packets', fontsize=12)
    ax.set_title('PRIoTP UACK Decision Distribution by Importance', fontsize=13, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(imps)
    ax.legend()
    ax.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    path = os.path.join(output_dir, 'fig1_uack_decision_distribution.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"  Saved: {path}")


def plot_rtt_comparison(decisions, output_dir):
    """Figure 2: Grouped bar chart comparing RTT impact."""
    rtt_action = defaultdict(Counter)
    for d in decisions:
        rtt_action[d['rtt_level']][d['action']] += 1

    actions = ['UNRELIABLE', 'RELIABLE', 'DROP']
    colors = ['#4CAF50', '#2196F3', '#F44336']
    labels = ['LOW RTT (<150ms)', 'HIGH RTT (>150ms)']

    fig, ax = plt.subplots(figsize=(8, 5))
    bar_width = 0.3
    x = range(len(actions))

    for i, rtt_lvl in enumerate([0, 1]):
        total = sum(rtt_action[rtt_lvl].values()) or 1
        values = [rtt_action[rtt_lvl].get(a, 0) / total * 100 for a in actions]
        offset = (i - 0.5) * bar_width
        ax.bar([xi + offset for xi in x], values, bar_width, label=labels[i],
               color=['#66BB6A', '#EF5350'][i], alpha=0.85)

    ax.set_xlabel('Action', fontsize=12)
    ax.set_ylabel('Percentage (%)', fontsize=12)
    ax.set_title('PRIoTP: RTT Impact on UACK Reliability Selection', fontsize=13, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(actions)
    ax.legend()
    ax.grid(axis='y', alpha=0.3)
    plt.tight_layout()
    path = os.path.join(output_dir, 'fig2_rtt_impact.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"  Saved: {path}")


def plot_sensor_type_pie(decisions, output_dir):
    """Figure 3: Pie charts per sensor type showing UACK behavior."""
    stype_action = defaultdict(Counter)
    for d in decisions:
        stype = d['sensor'].split('_')[0].upper()
        stype_action[stype][d['action']] += 1

    stypes = sorted(stype_action.keys())
    n = len(stypes)
    if n == 0:
        return

    fig, axes = plt.subplots(1, n, figsize=(4 * n, 4))
    if n == 1:
        axes = [axes]
    colors = ['#4CAF50', '#2196F3', '#F44336']
    action_order = ['UNRELIABLE', 'RELIABLE', 'DROP']

    for ax, stype in zip(axes, stypes):
        counts = stype_action[stype]
        values = [counts.get(a, 0) for a in action_order]
        if sum(values) == 0:
            continue
        ax.pie(values, labels=action_order, colors=colors, autopct='%1.0f%%',
               startangle=90, textprops={'fontsize': 9})
        ax.set_title(stype, fontsize=12, fontweight='bold')

    fig.suptitle('PRIoTP UACK Decisions by Sensor Type', fontsize=14, fontweight='bold', y=1.02)
    plt.tight_layout()
    path = os.path.join(output_dir, 'fig3_sensor_type_uack.png')
    plt.savefig(path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"  Saved: {path}")


def plot_ack_nack_timeline(events, output_dir):
    """Figure 4: ACK/NACK events over time (sliding window rate)."""
    if not events or events[0]['time'] == 0:
        print("  Skipping timeline plot (no timestamps in events)")
        return

    # Bin into 5-second windows
    min_t = min(e['time'] for e in events)
    max_t = max(e['time'] for e in events)
    window = 5.0
    bins = []
    t = min_t
    while t < max_t:
        ack_c = sum(1 for e in events if e['type'] == 'ACK' and t <= e['time'] < t + window)
        nack_c = sum(1 for e in events if e['type'] == 'NACK' and t <= e['time'] < t + window)
        drop_c = sum(1 for e in events if e['type'] == 'DROP' and t <= e['time'] < t + window)
        bins.append({'time': t - min_t, 'ACK': ack_c, 'NACK': nack_c, 'DROP': drop_c})
        t += window

    fig, ax = plt.subplots(figsize=(10, 5))
    times = [b['time'] for b in bins]
    ax.plot(times, [b['ACK'] for b in bins], 'g-o', markersize=3, label='ACK', linewidth=1.5)
    ax.plot(times, [b['NACK'] for b in bins], 'r-s', markersize=3, label='NACK', linewidth=1.5)
    ax.plot(times, [b['DROP'] for b in bins], 'k--^', markersize=3, label='DROP', linewidth=1)

    ax.set_xlabel('Time (seconds)', fontsize=12)
    ax.set_ylabel(f'Events per {window:.0f}s window', fontsize=12)
    ax.set_title('PRIoTP UACK Feedback Over Time', fontsize=13, fontweight='bold')
    ax.legend()
    ax.grid(alpha=0.3)
    plt.tight_layout()
    path = os.path.join(output_dir, 'fig4_uack_timeline.png')
    plt.savefig(path, dpi=150)
    plt.close()
    print(f"  Saved: {path}")


# ──────────────────────────────────────────────────────────────────────────
# MAIN
# ──────────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description='PRIoTP UACK Behavior Analyzer')
    parser.add_argument('--server-log', help='Path to server stderr/log file')
    parser.add_argument('--client-logs', help='Path to a client sensor log directory')
    parser.add_argument('--output-dir', default='./uack_analysis', help='Output directory for graphs')
    parser.add_argument('--demo', action='store_true', help='Run with sample data for demonstration')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    # Load data
    if args.demo or (not args.server_log):
        print("Running with SAMPLE DATA (use --server-log to analyze real logs)")
        print("=" * 70)
        decisions, events = generate_sample_data()
    else:
        with open(args.server_log) as f:
            log_lines = f.readlines()
        decisions = parse_q_decisions(log_lines)
        events = parse_ack_nack(log_lines)
        print(f"Parsed {len(decisions)} Q-decisions, {len(events)} ACK/NACK events")

    if not decisions:
        print("No Q-decision data found. Make sure Q-learning is enabled on the server.")
        sys.exit(1)

    # Print tables (always works)
    print_action_distribution_table(decisions)
    print_rtt_impact_table(decisions)
    print_sensor_type_table(decisions)
    print_ack_nack_summary(events)

    # Parse client logs if provided
    if args.client_logs:
        for d in sorted(os.listdir(args.client_logs)):
            full = os.path.join(args.client_logs, d)
            if os.path.isdir(full):
                counts = parse_client_sensor_logs(full)
                if counts:
                    print(f"\n--- Client: {d} ---")
                    for sid, n in sorted(counts.items()):
                        print(f"  {sid}: {n} updates received")

    # Generate graphs if matplotlib available
    if HAS_MATPLOTLIB:
        print(f"\nGenerating figures to {args.output_dir}/")
        plot_action_distribution(decisions, args.output_dir)
        plot_rtt_comparison(decisions, args.output_dir)
        plot_sensor_type_pie(decisions, args.output_dir)
        plot_ack_nack_timeline(events, args.output_dir)
        print("\nAll figures saved.")
    else:
        print("\nInstall matplotlib for graphs: pip3 install matplotlib")

    print("\nDone.")


if __name__ == '__main__':
    main()
