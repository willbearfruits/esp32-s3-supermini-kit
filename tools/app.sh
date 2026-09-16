#!/bin/sh
# Switch the firmware application: tools/app.sh looper | instrument | test
# Edits sdkconfig (local, gitignored) and rebuilds. Flash with idf.py flash.
set -e
cd "$(dirname "$0")/../firmware"
case "$1" in
  looper) want=LOOPER ;; instrument) want=INSTRUMENT ;; test) want=TEST ;; faust) want=FAUST ;; jam) want=JAM ;;
  *) echo "usage: $0 looper|instrument|test|faust|jam"; exit 1 ;;
esac
for a in LOOPER INSTRUMENT TEST FAUST JAM; do
  if [ "$a" = "$want" ]; then sed -i "s/^# CONFIG_KIT_APP_$a is not set/CONFIG_KIT_APP_$a=y/; s/^CONFIG_KIT_APP_$a=n/CONFIG_KIT_APP_$a=y/" sdkconfig
  else sed -i "s/^CONFIG_KIT_APP_$a=y/# CONFIG_KIT_APP_$a is not set/" sdkconfig; fi
done
grep -q "^CONFIG_KIT_APP_$want=y" sdkconfig || echo "CONFIG_KIT_APP_$want=y" >> sdkconfig
idf.py build
