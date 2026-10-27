# Voicemod for Linux

[![Download installer](https://img.shields.io/badge/Download-Linux%20installer-51A2DA)](https://github.com/danielcadev/voicemod-wine-compat/releases/tag/v0.1.0)
[![Status: preview](https://img.shields.io/badge/status-preview-orange)](docs/VERIFICATION.md)
![Platform: Fedora verified](https://img.shields.io/badge/Linux-Fedora%20verified-51A2DA?logo=fedora&logoColor=white)

**Use Voicemod voice effects on Linux. Simple setup, an app-menu shortcut, and no compiling.**

An independent community project for running the official Windows application on Linux. Clean Mic, Custom Pitch, and Discord's local microphone test worked in the documented Fedora setup. This is an early preview, not an official Voicemod Linux application.

## Install

**Supported preview:** x86-64 Linux with PipeWire and Wine 11 or newer. The tested application is **Voicemod 3.17.40**. A different application build is detected and setup stops without patching it. Other distributions and every available voice have not been verified.

1. Download the **Windows installer** from the [official Voicemod website](https://www.voicemod.net/).
2. Download our small [install.sh](https://github.com/danielcadev/voicemod-wine-compat/releases/download/v0.1.0/install.sh).
3. Open a terminal in the download folder and run:

```sh
bash install.sh
```

Choose the official `.exe` when the file picker opens, then follow the normal Voicemod installation and sign-in. If a required system tool is missing, setup shows the package-manager command and asks before installing it. If Voicemod opens during the official installer, setup restarts that separate installation once to finish configuration.

When finished, open **Voicemod for Linux** from your applications menu. No compiler or custom Wine build is needed. Python 3 is required for setup; a graphical file picker is used when Zenity is available, otherwise you can paste the installer path in the terminal.

You can also pass the file directly:

```sh
bash install.sh ~/Downloads/VoicemodInstaller.exe
```

## Set up your microphone

- In Voicemod, choose your **physical microphone** and the headphones you use.
- In Discord or another voice app, choose **Microphone (Voicemod Virtual Audio Device (WDM))** as the input.
- Hear Myself can stay off. Use Discord's local microphone test to hear the result.

Setup leaves system audio defaults alone. Your account and paid voices continue to use Voicemod's normal sign-in and licensing.

## What setup does

- Downloads approximately 127 KiB of our compiled compatibility components and checks their SHA-256.
- Creates a separate environment at `~/.local/share/voicemod-for-linux/prefix`.
- Runs the official installer you selected; we do not redistribute Voicemod.
- Configures the virtual microphone and creates an application shortcut.
- Registers the browser's “open Voicemod” link for normal sign-in, without logging the link.

There is no Wine compilation or system Wine patch. Missing dependencies can be installed through your distribution package manager after confirmation. Existing unrelated Wine prefixes and the original research installation are not migrated or replaced.

## Preview status

The original application audio path passed controlled generated-tone/speech checks. The installer passed nine isolated behavior tests, desktop-entry validation, and a real fresh-prefix check of the compiled control device and audio-session notification probe. The first complete official-installer/account-login journey on a clean machine is **not yet verified**. Treat this release as a setup preview, not a guarantee that every machine or future application update will work.

See [verification details](docs/VERIFICATION.md), [installation troubleshooting](docs/INSTALLATION.md), and [Clean Mic investigation](docs/CLEAN_MIC.md).

## Technical details

The project uses a local `version.dll` compatibility layer, an independent control device running in Wine user space, and PipeWire. Exact-build hooks enable continuous capture while Voicemod is open and correct Wine's scanf behavior for XML parameter references. The original SDK keeps its own dynamic links; the official executable is not modified. This is not native client-activity detection or execution of the proprietary Windows kernel driver.

The source tree includes the build script, focused probes, and synthetic audio-analysis helpers. Users of the installer do not need those development tools. For source builds, use MinGW-w64 with DDK headers and import libraries:

```sh
./build-compat.sh
python3 package-runtime.py
python3 -m unittest discover -s tests -v
```

`VMCOMPAT_CC` and `VMCOMPAT_DDK_INCLUDE` can override the compiler/header paths. Building new runtime components changes the package hash; a release must update `RUNTIME_SHA256` in setup.py and publish matching assets. Do not substitute an unverified runtime package.

The Python audio helpers require NumPy, machine-specific endpoint configuration, and a separately generated 48 kHz/16-bit speech fixture at `compat/discord-speech.wav`. No recordings or fixture are distributed. Generated routing logs can contain local paths and must be reviewed before sharing.

## Related project

[Hamilt79/linux-voicemod](https://github.com/Hamilt79/linux-voicemod) already tackles Voicemod on Linux through [a modified Wine](https://github.com/Hamilt79/wine-voicemod), with both prebuilt and source-build options. Its README describes related driver, capture, and Clean Mic fixes. We have read that README, but have not tested or compared its implementation. No code from it is included here, and we make no first-solution or universal compatibility claim.

## What is distributed

Project-authored source, installer scripts, documentation, and compiled project compatibility components. No Wine prefix, account/session data, credentials, proprietary application/installer/driver/resource files, downloaded Wine DLLs, memory dumps, raw logs, or recordings are distributed. This project is not affiliated with Voicemod.
