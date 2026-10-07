"""Package verified mod-only installer; source ZIPs are not installers."""
from pathlib import Path
import argparse
import hashlib
import json
import zipfile

HERE=Path(__file__).resolve().parent
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-dir', type=Path, default=HERE/'build')
parser.add_argument('--dist-dir', type=Path, default=HERE/'dist')
args=parser.parse_args()
BUILD=args.build_dir.resolve()
DIST=args.dist_dir.resolve(); DIST.mkdir(parents=True,exist_ok=True)
version=(HERE.parent/'VERSION').read_text(encoding='utf-8').strip()
verification=json.loads((BUILD/'verification.json').read_text(encoding='utf-8'))
if verification.get('status') != 'passed':
    raise SystemExit('Native verification must pass before packaging')
payloads={}
for name,key in [('SAN14ModManager.exe','manager_exe_sha256'),('dinput8.dll','production_dll_sha256')]:
    data=(BUILD/name).read_bytes()
    if hashlib.sha256(data).hexdigest() != verification[key]:
        raise SystemExit('Verified artifact changed: '+name)
    payloads[name]=data
license_data=(HERE/'vendor/minhook/LICENSE.txt').read_bytes()
package=DIST/f'SAN14ModManager-{version}-windows-x64.zip'
guide=(HERE.parent/'docs/USER_GUIDE.md').read_bytes()
config=b'[Rule]\nMode=2\n[Manager]\nEnabled=1\n[Features]\nWallClusterLimit=1\nLimitHint=1\nDiagnostics=0\n'
manifest={'version':version,'release_status':'prerelease','platform':'windows-x64',
          'supported_game_sha256':'e6ae68925c266a19b05641913e60bf7d97d5eb4754901c3e82a5362d05ff7372',
          'manager_exe_sha256':verification['manager_exe_sha256'],
          'embedded_dll_sha256':verification['production_dll_sha256'],
          'manager_in_game_acceptance':'pending'}
files={'SAN14ModManager.exe':payloads['SAN14ModManager.exe'],
       '使用说明.md':guide,'MinHook-LICENSE.txt':license_data,'默认配置示例.ini':config,
       'release.json':(json.dumps(manifest,indent=2)+'\n').encode()}
files['SHA256.txt']=''.join(hashlib.sha256(data).hexdigest()+'  '+name+'\n' for name,data in files.items()).encode('utf-8')
with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED) as output:
    for name,data in files.items():
        entry=zipfile.ZipInfo(name,(2026,10,7,0,0,0)); entry.compress_type=zipfile.ZIP_DEFLATED
        output.writestr(entry,data)
digest=hashlib.sha256(package.read_bytes()).hexdigest()
(DIST/'SHA256SUMS.txt').write_text(digest+'  '+package.name+'\n',encoding='ascii')
print(json.dumps({'package':str(package),'sha256':digest,'entries':list(files)},ensure_ascii=True))
