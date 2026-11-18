#!/usr/bin/env bash
# For CI only: non-mutating symbol-size inspection of compiled ESP-IDF objects.
set -euo pipefail
command -v riscv32-esp-elf-nm >/dev/null || {
  echo "RISC-V nm unavailable; cannot assert symbols"; exit 1;
}
file=$(find build/esp-idf -type f -name 'stdio_vfs.c.obj' -print -quit)
if [[ -z "$file" ]]; then
  echo "stdio_vfs object not found"; exit 1
fi
echo "==== LARGEST STDIO OBJECT SYMBOLS ===="
riscv32-esp-elf-nm -S --size-sort "$file" | tail -n 45
echo "==== STDIO SECTION HEADERS ===="
riscv32-esp-elf-objdump -h "$file" | head -n 70
echo "==== LARGEST LVGL SYMBOLS ===="
archive=$(find build/esp-idf -type f -name 'liblvgl__lvgl.a' -print -quit)
if [[ -z "$archive" ]]; then
  echo "LVGL archive unavailable"; exit 1
fi
riscv32-esp-elf-nm -S --size-sort "$archive" | tail -n 55
echo "==== REVIEW ONLY, NO DEVICE WRITES ===="
