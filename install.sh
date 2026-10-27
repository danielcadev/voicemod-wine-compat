#!/usr/bin/env bash
set -euo pipefail
if ! command -v python3 >/dev/null; then
    printf 'Python 3 is required. Install python3, then run this installer again.\n' >&2
    exit 1
fi
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -f "$script_dir/setup.py" && -f "$script_dir/compat/version_proxy.c" ]]; then
    exec python3 "$script_dir/setup.py" install "$@"
fi
# The standalone release asset retrieves the matching, versioned setup script.
exec python3 - "$@" <<'PY'
import pathlib, subprocess, sys, tempfile, urllib.request, urllib.error
url='https://github.com/danielcadev/voicemod-wine-compat/releases/download/v0.1.0/setup.py'
with tempfile.TemporaryDirectory(prefix='vmcompat-installer-') as directory:
    path=pathlib.Path(directory)/'setup.py'
    try:
        with urllib.request.urlopen(url,timeout=60) as response:
            path.write_bytes(response.read(256*1024))
    except (urllib.error.URLError,OSError):
        raise SystemExit('Could not download setup. Check your Internet connection and try again.')
    try:
        terminal=open('/dev/tty','rb')
    except OSError:
        terminal=None
    raise SystemExit(subprocess.call([sys.executable,str(path),'install',*sys.argv[1:]],stdin=terminal))
PY
