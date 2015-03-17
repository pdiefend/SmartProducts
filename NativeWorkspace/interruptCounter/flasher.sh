#! /bin/bash

python /opt/Espressif/esptool-py/esptool.py --port /dev/ttyUSB0 write_flash 0x00000 firmware/0x00000.bin 0x40000 firmware/0x40000.bin

