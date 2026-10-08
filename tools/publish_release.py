"""Publish verified installer assets for an explicitly pushed GitHub version tag."""
from pathlib import Path
import hashlib
import json
import os
import re
import urllib.error
import urllib.parse
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
VERSION=(ROOT/'VERSION').read_text(encoding='utf-8').strip()
TAG='v'+VERSION
REPO='Alexzz96/san14-mod-manager'
if not re.fullmatch(r'\d+\.\d+\.\d+',VERSION):
    raise SystemExit('VERSION must be numeric major.minor.patch')
if os.environ.get('GITHUB_REPOSITORY')!=REPO or os.environ.get('GITHUB_REF')!='refs/tags/'+TAG:
    raise SystemExit('Publishing requires the matching version tag in the project repository')
TOKEN=os.environ.get('GITHUB_TOKEN')
if not TOKEN:
    raise SystemExit('GITHUB_TOKEN is required')
PROOF=json.loads((ROOT/'native/build/verification.json').read_text(encoding='utf-8'))
if PROOF.get('status')!='passed':
    raise SystemExit('Native verification did not pass')
DIST=ROOT/'native/dist'
NAMES=[f'SAN14ModManager-{VERSION}-windows-x64.exe',f'SAN14ModManager-{VERSION}-windows-x64.zip','SHA256SUMS.txt']
FILES={name:(DIST/name).read_bytes() for name in NAMES}
if hashlib.sha256(FILES[NAMES[0]]).hexdigest()!=PROOF['manager_exe_sha256']:
    raise SystemExit('Installer differs from verified build')

def api(path,method='GET',body=None,mime=None):
    url=path if path.startswith('https://') else 'https://api.github.com/repos/'+REPO+path
    if urllib.parse.urlsplit(url).hostname not in ('api.github.com','uploads.github.com'):
        raise SystemExit('Unexpected publication host')
    data=body if mime else json.dumps(body,ensure_ascii=False).encode('utf-8') if body is not None else None
    request=urllib.request.Request(url,data=data,method=method,headers={
        'Authorization':'Bearer '+TOKEN,'Accept':'application/vnd.github+json',
        'User-Agent':'SAN14ModManager-release','X-GitHub-Api-Version':'2022-11-28',
        'Content-Type':mime or 'application/json'})
    try:
        with urllib.request.urlopen(request,timeout=60) as response:return json.load(response)
    except urllib.error.HTTPError as error:
        raise SystemExit(f'GitHub publication failed with HTTP {error.code}') from None

releases=api('/releases?per_page=30')
matching=[release for release in releases if release['tag_name']==TAG]
if len(matching)>1:
    raise SystemExit('Multiple releases use this tag')
release=matching[0] if matching else None
notes=ROOT/'docs'/f'RELEASE_NOTES_{VERSION}.md'
body=notes.read_text(encoding='utf-8') if notes.exists() else (
    f'安装前请保存并退出游戏，运行新版独立安装器。在线更新保留已有设置。\n\n'
    f'[变更记录](https://github.com/{REPO}/blob/{TAG}/CHANGELOG.md) · '
    f'[使用说明](https://github.com/{REPO}/blob/{TAG}/docs/USER_GUIDE.md)')
if not release:
    release=api('/releases','POST',{'tag_name':TAG,'name':VERSION+' · 三国志14 功能管理器',
        'body':body,'draft':True,'prerelease':True})
for name,data in FILES.items():
    assets=api(f"/releases/{release['id']}/assets")
    matching=[asset for asset in assets if asset['name']==name]
    if matching:
        asset=matching[0]
    else:
        if not release['draft']:
            raise SystemExit('Refusing to change an already published release')
        url=release['upload_url'].split('{',1)[0]+'?name='+urllib.parse.quote(name)
        asset=api(url,'POST',data,'application/octet-stream')
    if asset['state']!='uploaded' or asset['size']!=len(data) or asset.get('digest')!='sha256:'+hashlib.sha256(data).hexdigest():
        raise SystemExit('Release asset verification failed: '+name)
if release['draft']:
    release=api(f"/releases/{release['id']}",'PATCH',{'draft':False,'make_latest':'false'})
print('Verified release published: '+release['html_url'])
