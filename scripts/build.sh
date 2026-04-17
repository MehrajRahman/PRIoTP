#!/bin/bash

set -e  # Exit on error

echo "==> Moving into PRIoTP/PRTP directory"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR/../PRTP" || { echo "Directory not found"; exit 1; }

echo "==> Fixing system time (NTP sync)"
sudo timedatectl set-ntp true || true
sudo timedatectl set-timezone Asia/Dhaka || true
sudo systemctl restart systemd-timesyncd || true

# Force hardware clock sync (useful for VirtualBox)
sudo hwclock --hctosys || true

echo "==> Updating package list"
if ! sudo apt update; then
    echo "⚠️ APT update failed due to time issue. Retrying with relaxed validation..."
    sudo apt -o Acquire::Check-Valid-Until=false update
fi

echo "==> Installing required dependencies"
sudo apt install -y \
    build-essential \
    autoconf \
    automake \
    libtool \
    pkg-config \
    check

echo "==> Cleaning previous builds"
make clean 2>/dev/null || true
make distclean 2>/dev/null || true

echo "==> Running autotools (autoreconf)"
autoreconf -fi

echo "==> Running configure"
./configure

echo "==> Building project"
make -j"$(nproc)"

echo "==> Running tests (optional)"
make check || true

echo "==> Installing project"
sudo make install

echo "==> Verifying installation"
make installcheck || true

echo "==> Done."