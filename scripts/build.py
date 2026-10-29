#!/usr/bin/env python3
"""Compile RunClock's private worker and self-contained console ELF."""
from pathlib import Path
import os,shutil,subprocess,json,hashlib,base64
root=Path(__file__).resolve().parents[1]
def run(command,cwd):subprocess.run(command,cwd=cwd,check=True)
for name in ['ZIG','PS5_PAYLOAD_SDK']:
 if not os.environ.get(name):raise SystemExit('Set '+name+'; see BUILD.md.')
run(['python3','scripts/build.py','ps5'],root/'app')
engine=root/'engine';build=engine/'build'
shutil.rmtree(build/'overlays',ignore_errors=True)
payload=build/'overlays/runclock';payload.mkdir(parents=True)
for name in ['overlay.json','runclock.ovl']:shutil.copy2(root/'app/build/ps5/runclock'/name,payload/name)
for name in ['libcurl.a','libwolfssl.a','libpsl.a','libzstd.a','libz.a']:
 (build/'download').mkdir(parents=True,exist_ok=True);shutil.copy2(root/'build-inputs/download'/name,build/'download'/name)
os.environ['PROSPEROMENU_SQLITE_LIB']=str(root/'build-inputs/catalog/libsqlite3.a')
os.environ['PROSPEROMENU_SQLITE_HEADER']=str(root/'build-inputs/catalog/sqlite3.h')
(root/'dist').mkdir(exist_ok=True)
run(['npm','ci','--ignore-scripts','--no-audit','--no-fund'],engine)
run(['node','scripts/prepare-components.mjs'],engine)
run(['node','scripts/build.mjs'],engine)
elf=(root/'dist/RunClock.elf').read_bytes();worker=(root/'app/build/ps5/runclock/runclock.ovl').read_bytes()
assert base64.b64encode(worker) in elf and b'/data/ProsperoMenu' not in worker
manifest=json.loads((root/'dist/build-manifest.json').read_text())
manifest.update({'standalone':True,'requires_prosperomenu':False,'target_firmware':'13.60','kernel_requirement':'Both compatible kstuff and elfldr loaded and ready before RunClock; neither is bundled or installed.','worker_sha256':hashlib.sha256(worker).hexdigest(),'worker_bytes':len(worker),'bridge_ports':[8890,8891],'bridge_binding':'127.0.0.1','clock_redraws_preserve_action_generation':True,'physical_console_tested':False})
manifest['triangle_delete_confirmation']=True
manifest['functional_baseline']={'build_revision':'startup-fault-diagnostic-test-6','elf_sha256':'702d02eeb7729d4838e4afea64dfa95bced95ec396d8bd8118f64eb4e6097340','maintainer_report':'Five consecutive successful launches alternating Restart and complete Shutdown/reboot on PS5 13.60.','startup_attempts':5,'startup_failures':0,'observer_enabled':True,'versioned_elf_retest_pending':True}
manifest['build_revision']='v1.0.2-beta.3-release'
manifest['resident_sdk']={'source':'bundled SDK v0.43','fresh_coordinator_iommu_handoff':True,'resident_iommu_scan':False,'early_failure':'thread-only rejection','bootstrap_abi':16,'syscall_stack_abi_verified':True,'preparation':json.loads((engine/'build/resident-crt/manifest.json').read_text())}
manifest['startup_diagnostic']={'enabled':manifest['buildIdentity']['bootstrapDiagnostic'],'scope':'first signal through preparation, before publication','checkpoint_phase':23,'real_signals_preserved':True,'disarm_before_detach':True,'stack_capture_max_bytes':8192}
manifest['resident_hooks']={'count':2,'features':['render','input'],'automatic_reinjection':False,'preparation_failure':'retain module and require restart'}
manifest['startup_reservation_recovery']='Native boot/process expiry or verified clean intent; same-boot uncertain hooks remain protected.'
(root/'dist/build-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Standalone output:',root/'dist/RunClock.elf')
