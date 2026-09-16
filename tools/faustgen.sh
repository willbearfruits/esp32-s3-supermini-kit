#!/bin/sh
# Regenerate firmware/main/faust/<name>.h from firmware/faust/<name>.dsp.
# Uses `faust` from PATH, or $FAUSTC (a libfaust driver taking: name file options...)
# with $FAUSTLIB pointing at a checkout of github.com/grame-cncm/faustlibraries.
set -e
cd "$(dirname "$0")/../firmware"
mkdir -p main/faust
for f in faust/*.dsp; do
  n=$(basename "$f" .dsp)
  opts="-lang cpp -cn kfx_$n -ftz 1 -single -o main/faust/$n.h"
  if command -v faust >/dev/null 2>&1; then
    faust $opts "$f"
  elif [ -n "$FAUSTC" ]; then
    "$FAUSTC" "$n" "$f" $opts ${FAUSTLIB:+-I "$FAUSTLIB"}
  else
    echo "need faust on PATH or FAUSTC=<driver> FAUSTLIB=<faustlibraries dir>"; exit 1
  fi
  echo "generated main/faust/$n.h"
done
