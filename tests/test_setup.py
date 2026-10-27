"""Installer behavior checks using isolated directories and mocked Wine commands."""
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import types
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('vmsetup',Path(__file__).resolve().parents[1]/'setup.py')
setup=importlib.util.module_from_spec(spec)
spec.loader.exec_module(setup)


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='vmcompat-setup-test-')
        self.root=Path(self.temp.name)
        self.base=self.root/'data with spaces'

    def tearDown(self):
        self.temp.cleanup()

    def archive(self, names=None):
        path=self.root/'runtime.tar.gz'
        with tarfile.open(path,'w:gz') as archive:
            for name in names or setup.RUNTIME_FILES:
                payload=b'MZ-test-component'
                member=tarfile.TarInfo(name)
                member.size=len(payload)
                archive.addfile(member,io.BytesIO(payload))
        return path

    def test_package_checksum_and_exact_allowlist(self):
        archive=self.archive()
        with patch.object(setup,'RUNTIME_SHA256',setup.digest(archive)):
            setup.unpack_runtime(archive,self.base)
        self.assertEqual({p.name for p in self.base.iterdir()},setup.RUNTIME_FILES)
        archive.write_bytes(b'altered download')
        with self.assertRaisesRegex(setup.SetupError,'integrity'):
            setup.unpack_runtime(archive,self.root/'bad')
        self.assertFalse((self.root/'bad').exists())

    def test_archive_cannot_write_outside_destination(self):
        archive=self.archive(['../outside'])
        with patch.object(setup,'RUNTIME_SHA256',setup.digest(archive)):
            with self.assertRaisesRegex(setup.SetupError,'Unexpected'):
                setup.unpack_runtime(archive,self.base)
        self.assertFalse((self.root/'outside').exists())

    def test_unmanaged_or_symlinked_prefix_is_preserved(self):
        prefix=self.base/'prefix'
        prefix.mkdir(parents=True)
        marker=prefix/'account-state-test'
        marker.write_text('preserve')
        with self.assertRaisesRegex(setup.SetupError,'unmanaged'):
            setup.ensure_managed(self.base)
        self.assertEqual(marker.read_text(),'preserve')
        prefix.rename(self.base/'original')
        prefix.symlink_to(self.base/'original')
        with self.assertRaises(setup.SetupError):
            setup.ensure_managed(self.base)
        self.assertEqual(marker.read_text(),'preserve')

    def test_app_discovery_has_no_username_assumption_and_gates_build(self):
        app=self.base/'drive_c/users/Different User/AppData/Local/VoicemodV3/app/last/Voicemod.exe'
        app.parent.mkdir(parents=True)
        app.write_bytes(b'mock app')
        with patch.object(setup,'APP_SHA256',setup.digest(app)):
            self.assertEqual(setup.app_path(self.base),app)
        with self.assertRaisesRegex(setup.SetupError,'not supported'):
            setup.app_path(self.base)
        self.assertEqual(app.read_bytes(),b'mock app')

    def test_dependency_commands_only_install_missing_tools(self):
        self.assertEqual(setup.dependency_command(['pactl'],'ID=fedora\n'),['sudo','dnf','install','pulseaudio-utils'])
        self.assertNotIn('wine',setup.dependency_command(['pw-loopback'],'ID=ubuntu\nID_LIKE=debian\n'))
        with self.assertRaises(setup.SetupError):
            setup.dependency_command(['wine'],'ID=unknown\n')

    def test_desktop_handles_spaces_percent_and_preserves_other_entries(self):
        entries=self.root/'applications'
        entries.mkdir()
        other=entries/'unrelated.desktop'
        other.write_text('preserve')
        with patch.object(setup,'data_home',return_value=self.root):
            entry=setup.desktop_entry(self.base/'percent%')
        text=entry.read_text()
        self.assertIn('Name=Voicemod for Linux',text)
        self.assertIn('percent%%',text)
        self.assertIn('"'+str(self.base),text)
        self.assertEqual(other.read_text(),'preserve')
        with self.assertRaises(setup.SetupError):
            setup.desktop_quote('invalid\npath')
        self.assertEqual(setup.desktop_quote('$'), '"'+('\\'*2)+'$"')
        self.assertEqual(setup.desktop_quote('\\'), '"'+('\\'*4)+'"')

    def test_prefix_graphics_copy_does_not_change_system_symlinks_or_hardlinks(self):
        system=self.root/'wine-dlls'
        system.mkdir()
        target=self.base/'prefix/drive_c/windows/system32'
        target.mkdir(parents=True)
        original=b'MZ'+bytes(62)+b'Wine builtin DLL'+b'test data'
        for name in ['d3d11','dxgi']:
            (system/('wine-'+name+'.dll')).write_bytes(original)
        (target/'d3d11.dll').symlink_to(system/'wine-d3d11.dll')
        os.link(system/'wine-dxgi.dll',target/'dxgi.dll')
        with patch.dict(os.environ,VMCOMPAT_WINE_DLL_DIR=str(system)):
            setup.prepare_wined3d(self.base)
        for name in ['d3d11','dxgi']:
            self.assertEqual((system/('wine-'+name+'.dll')).read_bytes(),original)
            self.assertFalse((target/(name+'.dll')).is_symlink())
            self.assertEqual((target/(name+'.dll')).read_bytes()[64:80],b'Wine local   DLL')

    def test_install_flow_and_resume_preserve_prefix_and_audio_defaults(self):
        installer=self.root/'official installer.exe'
        installer.write_bytes(b'MZ-mocked-installer')
        payload=b'mocked supported app'
        expected=hashlib.sha256(payload).hexdigest()
        calls=[]
        def runtime(base):
            directory=base/'components'/setup.VERSION
            directory.mkdir(parents=True,exist_ok=True)
            for name in setup.RUNTIME_FILES:
                (directory/name).write_bytes(b'MZ-test-'+name.encode())
            return directory
        def wine_command(base,args,label,check=True):
            calls.append((label,args,setup.wine_env(base)))
            if label=='official-installer':
                app=base/'prefix/drive_c/users/Another User/AppData/Local/VoicemodV3/app/last/Voicemod.exe'
                app.parent.mkdir(parents=True,exist_ok=True)
                app.write_bytes(payload)
            return types.SimpleNamespace(returncode=0)
        def command(args,**kwargs):
            calls.append(('command',args,kwargs.get('env')))
            return types.SimpleNamespace(returncode=0)
        args=types.SimpleNamespace(data_dir=self.base,installer=str(installer),install_dependencies=False,no_launch=True)
        with patch.object(setup,'APP_SHA256',expected), patch.object(setup,'check_dependencies'), \
             patch.object(setup,'prepare_runtime',side_effect=runtime), patch.object(setup,'prepare_wined3d'), \
             patch.object(setup,'control_device'), patch.object(setup,'ensure_bridge'), \
             patch.object(setup,'wine_command',side_effect=wine_command), patch.object(setup.subprocess,'run',side_effect=command), \
             patch.object(setup,'data_home',return_value=self.root):
            setup.install(args)
            saved=self.base/'prefix/account-state-test'
            saved.write_text('preserve')
            args.installer=None
            setup.install(args)
            self.assertEqual(saved.read_text(),'preserve')
        app=list((self.base/'prefix').glob('drive_c/users/*/AppData/Local/VoicemodV3/app/last/Voicemod.exe'))[0]
        self.assertEqual(app.read_bytes(),payload)
        self.assertTrue((app.parent/'version.dll').is_file())
        self.assertTrue((self.base/'setup.py').is_file())
        self.assertEqual(sum(c[0]=='official-installer' for c in calls),1)
        for label,args,env in calls:
            self.assertNotIn('set-default-sink',args)
            self.assertNotIn('set-default-source',args)
            if env:
                self.assertEqual(env['WINEPREFIX'],str(self.base/'prefix'))

    def test_login_uses_official_handler_without_logging_the_link(self):
        setup.ensure_managed(self.base)
        args=types.SimpleNamespace(data_dir=self.base,url='voicemod:test-fixture')
        with patch.object(setup.subprocess,'run') as command:
            setup.login(args)
        command.assert_called_once()
        self.assertEqual(command.call_args.kwargs['stdout'],subprocess.DEVNULL)
        self.assertEqual(command.call_args.kwargs['stderr'],subprocess.DEVNULL)
        self.assertEqual(command.call_args.kwargs['env']['WINEPREFIX'],str(self.base/'prefix'))
        args.url='https://example.invalid/'
        with patch.object(setup.subprocess,'run') as command:
            with self.assertRaises(setup.SetupError):
                setup.login(args)
            command.assert_not_called()


if __name__=='__main__':
    unittest.main()
