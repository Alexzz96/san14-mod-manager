"""Exercise complete uninstall only in owned temporary folders."""
import ctypes as C
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


class Report(C.Structure):
    _fields_=[('detected',C.c_uint),('files',C.c_int),('directories',C.c_int),('preserved',C.c_int)]


def run_cleanup_tests(package, build, legacy=None):
    clean=package.S14TestClean
    clean.argtypes=[C.c_wchar_p,C.c_wchar_p,C.POINTER(Report),C.c_wchar_p];clean.restype=C.c_int
    detect=package.S14TestDetect
    detect.argtypes=[C.c_wchar_p,C.c_wchar_p];detect.restype=C.c_uint
    install=package.S14TestInstall
    source=str(build/'SAN14ModManager.exe')
    dll_data=(build/'dinput8.dll').read_bytes()
    manager_data=(build/'SAN14ModManager.exe').read_bytes()
    error=C.create_unicode_buffer(192)
    kernel=C.WinDLL('kernel32',use_last_error=True)
    create=kernel.CreateFileW
    create.argtypes=[C.c_wchar_p,C.c_uint,C.c_uint,C.c_void_p,C.c_uint,C.c_uint,C.c_void_p];create.restype=C.c_void_p
    close=kernel.CloseHandle;close.argtypes=[C.c_void_p];close.restype=C.c_int
    attributes=kernel.SetFileAttributesW;attributes.argtypes=[C.c_wchar_p,C.c_uint];attributes.restype=C.c_int
    io=kernel.DeviceIoControl
    io.argtypes=[C.c_void_p,C.c_uint,C.c_void_p,C.c_uint,C.c_void_p,C.c_uint,C.POINTER(C.c_uint),C.c_void_p];io.restype=C.c_int

    def junction(link, destination):
        link.mkdir()
        substitute=('\\??\\'+str(destination)).encode('utf-16-le')
        display=str(destination).encode('utf-16-le')
        names=substitute+b'\0\0'+display+b'\0\0'
        mount=struct.pack('<HHHH',0,len(substitute),len(substitute)+2,len(display))+names
        payload=struct.pack('<IHH',0xA0000003,len(mount),0)+mount
        file=create(str(link),0x40000000,0,None,3,0x02200000,None)
        assert file not in (None,C.c_void_p(-1).value)
        try:
            buffer=C.create_string_buffer(payload);got=C.c_uint()
            assert io(file,0x900A4,buffer,len(payload),None,0,C.byref(got),None),C.get_last_error()
        finally:assert close(file)

    cases=0
    with tempfile.TemporaryDirectory(prefix='SAN14-clean-测试-') as directory:
        base=Path(directory).resolve()
        assert base.parent==Path(tempfile.gettempdir()).resolve()
        outside=base/'outside';outside.mkdir();(outside/'keep.txt').write_bytes(b'outside-must-survive')
        def folder():
            nonlocal cases
            cases+=1;root=base/str(cases);root.mkdir()
            shutil.copyfile(build/'test_native.exe',root/'SAN14PK_SC.exe')
            (root/'save.sav').write_bytes(b'save-must-survive')
            return root
        def installed():
            root=folder();assert install(str(root),source,error)==1,error.value
            return root
        def cleanup(root, expected=1):
            before=(root/'SAN14PK_SC.exe').read_bytes()
            report=Report();assert clean(str(root),source,C.byref(report),error)==expected,error.value
            assert (root/'SAN14PK_SC.exe').read_bytes()==before
            assert (root/'save.sav').read_bytes()==b'save-must-survive'
            return report

        root=installed();state=root/'SAN14ModManager'
        (state/'runtime.ini').write_text('[Runtime]\nPid=42\nVersion=0.3.0\n',encoding='ascii')
        (state/'logs/plugin-20261007-120000-42.jsonl').write_text('{"event":"startup","wall_owner_source":"tile_current_force"}\n',encoding='ascii')
        (state/'backups/dinput8.previous.dll').write_bytes(dll_data)
        (state/'backups/manager.previous.exe').write_bytes(manager_data)
        report=cleanup(root)
        assert report.files==8 and report.directories==3 and report.preserved==0
        assert set(p.name for p in root.iterdir())=={'SAN14PK_SC.exe','save.sav'}

        root=installed();(root/'SAN14ModManager/installation.ini').unlink()
        assert detect(str(root),source)&7==7
        cleanup(root);assert not (root/'dinput8.dll').exists() and not (root/'SAN14ModManager').exists()

        root=folder();(root/'SAN14ModManager.exe').write_bytes(manager_data)
        assert detect(str(root),source)&2
        cleanup(root);assert not (root/'SAN14ModManager.exe').exists()

        root=installed();(root/'dinput8.dll').unlink();(root/'SAN14ModManager.exe').unlink()
        assert detect(str(root),source)&4
        cleanup(root);assert not (root/'SAN14ModManager').exists()

        root=folder();(root/'SAN14BuildLimit.ini').write_text('[Manager]\nEnabled=1\n[Rule]\nMode=2\n',encoding='ascii')
        assert detect(str(root),source)&4
        report=cleanup(root);assert report.files==1 and not (root/'SAN14BuildLimit.ini').exists()

        root=folder();(root/'dinput8.dll').write_bytes(b'unknown-dll');(root/'SAN14ModManager.exe').write_bytes(b'unknown-exe')
        cleanup(root,0);assert (root/'dinput8.dll').read_bytes()==b'unknown-dll' and (root/'SAN14ModManager.exe').read_bytes()==b'unknown-exe'

        root=installed();(root/'dinput8.dll').write_bytes(b'unknown-dll')
        (root/'SAN14ModManager/logs/notes.txt').write_bytes(b'foreign-notes')
        (root/'SAN14ModManager/backups/custom.dll').write_bytes(b'foreign-backup')
        (root/'SAN14ModManager/custom').mkdir()
        report=cleanup(root);assert report.preserved>=4
        assert (root/'dinput8.dll').read_bytes()==b'unknown-dll'
        assert (root/'SAN14ModManager/logs/notes.txt').read_bytes()==b'foreign-notes'
        assert (root/'SAN14ModManager/backups/custom.dll').read_bytes()==b'foreign-backup'
        assert not (root/'SAN14ModManager.exe').exists()

        root=installed();app=root/'SAN14ModManager.exe'
        held=create(str(app),0x80000000,1,None,3,0,None)
        assert held not in (None,C.c_void_p(-1).value)
        try:
            report=cleanup(root,0);assert report.files==0
            assert app.exists() and (root/'dinput8.dll').read_bytes()==dll_data
        finally:assert close(held)
        cleanup(root)

        root=installed();config=root/'SAN14BuildLimit.ini'
        assert attributes(str(config),1)
        try:
            report=cleanup(root,0);assert report.files==0 and (root/'dinput8.dll').exists()
        finally:assert attributes(str(config),0x80)
        cleanup(root)

        root=installed();shutil.rmtree(root/'SAN14ModManager')
        junction(root/'SAN14ModManager',outside)
        report=cleanup(root);assert report.preserved>=1 and (outside/'keep.txt').read_bytes()==b'outside-must-survive'
        assert (root/'SAN14ModManager').exists()
        (root/'SAN14ModManager').rmdir()

        root=installed();logs=root/'SAN14ModManager/logs';logs.rmdir();junction(logs,outside)
        report=cleanup(root);assert report.preserved>=1 and (outside/'keep.txt').exists()
        logs.rmdir()

        root=installed();alias=base/'root-alias';junction(alias,root)
        report=cleanup(alias,0);assert report.files==0 and (root/'dinput8.dll').exists()
        alias.rmdir();cleanup(root)

        root=installed();duplicate=outside/'linked.dll';duplicate.hardlink_to(root/'dinput8.dll')
        report=cleanup(root,0);assert report.files==0 and duplicate.read_bytes()==dll_data
        duplicate.unlink();cleanup(root)

        root=installed();app=root/'SAN14ModManager.exe'
        cli=subprocess.run([str(app),'--uninstall',str(root)],capture_output=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
        assert cli.returncode==2 and app.exists() and (root/'dinput8.dll').exists(),(cli.returncode,cli.stderr)
        cli=subprocess.run([source,'--uninstall',str(root)],capture_output=True,timeout=15,creationflags=subprocess.CREATE_NO_WINDOW)
        assert cli.returncode==0,(cli.returncode,cli.stderr)
        assert json.loads(cli.stdout)['files']==4 and not app.exists() and not (root/'SAN14ModManager').exists()

        # A launched executable can allow a DELETE handle but still reject
        # disposition. All earlier marks must be cancelled before closing them.
        root=installed();shutil.copyfile(build/'test_native.exe',root/'SAN14ModManager.exe')
        receipt=root/'SAN14ModManager/installation.ini'
        text=receipt.read_text(encoding='ascii')
        text=text.replace(hashlib.sha256(manager_data).hexdigest(),hashlib.sha256((root/'SAN14ModManager.exe').read_bytes()).hexdigest())
        receipt.write_text(text,encoding='ascii')
        child=subprocess.Popen([str(root/'SAN14ModManager.exe'),'--hold'],stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW)
        try:
            assert child.stdout.readline().strip()==b'ready'
            report=cleanup(root,0);assert report.files==0 and (root/'dinput8.dll').read_bytes()==dll_data
            assert (root/'SAN14ModManager/installation.ini').exists()
        finally:
            child.terminate();child.communicate(timeout=10)
        cleanup(root)

        if legacy:
            root=folder();(root/'SAN14ModManager.exe').write_bytes(legacy.resolve().read_bytes())
            assert detect(str(root),source)&2
            cleanup(root);assert not (root/'SAN14ModManager.exe').exists()

    return {'temporary_folder_cases':cases,'complete_uninstall':True,'missing_receipt':True,'orphan_receipt_and_config':True,
            'same_name_copy_detected':True,'legacy_copy_removed':bool(legacy),'unknown_files_preserved':True,
            'locked_files_preflight':True,'read_only_preflight':True,'junctions_not_followed':True,
            'hard_links_rejected':True,'running_self_rejected_before_changes':True,
            'external_cli_complete_uninstall':True,'game_and_save_unchanged':True}
