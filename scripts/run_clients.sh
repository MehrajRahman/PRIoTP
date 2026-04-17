#!/bin/bash

CLIENTS=10
CLIENT_DIR="$HOME/CS/PRIoTP/PRTP/application"
SERVER_IP="127.0.0.1"
PORT=5005

for ((i=1; i<=CLIENTS; i++))
do
  gnome-terminal -- bash -c "
    cd $CLIENT_DIR || exit;
    ./PRTP_client -l./client${i}_sensor_log -s$SERVER_IP -rtemp_${i} -p$PORT -A;
    exec bash" &

  echo "Launched client $i"
done