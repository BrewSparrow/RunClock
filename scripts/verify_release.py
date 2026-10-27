#!/usr/bin/env python3
"""Validate standalone release assets, encoded worker, archives and hashes."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,re,zipfile,base64
p=argparse.ArgumentParser();p.add_argument('assets',type=Path);root=p.parse_args().assets.resolve()
def sha(data):return hashlib.sha256(data).hexdigest()
for line in (root/'SHA256SUMS.txt').read_text().splitlines():
 h,n=line.split('  ',1)
 if not re.fullmatch('[a-f0-9]{64}',h) or Path(n).name!=n:raise SystemExit('Unsafe checksum entry')
 if sha((root/n).read_bytes())!=h:raise SystemExit('Checksum mismatch: '+n)
 print('SHA256 OK:',n)
m=json.loads((root/'release-manifest.json').read_text());elf=(root/'RunClock.elf').read_bytes()
assert elf[:4]==b'\x7fELF' and elf[4]==2 and int.from_bytes(elf[18:20],'little')==62
assert m['standalone'] is True and m['requires_prosperomenu'] is False
assert sha(elf)==m['elf']['sha256'] and len(elf)==m['elf']['bytes']
assert b'/data/ProsperoMenu' not in elf
for item in m['build']['embeddedAssets']:
 start=item['offset'];size=item['bytes']
 assert start>=0 and size>0 and start+size<=len(elf),'Embedded asset bounds'
 assert sha(elf[start:start+size])==item['sha256'],'Embedded native asset mismatch: '+item['symbol']
found=False
for match in re.finditer(rb'base64:\s*"([A-Za-z0-9+/=]{2048,})"',elf):
 worker=base64.b64decode(match[1])
 if worker[:4]==b'\x7fELF' and sha(worker)==m['timing_worker']['sha256']:
  assert b'/data/RunClock/runtime' in worker and b'/data/ProsperoMenu' not in worker;found=True
assert found,'Matching private timing worker not found inside ELF'
install_verified=False
for item in m['archives']:
 f=root/item['file'];data=f.read_bytes();assert len(data)==item['bytes'] and sha(data)==item['sha256']
 with zipfile.ZipFile(f) as z:
  assert z.testzip() is None and len(z.namelist())==len(set(z.namelist()))
  names=set()
  for name in z.namelist():assert not PurePosixPath(name).is_absolute() and '..' not in PurePosixPath(name).parts and '\\' not in name
  for line in z.read('SHA256SUMS.txt').decode().splitlines():
   h,n=line.split('  ',1);assert n not in names and sha(z.read(n))==h;names.add(n)
  assert names==set(z.namelist())-{'SHA256SUMS.txt'}
  if item['file'].startswith('RunClock_Standalone_Install_'):
   assert z.read('RunClock.elf')==elf,'Install ZIP contains a different ELF'
   assert 'KNOWN_ISSUES.md' in z.namelist(),'Missing known issues in install ZIP'
   install_verified=True
 print('ZIP OK:',item['file'])
assert install_verified,'Install ZIP was not verified'
print('Standalone ELF, private worker, source and archive integrity verified. Physical-console status:',m['physical_console_tested'])
