#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
def run(args,cwd=root):subprocess.run([str(x) for x in args],cwd=cwd,check=True)
run(['node','tests/startup-recovery.mjs'])
run(['python3','scripts/build.py','test'],root/'app')
startup=root/'app/build/test_native_startup'
run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Iengine/platform/native','tests/native_startup.c','-o',startup])
run([startup])
run(['python3','scripts/build.py','host'],root/'app')
run(['node','tests/integration.mjs'])
exe=root/'app/build/test_native_live'
run(['cc','-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Iengine/platform/deck','tests/native_live.c','engine/platform/deck/deck-core.c','-o',exe])
wire=root/'app/build/live-wire'
run([exe,wire/'before.bin',wire/'after.bin',(wire/'pause-row.txt').read_text().strip(),wire/'delete.bin',wire/'confirm.bin'])
stability=root/'app/build/test_native_stability'
run(['cc','-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Iengine/platform/deck','-Iengine/platform/runtime','tests/native_stability.c','engine/platform/deck/deck-core.c','engine/platform/deck/deck-route.c','-o',stability])
run([stability,wire/'before.bin'])
memory=root/'app/build/test_resident_memory'
run(['cc','-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-Itests/stubs','-Iengine/platform/deck','tests/resident_memory.c','engine/platform/deck/resident-memory.c','-o',memory])
run([memory])
kernel=root/'app/build/test_resident_kernel'
run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Itests/stubs','-Iengine/platform/deck','tests/resident_kernel.c','engine/platform/deck/resident-kernel-seed.c','engine/platform/deck/resident-init.c','-o',kernel])
run([kernel])
observer=root/'app/build/test_bootstrap_observer'
run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-ffunction-sections','-fdata-sections',
     '-DPROSPEROMENU_BOOTSTRAP_TRACE','-DPROSPEROMENU_BOOTSTRAP_SCAN','-Itests/stubs','-Iengine/platform/deck','-Iengine/platform/native',
     'tests/bootstrap_observer.c','engine/platform/native/bootstrap-observer.c','engine/platform/deck/resident-init.c',
     '-Wl,--gc-sections','-o',observer])
run([observer])
run(['python3','tests/resident_crt.py'])
run(['python3','tests/resident_syscall.py'])
print('Standalone timing, live display transport, storage, control, native input and startup observer checks passed.')
