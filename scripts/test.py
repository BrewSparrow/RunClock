#!/usr/bin/env python3
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
def run(args,cwd=root):subprocess.run([str(x) for x in args],cwd=cwd,check=True)
run(['python3','scripts/build.py','test'],root/'app')
run(['python3','scripts/build.py','host'],root/'app')
run(['node','tests/integration.mjs'])
exe=root/'app/build/test_native_live'
run(['cc','-std=c11','-D_GNU_SOURCE','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Iengine/platform/deck','tests/native_live.c','engine/platform/deck/deck-core.c','-o',exe])
wire=root/'app/build/live-wire'
run([exe,wire/'before.bin',wire/'after.bin',(wire/'pause-row.txt').read_text().strip(),wire/'delete.bin',wire/'confirm.bin'])
print('Standalone timing, live display transport, storage, control and native input checks passed.')
