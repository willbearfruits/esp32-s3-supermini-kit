#!/bin/sh
# Build every shippable application and merge each into one image that flashes
# at offset 0 (bootloader + partition table + app), for people without ESP-IDF.
#   tools/release.sh            -> release/<version>/kit-<app>.bin
# Needs the IDF environment sourced. Leaves the tree on the JAM app.
set -e
cd "$(dirname "$0")/.."
ver=$(git describe --tags --always --dirty 2>/dev/null || echo dev)
out="release/$ver"; mkdir -p "$out"
for app in jam looper test; do
  tools/app.sh "$app" >/dev/null
  ( cd firmware && python -m esptool --chip esp32s3 merge_bin -o "../$out/kit-$app.bin" \
      --flash_mode dio --flash_freq 80m --flash_size 4MB \
      0x0 build/bootloader/bootloader.bin 0x8000 build/partition_table/partition-table.bin 0x10000 build/kit_instrument.bin )
  echo "built $out/kit-$app.bin"
done
tools/app.sh jam >/dev/null
cp enclosure/out/shell.stl enclosure/out/lid.stl enclosure/out/print_bed.stl "$out/"
cp parts/minimal-kit.csv parts/minimal-kit.pdf "$out/"
( cd "$out" && sha256sum * > SHA256SUMS )
ls -la "$out"
