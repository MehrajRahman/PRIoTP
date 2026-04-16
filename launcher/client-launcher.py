#!/usr/bin/python
# -*- coding: utf-8 -*-
"""
PRTP Client Launcher
Spawns multiple PRTP_client processes, each logging to its own directory.
Optionally runs monitor_server.py in the background for system metrics.

Usage:
  python3 client-launcher.py <server_ip> <server_port> <sim_time> <num_clients> [--monitor]

Example:
  python3 client-launcher.py localhost 5005 60 5
  python3 client-launcher.py localhost 5005 60 5 --monitor
"""

import sys
import subprocess
import os
import signal
import time

spawns = []  # list of spawned subprocesses

# Resolve absolute paths relative to this script's location
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PRTP_CLIENT_BIN = os.path.join(SCRIPT_DIR, "../PRTP/application/PRTP_client")
CLIENT_LOG_BASE = os.path.join(SCRIPT_DIR, "../PRTP/application")
MONITOR_SCRIPT  = os.path.join(SCRIPT_DIR, "monitor_server.py")

# Seconds to wait between launching each client (allows server to process subscription)
CLIENT_STAGGER_SECS = 1.0


def usage():
    print("client-launcher.py <server_ip> <server_port> <sim_time> <num_clients> [--monitor]")


def signal_handler(sig, frame):
    print("\nShutting down Client Launcher...")
    for s in spawns:
        try:
            os.kill(s.pid, signal.SIGINT)
        except Exception:
            pass
    sys.exit(0)


def main(argv):
    ipaddr    = "localhost"
    port      = "5005"
    sim_time  = 60
    N         = 1
    monitor   = False

    if len(argv) >= 1 and argv[0] in ("-h", "--help"):
        usage()
        sys.exit(0)

    # Parse positional args
    pos = [a for a in argv if not a.startswith("--")]
    if len(pos) >= 1: ipaddr   = pos[0]
    if len(pos) >= 2: port     = pos[1]
    if len(pos) >= 3: sim_time = int(pos[2])
    if len(pos) >= 4: N        = int(pos[3])

    # Parse flags
    if "--monitor" in argv:
        monitor = True

    # Resolve binary path absolutely
    client_bin = os.path.abspath(PRTP_CLIENT_BIN)
    if not os.path.isfile(client_bin):
        print(f"ERROR: PRTP_client binary not found at '{client_bin}'")
        print("Make sure you ran 'make' in the PRTP directory first.")
        sys.exit(1)

    print("=" * 60)
    print("PRTP Client Launcher")
    print("=" * 60)
    print(f"Server      : {ipaddr}:{port}")
    print(f"Clients     : {N}")
    print(f"Sim time    : {sim_time}s")
    print(f"Stagger     : {CLIENT_STAGGER_SECS}s between clients")
    print(f"Monitor     : {'yes' if monitor else 'no'}")
    print("=" * 60)
    print()

    start_time = time.time()

    # ── Optional: start monitor_server.py in the background ───────────────
    if monitor and os.path.isfile(MONITOR_SCRIPT):
        monitor_log = os.path.join(CLIENT_LOG_BASE, "monitor_metrics.json")
        monitor_proc = subprocess.Popen(
            [sys.executable, MONITOR_SCRIPT,
             "--interval", "1",
             "--duration", str(sim_time + 10),
             "--output", monitor_log],
            stdout=sys.stdout,
            stderr=subprocess.DEVNULL,
        )
        spawns.append(monitor_proc)
        print(f"[Monitor] Started (metrics -> {monitor_log})")
        time.sleep(0.5)

    # ── Launch each client ─────────────────────────────────────────────────
    for i in range(1, N + 1):
        # Use ABSOLUTE path so chdir() inside the binary doesn't break it
        log_dir = os.path.abspath(os.path.join(CLIENT_LOG_BASE, f"client{i}_sensor_log"))
        os.makedirs(log_dir, exist_ok=True)

        # Per-client stdout/stderr log so we can inspect errors later
        client_stdout_log = open(os.path.join(log_dir, "client.out"), "w")
        client_stderr_log = open(os.path.join(log_dir, "client.err"), "w")

        cmd = [
            client_bin,
            f"-s{ipaddr}",
            f"-p{port}",
            "-a",             # subscribe to all sensors
            f"-l{log_dir}",   # absolute log directory
        ]

        print(f"Starting client {i:>3} | log -> {log_dir}/")
        proc = subprocess.Popen(
            cmd,
            shell=False,
            stdout=client_stdout_log,
            stderr=client_stderr_log,
        )
        spawns.append(proc)

        # Stagger clients so server can process each LIST+SUBSCRIBE cycle
        if i < N:
            time.sleep(CLIENT_STAGGER_SECS)

    print(f"\nAll {N} clients started. Running for {sim_time}s ...")
    time.sleep(sim_time)

    print(f"\nKilling all clients after {round(time.time() - start_time, 1)}s ...")
    for s in spawns:
        try:
            os.kill(s.pid, signal.SIGINT)
        except Exception:
            pass

    # Wait for processes to exit cleanly
    for s in spawns:
        try:
            s.wait(timeout=5)
        except Exception:
            s.kill()

    print("All clients stopped.")

    # ── Summary ────────────────────────────────────────────────────────────
    print()
    print("=" * 60)
    print("LOG SUMMARY")
    print("=" * 60)
    for i in range(1, N + 1):
        log_dir = os.path.abspath(os.path.join(CLIENT_LOG_BASE, f"client{i}_sensor_log"))
        logs = [f for f in os.listdir(log_dir) if f.endswith(".log")]
        total_lines = 0
        for lf in logs:
            with open(os.path.join(log_dir, lf)) as fh:
                total_lines += sum(1 for _ in fh)
        print(f"  client{i}: {len(logs):>2} sensor log(s), {total_lines:>6} lines  -> {log_dir}/")
        # Print any errors
        err_file = os.path.join(log_dir, "client.err")
        if os.path.exists(err_file) and os.path.getsize(err_file) > 0:
            with open(err_file) as ef:
                errs = [l.strip() for l in ef if "error" in l.lower() or "failed" in l.lower()]
            if errs:
                print(f"           ERRORS: {errs[0]}")
    print("=" * 60)


if __name__ == "__main__":
    signal.signal(signal.SIGINT, signal_handler)
    main(sys.argv[1:])
