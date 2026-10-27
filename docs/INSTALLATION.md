# Installation and troubleshooting

## Normal use

Download install.sh from the v0.1.0 release and run `bash install.sh`. The script obtains the matching versioned setup script, and that script verifies the compiled runtime archive against its pinned SHA-256. Complete the vendor's normal installer and sign-in yourself.

The default installation directory is `~/.local/share/voicemod-for-linux` (or the corresponding XDG data directory). It is separate from ~/.wine and from the original private research prefix. Rerunning setup resumes that installation and preserves its account data. Setup restarts only its own managed prefix to finish installation. It refuses an existing unmanaged prefix or an unsupported application hash.

Desktop shortcuts are `voicemod-for-linux.desktop` and the hidden `voicemod-for-linux-login.desktop` in the user's applications directory. The latter handles `voicemod:` links through the official Windows protocol registration. Setup makes it the user's default handler for that scheme; it does not alter default audio devices. Authentication URLs are not written to our logs.

## Missing tools

The installer checks Wine, wineboot, wineserver, pactl, pw-loopback, and xdg-mime. It can offer distribution package commands for Fedora, Debian/Ubuntu, and Arch families. Package availability and runtime compatibility on the latter distributions have not been tested. It never automatically replaces an already present older Wine with a different Wine source; Wine 11 or newer must be available.

For an explicit noninteractive package-install choice, use `bash install.sh --install-dependencies /path/to/VoicemodInstaller.exe`. Your package manager still uses its own permission and transaction prompts.

Python 3 is required to start setup. If there is no graphical file picker, pass the official installer path directly or paste it when prompted. A graphical file picker requires Zenity.

## Unsupported application build

This preview supports the exact official Voicemod 3.17.40 executable used during validation. The vendor's online installer may download another version. Setup will stop with an explanatory message and preserve the installation rather than applying incompatible hooks. Do not bypass this check or obtain old executables from an untrusted mirror.

## Graphics components not found

Setup uses the WineD3D files supplied by your Wine installation. It selects actual Wine-built DLLs, copies them inside its dedicated prefix, and changes only the local builtin marker, following the tested setup. System files and distribution-wide alternatives are untouched, including when prefix files are symlinks or hardlinks.

If Wine is installed in an unusual location, set `VMCOMPAT_WINE_DLL_DIR` to its x86_64-windows directory before setup. No external graphics DLLs are bundled or downloaded by this project.

## Launch and diagnostics

After setup, use the application-menu shortcut. From a source checkout, launch-voicemod.sh calls the managed installation's launcher. The original private research launcher is separate and was not changed.

```sh
python3 ~/.local/share/voicemod-for-linux/setup.py doctor
python3 ~/.local/share/voicemod-for-linux/setup.py run
```

For a separate install location, append `--data-dir /absolute/path` to install, run, or doctor. Do not point it at an existing unrelated Wine prefix.

Logs live under the installation's private logs/ directory. The official application and installer may log account details; do not publish these logs, the prefix, sign-in URLs, or recordings without a separate review. Our repository ignore rules exclude generated output.

## Verification boundary

Nine installer behavior tests pass, including package integrity/path validation, preserving existing prefixes, application build gating, dependency selection, desktop paths, install/resume behavior, login routing without URL logging, and graphics-link isolation. Desktop files passed desktop-file-validate, including paths with spaces and reserved characters.

A real temporary Wine prefix also passed: fresh initialization, local WineD3D setup, driver registration/start, full control-device probe, and the actual audio-session callback probe. The prefix was cleaned up and existing audio defaults were unchanged. This did not execute the vendor installer or authenticate a fresh account. The full clean-install/login sequence still needs that separate integration trial.
