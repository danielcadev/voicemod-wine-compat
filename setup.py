#!/usr/bin/env python3
"""Voicemod for Linux: per-user setup and launcher, Python standard library only."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request

VERSION = '0.1.0'
REPOSITORY = 'danielcadev/voicemod-wine-compat'
RUNTIME_SHA256 = 'a7d3368ca257fc6a531d84a0c9ffd867e1bb7fad3e6db65f5d6dd1a72d2487d4'
APP_SHA256 = 'f62420ebb2452c33e0948b0cb175a44438e32134af2b332bc4c302b7d9497805'
RUNTIME_FILES = {'version.dll', 'vmcompat-vad.sys', 'probe.exe', 'register-device.exe',
                 'audio-enum.exe', 'session-probe.exe'}
BRIDGE_SINK = 'vm_compat_input'
BRIDGE_SOURCE = 'vm_compat_mic'


class SetupError(Exception):
    pass


def data_home():
    return Path(os.environ.get('XDG_DATA_HOME', str(Path.home()/'.local/share')))


def digest(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest() if hasattr(hashlib, 'file_digest') else hashlib.sha256(stream.read()).hexdigest()


def app_path(prefix):
    matches = list((prefix/'drive_c/users').glob('*/AppData/Local/VoicemodV3/app/last/Voicemod.exe'))
    if len(matches) != 1:
        raise SetupError('Voicemod was not found. Complete the official installer, then run setup again.')
    if digest(matches[0]) != APP_SHA256:
        raise SetupError('This Voicemod build is not supported yet. This release supports 3.17.40 only. '
                         'Your installation and account were preserved; no application file was patched.')
    return matches[0]


def download(url, destination, limit):
    request = urllib.request.Request(url, headers={'User-Agent': 'VoicemodForLinux/'+VERSION})
    with urllib.request.urlopen(request, timeout=60) as response, destination.open('wb') as output:
        remaining = limit
        while True:
            block = response.read(min(65536, remaining+1))
            if not block:
                break
            remaining -= len(block)
            if remaining < 0:
                raise SetupError('The download exceeded its expected size. Nothing was installed.')
            output.write(block)


def unpack_runtime(archive, destination):
    if digest(archive) != RUNTIME_SHA256:
        raise SetupError('The component download failed its integrity check. Please retry setup.')
    with tarfile.open(archive, 'r:gz') as tar:
        members = tar.getmembers()
        if len(members) != len(RUNTIME_FILES) or {m.name for m in members} != RUNTIME_FILES:
            raise SetupError('Unexpected files in the component package.')
        if any(not m.isfile() or m.size > 8*1024*1024 for m in members):
            raise SetupError('Invalid component package.')
        destination.mkdir(parents=True, exist_ok=True)
        for member in members:
            with tar.extractfile(member) as source:
                (destination/member.name).write_bytes(source.read())


def prepare_runtime(base):
    runtime = base/'components'/VERSION
    if all((runtime/name).is_file() for name in RUNTIME_FILES):
        return runtime
    print('[2/5] Downloading the small Linux compatibility package (no Wine compilation)...', flush=True)
    with tempfile.TemporaryDirectory(prefix='vmcompat-download-') as temporary:
        archive = Path(temporary)/'components.tar.gz'
        download(f'https://github.com/{REPOSITORY}/releases/download/v{VERSION}/vmcompat-runtime-x86_64.tar.gz',
                 archive, 16*1024*1024)
        unpack_runtime(archive, runtime)
    return runtime


def dependency_command(missing, os_release=None):
    if os_release is None:
        os_release = Path('/etc/os-release').read_text()
    values = dict(re.findall(r'^(\w+)=["\x27]?([^\n"\x27]*)', os_release, re.M))
    family = values.get('ID', '')+' '+values.get('ID_LIKE', '')
    if 'fedora' in family or 'rhel' in family:
        mapping = {'wine':'wine', 'wineboot':'wine', 'wineserver':'wine',
                   'pactl':'pulseaudio-utils', 'pw-loopback':'pipewire-utils', 'xdg-mime':'xdg-utils'}
        command = ['sudo', 'dnf', 'install']
    elif any(x in family for x in ['debian', 'ubuntu']):
        mapping = {'wine':'wine', 'wineboot':'wine', 'wineserver':'wine',
                   'pactl':'pulseaudio-utils', 'pw-loopback':'pipewire-bin', 'xdg-mime':'xdg-utils'}
        command = ['sudo', 'apt-get', 'install']
    elif 'arch' in family:
        mapping = {'wine':'wine', 'wineboot':'wine', 'wineserver':'wine',
                   'pactl':'libpulse', 'pw-loopback':'pipewire', 'xdg-mime':'xdg-utils'}
        command = ['sudo', 'pacman', '-S', '--needed']
    else:
        raise SetupError('Install these tools with your distribution package manager: '+', '.join(missing))
    return command+sorted({mapping[name] for name in missing})


def check_dependencies(allow_install=False):
    if platform.machine().lower() not in ('x86_64', 'amd64'):
        raise SetupError('This release is for x86-64 Linux computers.')
    required = ['wine', 'wineboot', 'wineserver', 'pactl', 'pw-loopback', 'xdg-mime']
    missing = [name for name in required if not shutil.which(name)]
    if missing:
        command = dependency_command(missing)
        print('Required system tools are missing. Install them with:\n  '+' '.join(command), flush=True)
        accepted = allow_install or (sys.stdin.isatty() and input('Install the missing packages now? [y/N] ').strip().lower() == 'y')
        if not accepted:
            raise SetupError('Run the package command above, then start setup again.')
        subprocess.run(command, check=True)
        if any(not shutil.which(name) for name in required):
            raise SetupError('Some required tools are still missing. Please check the package-manager output.')
    version = subprocess.check_output(['wine', '--version'], text=True).strip()
    match = re.search(r'wine-(\d+)', version)
    if not match or int(match[1]) < 11:
        raise SetupError('Wine 11 or newer is required; the tested version is Wine 11.0 Staging. Found: '+version)
    subprocess.run(['pactl', 'info'], check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return version


def choose_installer(value):
    if value:
        path = Path(value).expanduser().resolve()
    elif shutil.which('zenity') and (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
        result = subprocess.run(['zenity', '--file-selection', '--title=Choose the official Voicemod Windows installer',
                                 '--file-filter=Windows installer | *.exe', '--filename='+str(Path.home()/'Downloads')+'/'],
                                text=True, capture_output=True)
        if result.returncode:
            raise SetupError('Setup cancelled. Existing files were preserved.')
        path = Path(result.stdout.strip()).resolve()
    elif sys.stdin.isatty():
        print('Download the Windows installer from https://www.voicemod.net/ first.')
        path = Path(input('Drag the Voicemod installer here, or paste its full path: ').strip().strip('"\x27')).expanduser().resolve()
    else:
        raise SetupError('Run install.sh /path/to/VoicemodInstaller.exe, or install zenity for a graphical file picker.')
    if not path.is_file() or path.suffix.lower() != '.exe':
        raise SetupError('Please select the official Voicemod Windows installer (.exe).')
    with path.open('rb') as stream:
        if stream.read(2) != b'MZ':
            raise SetupError('Please select the official Voicemod Windows installer (.exe).')
    return path


def wine_env(base):
    return dict(os.environ, WINEPREFIX=str(base/'prefix'), WINEARCH='win64',
                WINEDEBUG='-all', QTWEBENGINE_CHROMIUM_FLAGS='--disable-gpu --disable-gpu-compositing',
                WINEDLLOVERRIDES='version=n,b;d3d11,dxgi=n',
                VMCOMPAT_CAPTURE_ALWAYS='1', VMCOMPAT_FIX_FLOAT_REFS='1')


def wine_command(base, args, label, check=True):
    logs = base/'logs'
    logs.mkdir(exist_ok=True)
    with (logs/(label+'.log')).open('ab') as log:
        return subprocess.run(['wine', *map(str,args)], env=wine_env(base), stdout=log, stderr=log, check=check)


def ensure_managed(base):
    marker = base/'.managed-prefix'
    prefix = base/'prefix'
    if prefix.is_symlink() or (prefix.exists() and not marker.is_file()):
        raise SetupError('Refusing to modify an existing unmanaged Wine prefix. Choose a new --data-dir.')
    base.mkdir(parents=True, exist_ok=True, mode=0o700)
    base.chmod(0o700)
    marker.write_text('Voicemod for Linux: dedicated prefix\n')


def prepare_wined3d(base):
    # Reuse local Wine components; never replace distribution-wide alternatives.
    supplied = os.environ.get('VMCOMPAT_WINE_DLL_DIR')
    directories = [Path(supplied)] if supplied else []
    directories += [p for pattern in ['/usr/lib*/wine*/x86_64-windows',
                                      '/usr/lib*/wine*/wine/x86_64-windows',
                                      '/usr/lib/*-linux-gnu/wine/x86_64-windows',
                                      '/opt/wine*/lib*/wine/x86_64-windows'] for p in Path('/').glob(pattern.lstrip('/'))]
    for name in ['d3d11', 'dxgi']:
        source = None
        for directory in directories:
            for filename in ['wine-'+name+'.dll', name+'.dll']:
                candidate = directory/filename
                if candidate.is_file():
                    with candidate.open('rb') as stream:
                        if stream.read(80)[64:80] == b'Wine builtin DLL':
                            source = candidate
                            break
            if source:
                break
        if not source:
            raise SetupError('Wine graphics components were not found. Install WineD3D or set VMCOMPAT_WINE_DLL_DIR.')
        data = source.read_bytes()
        # Same prefix-local marker change used in the verified research setup.
        data = data[:64]+b'Wine local   DLL'+data[80:]
        target = base/'prefix/drive_c/windows/system32'/(name+'.dll')
        if target.exists() or target.is_symlink():
            target.unlink()
        target.write_bytes(data)


def control_device(base, runtime):
    path = 'Z:'+str(runtime/'vmcompat-vad.sys').replace('/', '\\')
    query = wine_command(base, ['sc.exe','query','vmcompat'], 'control-query', check=False)
    if query.returncode:
        wine_command(base, ['sc.exe','create','vmcompat','type=','kernel','start=','demand','binPath=',path], 'control-create')
    else:
        wine_command(base, ['sc.exe','config','vmcompat','binPath=',path], 'control-config')
    wine_command(base, [runtime/'register-device.exe'], 'device-registration')
    wine_command(base, ['sc.exe','start','vmcompat'], 'control-start', check=False)
    wine_command(base, [runtime/'probe.exe','--full','--compat'], 'control-probe')


def ensure_bridge(base):
    sources = json.loads(subprocess.check_output(['pactl','-f','json','list','sources'], text=True))
    sinks = json.loads(subprocess.check_output(['pactl','-f','json','list','sinks'], text=True))
    have_source = any(s['name'] == BRIDGE_SOURCE for s in sources)
    have_sink = any(s['name'] == BRIDGE_SINK for s in sinks)
    if have_source and have_sink:
        return
    if have_source or have_sink:
        raise SetupError('An incomplete Voicemod audio bridge exists. Close other Voicemod sessions and retry.')
    with (base/'logs/pipewire.log').open('ab') as log:
        child = subprocess.Popen(['pw-loopback','-n','vm_compat_bridge','-c','2','-l','10',
            '--capture-props={ media.class = Audio/Sink node.name = vm_compat_input node.description = "Speakers (Voicemod Virtual Audio Device (WDM))" node.autoconnect = false }',
            '--playback-props={ media.class = Audio/Source node.name = vm_compat_mic node.description = "Microphone (Voicemod Virtual Audio Device (WDM))" node.autoconnect = false }'],
            stdout=log, stderr=log, start_new_session=True)
    import time
    for _ in range(50):
        sources = json.loads(subprocess.check_output(['pactl','-f','json','list','sources'], text=True))
        if any(s['name'] == BRIDGE_SOURCE for s in sources):
            return
        if child.poll() is not None:
            break
        time.sleep(.1)
    child.terminate()
    raise SetupError('The virtual microphone could not start. Check that PipeWire is running.')


def desktop_quote(value):
    if '\n' in value or '\r' in value:
        raise SetupError('Installation paths cannot contain newlines.')
    # Exec quoting is followed by the desktop-file string escaping layer.
    # https://specifications.freedesktop.org/desktop-entry/latest/exec-variables.html
    escaped=''.join('\\'+c if c in '\\"`$' else c for c in value)
    return '"'+escaped.replace('\\','\\\\').replace('%','%%')+'"'


def desktop_entry(base):
    entries = data_home()/'applications'
    entries.mkdir(parents=True, exist_ok=True)
    command = ' '.join(desktop_quote(str(x)) for x in [sys.executable, base/'setup.py', 'run', '--data-dir', base])
    entry = entries/'voicemod-for-linux.desktop'
    entry.write_text('[Desktop Entry]\nType=Application\nName=Voicemod for Linux\n'
                     'Comment=Voice effects for Discord and other audio apps\n'
                     'Exec='+command+'\nIcon=audio-input-microphone\nTerminal=false\nCategories=AudioVideo;Audio;\n')
    entry.chmod(0o755)
    login_command=' '.join(desktop_quote(str(x)) for x in [sys.executable,base/'setup.py','login','--data-dir',base])
    handler=entries/'voicemod-for-linux-login.desktop'
    handler.write_text('[Desktop Entry]\nType=Application\nName=Voicemod sign-in\nNoDisplay=true\n'
                       'Exec='+login_command+' %u\nTerminal=false\nMimeType=x-scheme-handler/voicemod;\n')
    return entry


def install(args):
    print('Voicemod for Linux — easy setup\n[1/5] Checking your computer...', flush=True)
    check_dependencies(args.install_dependencies)
    base = args.data_dir
    ensure_managed(base)
    app_installed = list((base/'prefix/drive_c/users').glob('*/AppData/Local/VoicemodV3/app/last/Voicemod.exe'))
    installer = None if app_installed else choose_installer(args.installer)
    if args.installer and app_installed:
        installer = choose_installer(args.installer)
    runtime = prepare_runtime(base)
    print('[3/5] Preparing a separate Windows environment for Voicemod...', flush=True)
    (base/'logs').mkdir(exist_ok=True)
    if (base/'prefix').exists():
        print('Restarting only this installation of Voicemod to apply setup...', flush=True)
        subprocess.run(['wineserver','-k'], env=wine_env(base), check=True)
    with (base/'logs/prefix.log').open('ab') as log:
        subprocess.run(['wineboot','-u'], env=wine_env(base), stdout=log, stderr=log, check=True)
    prepare_wined3d(base)
    control_device(base, runtime)
    ensure_bridge(base)
    if installer:
        print('[4/5] Complete the official Voicemod installer. Review its terms normally.\n'
              'If Voicemod opens during installation, it will restart once to finish setup.', flush=True)
        wine_command(base, [installer, '/NORESTART'], 'official-installer')
        # Only our dedicated, marked prefix; never the user's default/other prefixes.
        subprocess.run(['wineserver','-k'], env=wine_env(base), check=True)
    app = app_path(base/'prefix')
    proxy=app.parent/'version.dll'
    if proxy.exists() and digest(proxy) not in {digest(p) for p in (base/'components').glob('*/version.dll')}:
        raise SetupError('The application already contains a different version.dll. It was preserved; setup cannot replace it safely.')
    if proxy.exists() or proxy.is_symlink():
        proxy.unlink()
    shutil.copyfile(runtime/'version.dll', proxy)
    installed_script = base/'setup.py'
    if Path(__file__).resolve() != installed_script:
        shutil.copyfile(__file__, installed_script)
    entry = desktop_entry(base)
    subprocess.run(['xdg-mime','default','voicemod-for-linux-login.desktop','x-scheme-handler/voicemod'],check=True)
    print('[5/5] Ready! Open “Voicemod for Linux” from your applications menu.\n'
          'Choose your physical microphone and headphones inside Voicemod.\n'
          'In Discord, choose “Microphone (Voicemod Virtual Audio Device (WDM))”.\n'
          'Your system audio defaults were not changed.\nShortcut: '+str(entry), flush=True)
    if not args.no_launch:
        subprocess.Popen([sys.executable,str(installed_script),'run','--data-dir',str(base)],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)


def run_app(args):
    base = args.data_dir
    if not (base/'.managed-prefix').is_file():
        raise SetupError('Run install.sh first to set up Voicemod for Linux.')
    app = app_path(base/'prefix')
    runtime = base/'components'/VERSION
    if any(not (runtime/name).is_file() for name in RUNTIME_FILES):
        raise SetupError('Compatibility components are missing. Run install.sh again.')
    with (base/'launcher.lock').open('w') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        if digest(app.parent/'version.dll') != digest(runtime/'version.dll'):
            raise SetupError('Compatibility files changed. Close Voicemod, then run install.sh again.')
        (base/'logs').mkdir(exist_ok=True)
        ensure_bridge(base)
        control_device(base, runtime)
        wine_command(base, [app,'--launcher'], 'voicemod')


def login(args):
    if not (args.data_dir/'.managed-prefix').is_file():
        raise SetupError('Run install.sh first.')
    if not args.url.lower().startswith('voicemod:') or len(args.url)>32768 or any(ord(c)<33 or c in '"\\' for c in args.url):
        raise SetupError('This is not a Voicemod sign-in link.')
    # Use the protocol registered by the official Windows installer. Do not log
    # the URL, which may contain a short-lived authentication credential.
    subprocess.run(['wine','start.exe',args.url],env=wine_env(args.data_dir),
                   stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,check=True)


def main():
    parser = argparse.ArgumentParser(description='Voicemod for Linux: easy setup and launcher')
    commands = parser.add_subparsers(dest='action', required=True)
    default = data_home()/'voicemod-for-linux'
    for name in ['install','run','doctor','login']:
        command = commands.add_parser(name)
        command.add_argument('--data-dir', type=lambda s:Path(s).expanduser().resolve(), default=default.resolve())
        if name == 'install':
            command.add_argument('installer', nargs='?', help='Official Voicemod Windows installer (.exe)')
            command.add_argument('--install-dependencies', action='store_true', help='Use your package manager for missing tools')
            command.add_argument('--no-launch', action='store_true', help='Create the shortcut without opening Voicemod')
        if name == 'login':
            command.add_argument('url',help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.action == 'install':
        install(args)
    elif args.action == 'run':
        run_app(args)
    elif args.action == 'login':
        login(args)
    else:
        print('System tools: OK — '+check_dependencies())
        print('Application: '+str(app_path(args.data_dir/'prefix')))


if __name__ == '__main__':
    try:
        main()
    except (SetupError, subprocess.CalledProcessError, OSError, ValueError, tarfile.TarError) as error:
        message = str(error) if isinstance(error, SetupError) else 'Setup could not complete. Check your connection, system tools, and the private logs in the installation directory.'
        print('Voicemod for Linux: '+message, file=sys.stderr)
        if len(sys.argv)>1 and sys.argv[1]=='run' and shutil.which('zenity') and (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
            subprocess.run(['zenity','--error','--title=Voicemod for Linux','--text='+message])
        sys.exit(1)
