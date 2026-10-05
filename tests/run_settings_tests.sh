#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
mkdir -p build
powerpc-eabi-gcc -O2 -ffreestanding -fno-builtin -msdata=none -mcpu=750 \
    -nostdlib -Wa,-mregnames -Wl,-Ttext=0x101000,-e,TMSettings_Init \
    src/settings.c src/settings_game.c tests/settings_support.c tests/settings_hooks.S \
    -o build/settings-test.elf
python tests/test_settings.py -v
