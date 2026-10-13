"""Reset only synthetic plugin installations, never the user's game data."""
import ctypes as C
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile


def run_reset_tests(package, build):
    reset=package.S14TestReset
    reset.argtypes=[C.c_wchar_p,C.c_wchar_p,C.c_wchar_p];reset.restype=C.c_int
    install=package.S14TestInstall
    source=str(build/'SAN14ModManager.exe')
    error=C.create_unicode_buffer(192);backup=C.create_unicode_buffer(260)
    kernel=C.WinDLL('kernel32',use_last_error=True)
    create=kernel.CreateFileW
    create.argtypes=[C.c_wchar_p,C.c_uint,C.c_uint,C.c_void_p,C.c_uint,C.c_uint,C.c_void_p];create.restype=C.c_void_p
    close=kernel.CloseHandle;close.argtypes=[C.c_void_p]
    io=kernel.DeviceIoControl
    io.argtypes=[C.c_void_p,C.c_uint,C.c_void_p,C.c_uint,C.c_void_p,C.c_uint,C.POINTER(C.c_uint),C.c_void_p];io.restype=C.c_int
    def junction(link,destination):
        link.mkdir()
        substitute=('\\??\\'+str(destination)).encode('utf-16-le');display=str(destination).encode('utf-16-le')
        names=substitute+b'\0\0'+display+b'\0\0'
        mount=struct.pack('<HHHH',0,len(substitute),len(substitute)+2,len(display))+names
        payload=struct.pack('<IHH',0xA0000003,len(mount),0)+mount
        handle=create(str(link),0x40000000,0,None,3,0x02200000,None)
        assert handle not in (None,C.c_void_p(-1).value)
        try:
            buffer=C.create_string_buffer(payload);got=C.c_uint()
            assert io(handle,0x900A4,buffer,len(payload),None,0,C.byref(got),None)
        finally:close(handle)
    with tempfile.TemporaryDirectory(prefix='SAN14-reset-测试-') as directory:
        root=Path(directory).resolve()
        assert root.parent==Path(tempfile.gettempdir()).resolve()
        game=root/'SAN14PK_SC.exe';shutil.copyfile(build/'test_native.exe',game)
        original_game=game.read_bytes();save=root/'Save016.s14';save.write_bytes(b'native-save-untouched')
        assert install(str(root),source,error),error.value
        state=root/'SAN14ModManager';config=root/'SAN14BuildLimit.ini'
        old_config=b'[Manager]\nEnabled=1\n[Domestic]\nQuickSetupShown=1\nPreferences=old\n[Features]\nPluginTroops=1\nAiRandomAffix=1\n'
        config.write_bytes(old_config)
        for name in ['career/checkpoints/history.s14career','troops/unit.s14troops','ai-affixes/unit.s14aiaffix','blink-backstab/unit.s14blink','logs/plugin.jsonl','custom/future-data.bin']:
            f=state/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(name.encode())
        old_state={str(f.relative_to(state)):f.read_bytes() for f in state.rglob('*') if f.is_file()}
        binaries={n:(root/n).read_bytes() for n in ['dinput8.dll','SAN14ModManager.exe']}
        outside=root/'outside';outside.mkdir();(outside/'keep.txt').write_bytes(b'preserve-link-target')
        link=state/'linked';junction(link,outside)
        try:
            assert not reset(str(root),backup,error)
            assert config.read_bytes()==old_config and (outside/'keep.txt').read_bytes()==b'preserve-link-target'
        finally:link.rmdir()
        link=root/'SAN14ModManager-ResetBackups';junction(link,outside)
        try:
            assert not reset(str(root),backup,error)
            assert config.read_bytes()==old_config and list(outside.iterdir())==[outside/'keep.txt']
        finally:link.rmdir()
        # A running game and a locked config must leave every active file unchanged.
        proc=subprocess.Popen([str(game)],creationflags=0x4|subprocess.CREATE_NO_WINDOW)
        try:
            assert not reset(str(root),backup,error)
            assert config.read_bytes()==old_config
        finally:
            proc.terminate();proc.wait(timeout=5);proc._handle.Close()
        held=create(str(config),0x80000000,1,None,3,0,None)
        assert held not in (None,C.c_void_p(-1).value)
        try:
            assert not reset(str(root),backup,error),error.value
            assert config.read_bytes()==old_config
            assert {str(f.relative_to(state)):f.read_bytes() for f in state.rglob('*') if f.is_file()}==old_state
        finally:close(held)
        assert reset(str(root),backup,error),error.value
        saved=Path(backup.value)
        assert saved.parent==root/'SAN14ModManager-ResetBackups'
        assert (saved/'SAN14BuildLimit.ini').read_bytes()==old_config
        assert {str(f.relative_to(saved/'SAN14ModManager')):f.read_bytes() for f in (saved/'SAN14ModManager').rglob('*') if f.is_file()}==old_state
        assert sorted(f.name for f in state.iterdir())==['installation.ini']
        fresh=config.read_text('ascii')
        assert 'QuickSetupShown' not in fresh and 'Preferences' not in fresh
        for key in ['WallClusterLimit','PluginTroops','CaoRenAttackDefense','AiRandomAffix','HuangZhongBlink']:
            assert f'{key}=0' in fresh
        assert 'AutoSearch=1' in fresh and 'BattleEvents=1' in fresh and 'AutoDomesticFill=1' in fresh
        assert game.read_bytes()==original_game and save.read_bytes()==b'native-save-untouched'
        for name,data in binaries.items():
            assert (root/name).read_bytes()==data and (saved/name).read_bytes()==data
        assert package.S14TestOwned(str(root))==1
        # A second reset retains the first backup and is a separate transaction.
        assert reset(str(root),backup,error),error.value
        assert Path(backup.value)!=saved and (saved/'SAN14BuildLimit.ini').read_bytes()==old_config
        (root/'dinput8.dll').write_bytes(b'unknown-proxy')
        before=config.read_bytes()
        assert not reset(str(root),backup,error)
        assert config.read_bytes()==before and (root/'dinput8.dll').read_bytes()==b'unknown-proxy'
    return {'backup_complete':True,'native_save_and_game_preserved':True,'binaries_preserved':True,
            'quick_setup_marker_reset':True,'nonfoundation_defaults_off':True,'live_game_blocked':True,
            'locked_config_rollback':True,'unknown_dll_blocked':True,'reparse_subtree_blocked':True,
            'reparse_backup_blocked':True,'repeat_reset_separate_backups':True}
