#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PRTP Stress Test
================
Runs ONE PRTP_server + sensors, then drives progressively more PRTP_client
processes to stress-test server scalability.

Default tiers: 10 → 100 → 500 → 1000 clients
  (override with --counts 10 100 1000 2000 ...)

Each tier:
  1. Batch-launch N PRTP_client processes with adaptive stagger
  2. Wait --duration seconds
  3. Sample server CPU/mem via /proc
  4. Count clients alive at connect-time and at end-of-tier
  5. Kill all clients → brief cooldown → next tier

Usage:
  python3 stress_test.py
  python3 stress_test.py --counts 10 100 1000 2000 --duration 30
  python3 stress_test.py --counts 10 100 --duration 20 --no-sensors
"""

import argparse
import atexit
import json
import os
import resource
import shutil
import signal
import subprocess
import sys
import threading
import time
from datetime import datetime
from pathlib import Path

# ── Paths ──────────────────────────────────────────────────────────────────────
SCRIPT_DIR   = Path(__file__).resolve().parent
PRTP_APP_DIR = SCRIPT_DIR.parent / "PRTP" / "application"
PRTP_SRC_DIR = SCRIPT_DIR.parent / "PRTP" / "src"
PRTP_CONF_DIR= SCRIPT_DIR.parent / "PRTP" / "conf"
SENSOR_PY    = SCRIPT_DIR / "sensor.py"
SERVER_BIN   = PRTP_APP_DIR / "PRTP_server"
CLIENT_BIN   = PRTP_APP_DIR / "PRTP_client"

# ── Defaults ───────────────────────────────────────────────────────────────────
DEFAULT_SERVER_IP   = "127.0.0.1"
DEFAULT_SENSOR_PORT = 5000
DEFAULT_CLIENT_PORT = 5001
DEFAULT_DURATION    = 30          # seconds per tier
DEFAULT_COUNTS      = [10, 100, 500, 1000]
DEFAULT_NUM_SENSORS = 4

# ── Global cleanup registry ────────────────────────────────────────────────────
_PROCS_LOCK = threading.Lock()
_ALL_PROCS  = []          # (Popen, open file handle or None)

def _register(proc, fh=None):
    with _PROCS_LOCK:
        _ALL_PROCS.append((proc, fh))
    return proc

def _kill_registered():
    with _PROCS_LOCK:
        items = list(_ALL_PROCS)
    for proc, fh in items:
        try:
            proc.send_signal(signal.SIGINT)
        except Exception:
            pass
    time.sleep(1)
    for proc, fh in items:
        try:
            if proc.poll() is None:
                proc.kill()
        except Exception:
            pass
        if fh:
            try:
                fh.close()
            except Exception:
                pass

atexit.register(_kill_registered)

# ── Helpers ────────────────────────────────────────────────────────────────────
def _raise_fd_limit(n: int) -> int:
    """Try to raise the process NOFILE soft limit to at least n."""
    try:
        soft, hard = resource.getrlimit(resource.RLIMIT_NOFILE)
        need = min(max(n, soft), hard if hard != resource.RLIM_INFINITY else n)
        if need > soft:
            resource.setrlimit(resource.RLIMIT_NOFILE, (need, hard))
        return need
    except Exception:
        return 0

def _sep(char="═", width=72):
    print(char * width)

def _banner(msg, char="═", width=72):
    _sep(char, width)
    pad_l = (width - len(msg) - 2) // 2
    pad_r = width - 2 - pad_l - len(msg)
    print(f"{char}{' ' * pad_l}{msg}{' ' * pad_r}{char}")
    _sep(char, width)

# ── Server startup ─────────────────────────────────────────────────────────────
def start_server(server_ip, sensor_port, client_port, results_dir: Path):
    """Launch PRTP_server; return (Popen, log_file_handle)."""
    # Build sensor.list in PRTP_APP_DIR (server reads from its CWD)
    sensor_list = PRTP_APP_DIR / "sensor.list"

    # Find clients config (test.conf)
    client_conf = None
    for cp in [PRTP_CONF_DIR / "test.conf", PRTP_CONF_DIR / "test3.conf"]:
        if cp.exists():
            client_conf = cp
            break

    # Find Q-table
    q_table = None
    for qp in [SCRIPT_DIR / "q_agent_trained.csv",
               PRTP_APP_DIR / "q_agent_trained.csv"]:
        if qp.exists():
            q_table = qp
            break

    cmd = [
        str(SERVER_BIN),
        f"-i{server_ip}",
        f"-p{sensor_port}",
        f"-s{client_port}",
        f"-l{sensor_list}",
    ]
    if client_conf:
        cmd.append(f"-c{client_conf}")
    if q_table:
        cmd.append(f"-q{q_table}")

    log_path = results_dir / "server.log"
    log_fh   = open(log_path, "w")
    proc = subprocess.Popen(
        cmd,
        cwd=str(PRTP_APP_DIR),
        stdout=log_fh,
        stderr=subprocess.STDOUT,
    )
    return proc, log_fh

# ── Sensor startup ─────────────────────────────────────────────────────────────
def start_sensors(server_ip, sensor_port, num_sensors, results_dir: Path):
    """Generate sensor.list and launch sensor.py simulators."""
    sensor_types = ["device", "temp", "gps", "camera"]

    # Build sensor.list in both launcher dir (sensor.py CWD) and app dir (server CWD)
    entries = []
    for i in range(num_sensors):
        stype = sensor_types[i % 4]
        entries.append(f"{stype}_{i}")

    sensor_list_content = "\n".join(entries) + "\n"
    for p in [SCRIPT_DIR / "sensor.list", PRTP_APP_DIR / "sensor.list"]:
        p.write_text(sensor_list_content)

    log_path = results_dir / "sensors.log"
    log_fh   = open(log_path, "w")

    procs = []
    for i, entry in enumerate(entries):
        stype, sid = entry.split("_", 1)
        cmd = [sys.executable, str(SENSOR_PY), stype, server_ip, str(sensor_port), sid]
        p = subprocess.Popen(
            cmd,
            cwd=str(SCRIPT_DIR),   # sensor.py needs path.txt in its cwd
            stdout=log_fh,
            stderr=subprocess.STDOUT,
        )
        procs.append(p)

    return procs, log_fh

# ── Client launcher ────────────────────────────────────────────────────────────
def launch_clients(server_ip, client_port, n_clients: int, log_base: Path) -> list:
    """
    Batch-launch N PRTP_client processes.
    Adaptive stagger so that even 1000 clients finish launching in ~5 seconds.
    Returns list of Popen objects.
    """
    log_base.mkdir(parents=True, exist_ok=True)

    # Adaptive batch size / inter-batch delay
    if n_clients <= 20:
        batch_size, batch_delay = 2,  0.30
    elif n_clients <= 100:
        batch_size, batch_delay = 10, 0.20
    elif n_clients <= 500:
        batch_size, batch_delay = 25, 0.15
    else:
        batch_size, batch_delay = 50, 0.10

    procs = []
    for i in range(1, n_clients + 1):
        log_dir = log_base / f"c{i:04d}"
        log_dir.mkdir(parents=True, exist_ok=True)

        cmd = [
            str(CLIENT_BIN),
            f"-s{server_ip}",
            f"-p{client_port}",
            "-A",            # subscribe to ALL sensors reliably
            f"-l{log_dir}",
        ]
        p = subprocess.Popen(
            cmd,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        procs.append(p)

        if i < n_clients and i % batch_size == 0:
            time.sleep(batch_delay)

    return procs

def _kill_procs(procs: list, timeout: float = 1.5):
    """SIGINT then SIGKILL a list of Popen objects."""
    for p in procs:
        try:
            p.send_signal(signal.SIGINT)
        except Exception:
            pass
    time.sleep(timeout)
    for p in procs:
        try:
            if p.poll() is None:
                p.kill()
        except Exception:
            pass

# ── Server stats via /proc ─────────────────────────────────────────────────────
def _collect_server_stats(pid: int, duration: float, interval: float, out: dict):
    """
    Runs in a background thread. Samples /proc/<pid>/status and /proc/<pid>/stat
    for RSS and CPU ticks. Writes results into `out` dict.
    """
    rss_samples  = []
    cpu_prev     = None
    cpu_deltas   = []
    deadline     = time.time() + duration
    clk_tck      = os.sysconf("SC_CLK_TCK") or 100

    while time.time() < deadline:
        # RSS
        try:
            for line in Path(f"/proc/{pid}/status").read_text().splitlines():
                if line.startswith("VmRSS:"):
                    rss_samples.append(int(line.split()[1]))
                    break
        except Exception:
            pass
        # CPU
        try:
            parts    = Path(f"/proc/{pid}/stat").read_text().split()
            cpu_now  = int(parts[13]) + int(parts[14])
            if cpu_prev is not None:
                cpu_deltas.append(cpu_now - cpu_prev)
            cpu_prev = cpu_now
        except Exception:
            pass
        time.sleep(interval)

    out["rss_kb_avg"] = int(sum(rss_samples) / len(rss_samples)) if rss_samples else 0
    out["rss_kb_max"] = max(rss_samples) if rss_samples else 0
    # Average CPU % across the sampling window
    if cpu_deltas and interval > 0:
        avg_ticks_per_sec = (sum(cpu_deltas) / len(cpu_deltas)) / interval
        out["cpu_pct_avg"] = round(avg_ticks_per_sec / clk_tck * 100, 1)
    else:
        out["cpu_pct_avg"] = 0.0

# ── Single tier ────────────────────────────────────────────────────────────────
def run_tier(n_clients, server_proc, server_ip, client_port, duration, results_dir):
    """
    Launch n_clients, hold for `duration` s, collect stats, kill clients.
    Returns a result dict.
    """
    tier_dir = results_dir / f"clients_{n_clients:04d}"
    tier_dir.mkdir(parents=True, exist_ok=True)

    print(f"\n  [Tier {n_clients:>5} clients]", end="", flush=True)

    # Server must be alive before we start
    if server_proc.poll() is not None:
        print("  SKIPPED — server already dead")
        return {
            "n_clients": n_clients, "launched": 0,
            "alive_after_connect": 0, "alive_end": 0,
            "launch_time_s": 0, "server_alive": False,
            "rss_kb_max": 0, "rss_kb_avg": 0, "cpu_pct_avg": 0,
        }

    # Launch clients
    t0 = time.time()
    client_procs = launch_clients(server_ip, client_port, n_clients, tier_dir / "logs")
    launch_time  = round(time.time() - t0, 2)
    print(f"  launched {n_clients} clients in {launch_time}s", end="", flush=True)

    # Short settle time so clients can complete their subscribe handshake
    settle = min(5, max(2, duration // 6))
    time.sleep(settle)

    alive_after_connect = sum(1 for p in client_procs if p.poll() is None)
    print(f"  →  {alive_after_connect}/{n_clients} alive after {settle}s settle",
          end="", flush=True)

    # Background stats collection for the remaining duration
    remaining = max(5, duration - settle)
    stats      = {}
    stats_t    = threading.Thread(
        target=_collect_server_stats,
        args=(server_proc.pid, remaining, 2.0, stats),
        daemon=True,
    )
    stats_t.start()
    time.sleep(remaining)
    stats_t.join(timeout=5)

    alive_end    = sum(1 for p in client_procs if p.poll() is None)
    server_alive = server_proc.poll() is None
    status_str   = "server=OK" if server_alive else "server=CRASHED"
    print(f"  →  {alive_end} alive at end  ({status_str})")

    _kill_procs(client_procs)

    return {
        "n_clients":            n_clients,
        "launched":             len(client_procs),
        "alive_after_connect":  alive_after_connect,
        "alive_end":            alive_end,
        "launch_time_s":        launch_time,
        "server_alive":         server_alive,
        "rss_kb_max":           stats.get("rss_kb_max", 0),
        "rss_kb_avg":           stats.get("rss_kb_avg", 0),
        "cpu_pct_avg":          stats.get("cpu_pct_avg", 0.0),
    }

# ── Argument parsing ───────────────────────────────────────────────────────────
def _parse_args():
    ap = argparse.ArgumentParser(
        description="PRTP stress test — one server, escalating client counts",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    ap.add_argument("--server-ip",   default=DEFAULT_SERVER_IP,
                    help=f"Bind/connect IP (default {DEFAULT_SERVER_IP})")
    ap.add_argument("--sensor-port", type=int, default=DEFAULT_SENSOR_PORT,
                    help=f"Sensor → server port (default {DEFAULT_SENSOR_PORT})")
    ap.add_argument("--client-port", type=int, default=DEFAULT_CLIENT_PORT,
                    help=f"Client → server port (default {DEFAULT_CLIENT_PORT})")
    ap.add_argument("--counts",      type=int, nargs="+", default=DEFAULT_COUNTS,
                    metavar="N",
                    help=f"Client counts to test (default {DEFAULT_COUNTS})")
    ap.add_argument("--duration",    type=int, default=DEFAULT_DURATION,
                    help=f"Seconds per tier (default {DEFAULT_DURATION})")
    ap.add_argument("--num-sensors", type=int, default=DEFAULT_NUM_SENSORS,
                    help=f"Sensor simulators to run (default {DEFAULT_NUM_SENSORS})")
    ap.add_argument("--output-dir",  default=None,
                    help="Results directory (default stress_results_<timestamp>)")
    ap.add_argument("--no-sensors",  action="store_true",
                    help="Skip sensor simulators (clients subscribe to nothing)")
    ap.add_argument("--cooldown",    type=int, default=5,
                    help="Seconds between tiers for server to settle (default 5)")
    return ap.parse_args()

# ── Main ───────────────────────────────────────────────────────────────────────
def main():
    args = _parse_args()

    # Results directory
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    results_dir = Path(args.output_dir) if args.output_dir \
                  else SCRIPT_DIR / f"stress_results_{ts}"
    results_dir.mkdir(parents=True, exist_ok=True)

    # Sanity-check binaries
    for name, path in [("PRTP_server", SERVER_BIN), ("PRTP_client", CLIENT_BIN)]:
        if not path.is_file():
            print(f"ERROR: {name} binary not found at {path}")
            print("Run 'make' in the PRTP/ directory first.")
            sys.exit(1)
        if not os.access(path, os.X_OK):
            print(f"ERROR: {path} is not executable")
            sys.exit(1)

    # Raise FD limit (the launcher process opens 2 FH per tier log + misc)
    fd_raised = _raise_fd_limit(max(args.counts) * 4 + 512)

    _banner("PRTP Stress Test")
    print(f"  Server IP       : {args.server_ip}")
    print(f"  Sensor port     : {args.sensor_port}")
    print(f"  Client port     : {args.client_port}")
    print(f"  Client tiers    : {sorted(args.counts)}")
    print(f"  Duration / tier : {args.duration}s")
    print(f"  Cooldown / tier : {args.cooldown}s")
    print(f"  Sensors         : {'none (--no-sensors)' if args.no_sensors else args.num_sensors}")
    print(f"  Results dir     : {results_dir}")
    if fd_raised:
        print(f"  FD soft limit   : {fd_raised}")
    _sep()
    print()

    # ── Kill any leftover processes from a previous run ──────────────────────
    os.system("pkill -x PRTP_server 2>/dev/null; pkill -x PRTP_client 2>/dev/null; sleep 0.5")

    # ── Start PRTP_server ─────────────────────────────────────────────────────
    print("── Starting PRTP_server " + "─" * 47)
    server_proc, server_fh = start_server(
        args.server_ip, args.sensor_port, args.client_port, results_dir
    )
    _register(server_proc, server_fh)
    print(f"  PID {server_proc.pid}  |  log → {results_dir.name}/server.log")

    time.sleep(2)
    if server_proc.poll() is not None:
        print("ERROR: PRTP_server exited immediately. Tail of server.log:")
        try:
            print(open(results_dir / "server.log").read()[-1200:])
        except Exception:
            pass
        sys.exit(1)
    print("  Server is running OK")

    # ── Start sensor simulators ───────────────────────────────────────────────
    sensor_procs = []
    sensor_fh    = None
    if not args.no_sensors:
        print(f"\n── Starting {args.num_sensors} sensor simulators " + "─" * 37)
        sensor_procs, sensor_fh = start_sensors(
            args.server_ip, args.sensor_port, args.num_sensors, results_dir
        )
        for sp in sensor_procs:
            _register(sp)
        if sensor_fh:
            _register(subprocess.Popen(["true"]), sensor_fh)  # register FH for close
        print(f"  {len(sensor_procs)} sensors started  |  log → {results_dir.name}/sensors.log")
        time.sleep(2)  # let sensors register with the server
    else:
        print("\n  Sensors: skipped (--no-sensors)")

    # ── Stress tiers ──────────────────────────────────────────────────────────
    print(f"\n── Stress Tiers " + "─" * 56)
    print(f"  Each tier runs for {args.duration}s with a {args.cooldown}s cooldown.\n")

    results   = []
    all_counts = sorted(set(args.counts))

    for idx, n in enumerate(all_counts):
        result = run_tier(
            n_clients   = n,
            server_proc = server_proc,
            server_ip   = args.server_ip,
            client_port = args.client_port,
            duration    = args.duration,
            results_dir = results_dir,
        )
        results.append(result)

        if not result["server_alive"]:
            print(f"\n  !! Server crashed at {n} clients — stopping stress test.")
            break

        # Cooldown between tiers (skip after last)
        if idx < len(all_counts) - 1:
            print(f"  Cooldown {args.cooldown}s ...", end="", flush=True)
            time.sleep(args.cooldown)
            print(" done")

    # ── Shutdown server and sensors ───────────────────────────────────────────
    print(f"\n── Shutting down " + "─" * 54)
    _kill_procs(sensor_procs, timeout=1)
    try:
        server_proc.send_signal(signal.SIGINT)
        server_proc.wait(timeout=5)
    except Exception:
        try:
            server_proc.kill()
        except Exception:
            pass
    if server_fh:
        try:
            server_fh.close()
        except Exception:
            pass
    print("  All processes stopped.")

    # ── Summary table ─────────────────────────────────────────────────────────
    print()
    _banner("Stress Test Results")
    hdr = (
        f"  {'Clients':>7}  "
        f"{'Launch(s)':>9}  "
        f"{'Alive@conn':>10}  "
        f"{'Alive@end':>9}  "
        f"{'Success%':>8}  "
        f"{'Server':>6}  "
        f"{'RSS max':>9}  "
        f"{'CPU avg%':>8}"
    )
    print(hdr)
    print("  " + "─" * (len(hdr) - 2))
    for r in results:
        n  = r["n_clients"]
        pct = round(r["alive_end"] / n * 100, 1) if n else 0
        sv  = " OK" if r["server_alive"] else "CRASH"
        rss = f"{r['rss_kb_max']:,}" if r['rss_kb_max'] else "  N/A"
        cpu = f"{r['cpu_pct_avg']:.1f}%" if r['cpu_pct_avg'] else "  N/A"
        print(
            f"  {n:>7}  "
            f"{r.get('launch_time_s', 0):>9.2f}  "
            f"{r.get('alive_after_connect', 0):>10}  "
            f"{r['alive_end']:>9}  "
            f"{pct:>7.1f}%  "
            f"{sv:>6}  "
            f"{rss:>9}  "
            f"{cpu:>8}"
        )
    _sep()
    print()

    # Persist JSON
    out = {
        "timestamp": ts,
        "config": vars(args),
        "system": {
            "hostname": os.uname().nodename,
            "fd_limit": fd_raised,
        },
        "results": results,
    }
    json_path = results_dir / "stress_results.json"
    json_path.write_text(json.dumps(out, indent=2))
    print(f"  Full results saved → {json_path}")
    print()


if __name__ == "__main__":
    main()
