#! /bin/sh
sudo rmmod usbserial
sudo rmmod ftdi_sio
sudo ./UNFLoader -r unflexa2.n64 -c 7101 -d -l