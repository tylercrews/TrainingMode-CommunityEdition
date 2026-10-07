#!/usr/bin/env bash
set -eo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
mkdir -p build
# Compile a fresh optimized production DAT too: ELF linking does not reproduce
# hmex/native-loader relocation limitations (including symbol address addends).
case "$(uname)" in
    MSYS*|MINGW*) settings_hmex=bin/hmex.exe; settings_hgecko=bin/hgecko.exe ;;
    Darwin*) settings_hmex=bin/hmex_macos_arm64; settings_hgecko=bin/hgecko_macos_arm64 ;;
    *) settings_hmex=bin/hmex; settings_hgecko=bin/hgecko ;;
esac
"${settings_hmex}" -q -l MexTK/melee.link -f "-O2 -w" -s tmFunction \
    -t MexTK/tmFunction.txt -o build/settings-relocated-test.dat \
    -i src/events.c src/menu.c src/osds.c src/savestate_v1.c src/settings.c src/settings_game.c \
       src/trails.c src/trails_game.c src/osd_context.c src/osd_context_game.c src/osd_style.c src/osd_style_game.c src/osd_editor_game.c \
       src/action_cues.c src/action_cues_game.c -dat dats/eventMenu.dat
powerpc-eabi-gcc -O2 -ffreestanding -fno-builtin -msdata=none -mcpu=750 \
    -nostdlib -Wa,-mregnames -Wl,-Ttext=0x101000,-e,TMSettings_Init \
    src/settings.c src/settings_game.c src/trails.c src/osd_context.c src/osd_context_game.c src/osd_style.c src/osd_style_game.c src/osd_editor_game.c src/action_cues.c src/action_cues_game.c src/ledgedash_logic.c tests/settings_support.c tests/settings_hooks.S \
    -o build/settings-test.elf
"${settings_hgecko}" -q ASM build/settings-codes-test.gct
python tests/test_settings.py -v
