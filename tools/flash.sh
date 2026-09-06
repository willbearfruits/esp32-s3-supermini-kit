#!/usr/bin/env bash
# Flash the kit firmware over USB.
#
# The instrument firmware owns the USB port as a TinyUSB MIDI + serial device,
# so the usual USB Serial/JTAG path is not there. The firmware watches the
# serial port's DTR/RTS lines and reboots into the ROM bootloader when it sees
# esptool's reset dance; this script performs that dance, waits for the port
# to come back, then runs the normal flash.
#
# Usage: tools/flash.sh [port] [extra idf.py args, e.g. monitor]
set -e
cd "$(dirname "$0")/../firmware"
. ~/esp/esp-idf/export.sh >/dev/null
PORT=${1:-/dev/ttyACM0}
shift || true

python - "$PORT" <<'PY'
import os, sys, time
import serial
port = sys.argv[1]
try:
    s = serial.Serial(port, 115200, timeout=0.2)
except Exception as e:
    print(f"could not open {port}: {e}; assuming the board is already in the bootloader")
    sys.exit(0)
s.dtr = False; s.rts = True;  time.sleep(0.1)
s.dtr = True;  s.rts = False; time.sleep(0.1)
s.dtr = False; s.rts = False
try:
    s.close()
except Exception:
    pass
for _ in range(80):
    time.sleep(0.25)
    if os.path.exists(port):
        time.sleep(0.5)
        break
else:
    print("port did not come back; hold BOOT, tap RESET, and rerun")
    sys.exit(1)
PY

idf.py -p "$PORT" flash "$@"
