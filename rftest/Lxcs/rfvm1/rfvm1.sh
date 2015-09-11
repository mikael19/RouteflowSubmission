#!/bin/bash

if [ "$EUID" != "0" ]; then
  echo "You must be root to run this script. Sorry, dude!"
  exit 1
fi


mkdir /home/ubuntu/confdInstalled
echo "directory for installed confd created"

cd /home/ubuntu/confd_src
sh confd-basic-5.4.linux.x86_64.installer.bin /home/ubuntu/confdInstalled
echo "confd was installed successfully"

CONFD="/home/ubuntu/confdInstalled"
cd $CONFD

#copying the bgp config files
cp -rf /home/ubuntu/Lxcs/rfvm1/bgp.fxs /home/ubuntu/confdInstalled/etc/confd
cp -rf /home/ubuntu/Lxcs/rfvm1/bgp.yang /home/ubuntu/confdInstalled/etc/confd
source $CONFD/confdrc
cd bin
./confd --foreground --verbose &
CONFD_PID=$!

sleep 5

#copy the Makefile to the confd_socket
cd /home/ubuntu/Lxcs/rfvm1/
cp -rf Makefile /home/ubuntu/confd_socket/

#copy socket program to confd directory
cd /home/ubuntu
cp -rf confd_socket /home/ubuntu/confdInstalled

#start the confd_socket program
cd /home/ubuntu/confdInstalled/confd_socket
./bgpconf &

sleep 5

PS3='do you want to configure the lxc?: '
options=("Yes" "No" "Quit")
select opt in "${options[@]}"
do
    case $opt in
        "Yes")
            cd /home/ubuntu/confdInstalled/bin
	    ./confd_cli -C
#copy the daemons file to the quagga directory to allow the bgp configuration to  be read accordingly
	    cp -rf /home/ubuntu/Lxcs/daemons /etc/quagga
	    cp -rf /home/ubuntu/Lxcs/ospfd.conf /etc/quagga
	    cp -rf /home/ubuntu/Lxcs/zebra.conf /etc/quagga
	    cp -rf /home/ubuntu/Lxcs/vtysh.conf /etc/quagga
            ;;
        "No")
            ls
            ;;
        "Quit")
            break
            ;;
        *) echo invalid option;;
    esac
done

