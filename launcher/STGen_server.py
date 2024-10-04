#!/usr/bin/python
# -*- coding: utf-8 -*-
import sys
import subprocess
import os
import shutil
import random
import signal
import time

spawns=[] #list of subprocesses spawned 

def usage():
    print( "iot-launcher.py <clients_configuration_file> <server_ip> <server_sensor_port> <server_client_port> <sim_time> ")

def signal_handler(signal, frame):
  print( 'Shutting down Launcher...')
  #probably need to close all the forked subprocesses?
  sys.exit(0)

def main(argv):
  ipaddr="localhost"
  port="5000"
  client_port="5001"
  client_config="../iot/conf/test.conf"
  sim_time=10 #seconds
  
  if(len(argv)>=1):
     client_config=argv[0]
     if(len(argv)>=3):
       ipaddr=argv[1]
       port=argv[2]
       if(len(argv)>=4):
         client_port=argv[3]
         if(len(argv)>=5):
           sim_time=int(argv[4])
           
               
  if len(argv)!=0:
    if (argv[0] == "-h" or argv[0]=="--help"):
      usage()
      sys.exit(0)
  else:
    print( "using Launcher defaults =>")

  print( "ip",ipaddr,"and port:",port)
  print( "sim_time",sim_time)
  
  start_time = round(time.time(), 3)
 
  #[STEP1]: starting IoT Server to handle input from sensors
  # NOTE: depending on your coding language the values in the list will change
  # Also implement SIGINT (Ctrl+C), so that we can kill them. 
  FNULL = open(os.devnull, 'w')
  spawns.append(subprocess.Popen(["../iot/application/iotserver", "-i"+ipaddr, "-p"+port, "-s"+client_port,
    "-l./sensor.list","-c"+client_config],shell=False,stdout=FNULL))
  
  time.sleep(sim_time)
  print( "killing processes after", (time.time()-start_time))
  for s in spawns:
    os.kill(s.pid, signal.SIGINT)

if __name__ == "__main__":
  signal.signal(signal.SIGINT, signal_handler)
  main(sys.argv[1:])
