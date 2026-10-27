#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
export WINEPREFIX="$PWD/wine-prefix"
export QTWEBENGINE_CHROMIUM_FLAGS="${QTWEBENGINE_CHROMIUM_FLAGS:---disable-gpu --disable-gpu-compositing}"
export WINEDLLOVERRIDES='version=n,b;d3d11,dxgi=n'
export VMCOMPAT_CAPTURE_ALWAYS="${VMCOMPAT_CAPTURE_ALWAYS:-1}"
export VMCOMPAT_FIX_FLOAT_REFS="${VMCOMPAT_FIX_FLOAT_REFS:-1}"
export WINEDEBUG="${WINEDEBUG:--all}"
app="$WINEPREFIX/drive_c/users/USERNAME/AppData/Local/VoicemodV3/app/last"
test -f "$app/Voicemod.exe"
test -f compat/version.dll
# Do not replace mapped DLLs underneath an existing application.
if ! cmp -s compat/version.dll "$app/version.dll"; then
    if python3 - "$WINEPREFIX" <<'PY'
import pathlib,sys
expected=('WINEPREFIX='+sys.argv[1]).encode()
for path in pathlib.Path('/proc').iterdir():
    if not path.name.isdigit(): continue
    try:
        args=(path/'cmdline').read_bytes().split(b'\0')
        env=(path/'environ').read_bytes().split(b'\0')
        if args and args[0].replace(b'\\',b'/').lower().endswith(b'/voicemod.exe') and expected in env:
            sys.exit(0)
    except OSError: pass
sys.exit(1)
PY
    then
        printf 'Close Voicemod before updating its compatibility DLL.\n' >&2
        exit 1
    fi
    cp compat/version.dll "$app/version.dll"
fi
if ! pactl -f json list sinks | python3 -c 'import json,sys;sys.exit(not any(x["name"]=="vm_compat_input" for x in json.load(sys.stdin)))'; then
    # Retain the bridge after launcher exit: the separate Voicemod process uses it.
    nohup pw-loopback -n vm_compat_probe -c 2 -l 10 \
        --capture-props='{ media.class = Audio/Sink node.name = vm_compat_input node.description = "Speakers (Voicemod Virtual Audio Device (WDM))" node.autoconnect = false }' \
        --playback-props='{ media.class = Audio/Source node.name = vm_compat_mic node.description = "Microphone (Voicemod Virtual Audio Device (WDM))" node.autoconnect = false }' \
        > pipewire-probe.log 2>&1 < /dev/null &
    for ((attempt=0; attempt<30; attempt++)); do
        if pactl list short sources | rg -q '\bvm_compat_mic\b'; then break; fi
        sleep 0.1
    done
fi
pactl list short sources | rg -q '\bvm_compat_mic\b'
if ! wine sc.exe query vmcompat > service-query.log 2>&1; then
    wine sc.exe create vmcompat type= kernel start= demand \
        binPath= "Z:${PWD//\//\\}\\compat\\vmcompat-vad.sys" > sc-create.log 2>&1
fi
wine compat/register-device.exe > registration-probe.log 2>&1
# Already running is fine; probe the device to distinguish that case from failure.
wine sc.exe start vmcompat > driver-short.log 2>&1 || true
wine compat/probe.exe --full --compat > full-control-probe-compat.log 2>&1
exec wine "$app/Voicemod.exe" --launcher >> launch-compat.log 2>&1
