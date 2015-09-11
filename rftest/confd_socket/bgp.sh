#!/bin/bash

#this script does the following:
#- copies the bgpd.conf file to the Quagga directory of the lxc container
#- turns off the ospf daemon of quagga
#- turns on the bgp daemon of quagga
#- restarts the quagga service

# this file should be run after the necessary bgp configuration has been done 

if [ "$EUID" != "0" ]; then
  echo "You must be root to run this script. Sorry, dude!"
  exit 1
fi

QUAGGA_DIR="/etc/quagga"

CUR_DIR=`pwd`

#copying the bgpd.conf file to the quagga directory
cp -f $CUR_DIR/bgpd.conf $QUAGGA_DIR

#opening the file 'daemons'



#restarting the quagga service




