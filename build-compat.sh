#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
cc="$PWD/toolchain/usr/bin/x86_64-w64-mingw32-gcc"
"$cc" -shared -O2 -Wall -Wextra -Wno-cast-function-type \
    compat/version_proxy.c compat/audio_notifications.c compat/force_capture.c compat/clean_mic_probe.c compat/clean_mic_fix.c compat/version.def \
    -o compat/version.dll -static-libgcc -lole32 -luuid
"$cc" -shared -O2 -Wall -Wextra \
    -Itoolchain/usr/x86_64-w64-mingw32/sys-root/mingw/include/ddk \
    -Wl,--subsystem,native -Wl,--entry,DriverEntry -nostdlib \
    compat/vad_control.c -o compat/vmcompat-vad.sys -lntoskrnl -lgcc
for source in probe register_device audio_enum session_probe; do
    output="${source//_/-}"
    "$cc" -O2 -Wall -Wextra "compat/$source.c" -o "compat/$output.exe" -lole32 -luuid -lsetupapi -ladvapi32
done
printf 'Compatibility DLL, control driver and probes built.\n'
