# Voicemod on Linux: Wine Compatibility Shim

[![Status: experimental](https://img.shields.io/badge/status-experimental-orange)](docs/VERIFICATION.md)
![Platform: Fedora verified](https://img.shields.io/badge/Linux-Fedora%20verified-51A2DA?logo=fedora&logoColor=white)
![Wine: no rebuild](https://img.shields.io/badge/Wine-no%20rebuild-722F37)
![Audio: PipeWire](https://img.shields.io/badge/audio-PipeWire-blue)

**A local shim for official Voicemod on Linux, without rebuilding Wine.** Clean Mic, capture with Hear Myself off, and Discord local loopback were verified on the Fedora setup documented below.

An experimental compatibility layer for the official Voicemod Windows application on Wine and PipeWire. It uses a local `version.dll` proxy, a control device hosted by Wine's user-space driver process, and a PipeWire audio bridge. It does not require rebuilding or replacing the system Wine installation.

This is a sanitized source snapshot of a working local research setup, not a turnkey installer. Verified configuration: Fedora, Wine 11.0 Staging, PipeWire 1.6.9, official Voicemod 3.17.40 / frontend 23.181.0. Other distributions, application versions, and every available voice have not been validated. Build-specific hooks intentionally refuse mismatching executable layouts.

## What works in the tested setup

- Rendered application UI and recognized virtual audio endpoints.
- Control-device status and event lifecycle through an independent compatibility implementation.
- Capture with Hear Myself off, using an intentional continuous-capture policy while the application is open.
- Clean Mic loading with the original SDK's dynamic parameter links intact.
- Real Clean Mic processing and Custom Pitch processing.
- Discord's local microphone test receiving the virtual mic and returning matching processed tones and generated speech to the selected rear-headphone sink, with the front monitor silent.

See [verification summary](docs/VERIFICATION.md) and [Clean Mic investigation](docs/CLEAN_MIC.md). The user subsequently reported that audio worked, but there is no recorded acoustic measurement of the physical microphone or headphone transducers.

## Components

| File | Purpose |
| --- | --- |
| `compat/version_proxy.c` | Version API proxy, explicit Qt software-rendering flags, control-event compatibility |
| `compat/vad_control.c`, `compat/register_device.c` | Independent Wine control device and SetupAPI metadata |
| `compat/audio_notifications.c` | Poll actual Wine audio-session enumeration for notifications |
| `compat/force_capture.c` | Exact-build continuous capture policy independent of monitoring |
| `compat/clean_mic_fix.c` | Exact-build scanf return correction for XML parameter references |
| `compat/clean_mic_probe.c` | Optional diagnostic probe, disabled by default |
| `compat/*probe.c`, `compat/audio_enum.c` | Focused control, session, parsing, and endpoint probes |
| `clean_mic_inspect_links.py` | Read-only inspection of known SDK parameter links |
| `verify_audio.py`, `analyze_audio.py` | Synthetic audio capture routing and analysis helpers |

## Build and local setup

The original build used a private MinGW-w64 toolchain at `toolchain/usr`. Toolchain packages and binaries are intentionally excluded. `build-compat.sh` shows the compiler, DDK include, and linker requirements. Supply your own compatible compiler/toolchain and adjust these paths before building.

The launcher expects a private Wine prefix at `wine-prefix` and an official application installation. The published launcher and two probe sources contain `USERNAME` path placeholders replacing local personal paths. Update them for your local installation before use. Existing local WineD3D DLL setup is a prerequisite; those external DLLs are not included or automatically installed here.

```sh
./build-compat.sh
./launch-voicemod.sh
```

These are source-snapshot entry points, not instructions for an unconfigured machine. The running research installation was preserved separately and was not replaced by this sanitized snapshot. Portability and automatic setup remain future work.

The launcher defaults to `VMCOMPAT_CAPTURE_ALWAYS=1` and `VMCOMPAT_FIX_FLOAT_REFS=1`. The optional `VMCOMPAT_TRACE_CLEAN` diagnostic stays off. Normal startup has no remote-debugging port.

The Python audio helpers require NumPy and PulseAudio tools. `verify_audio.py` contains machine-specific endpoint names that must be adapted. It also expects a separately generated 48 kHz, 16-bit PCM speech fixture at `compat/discord-speech.wav`; no audio fixture is distributed. The analyzer's speech interval was chosen for the original fixture. Its routing snapshots can include local application names and paths, so generated output must be reviewed before sharing.

## Existing related work

[Hamilt79/linux-voicemod](https://github.com/Hamilt79/linux-voicemod) already addresses Voicemod on Linux using [a modified Wine](https://github.com/Hamilt79/wine-voicemod). Its README describes a stand-in control driver, processing without Listen to myself, and number-parsing fixes affecting Clean Mic. It offers both prebuilt Wine and a source-build route; users are not always required to compile Wine.

That project was identified after this local shim investigation. We have read its README but have not installed, executed, benchmarked, or compared its implementation. No code from it is included in this snapshot. The distinction here is the local shim/control bridge on the tested system Wine installation. We make no first-solution, universal compatibility, maintenance superiority, or inevitable fork-breakage claim.

## Publication scope

Only project-authored source, build/launch scripts, analysis helpers, and sanitized English documentation are included. No Wine prefix, account/session data, credentials, proprietary installers/executables/drivers/resources, third-party downloads, generated binaries, memory dumps, raw logs, or recordings are distributed. Obtain the official application separately and use its normal account and licensing flow. This project is not affiliated with Voicemod.

Work stopped after publishing this snapshot. See [handoff](docs/HANDOFF.md) for the preserved state and possible future work.
