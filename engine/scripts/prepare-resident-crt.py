#!/usr/bin/env python3
"""Prepare exact SDK v0.43 CRT sources with a resident-only IOMMU handoff.

Original SDK copyright/GPL notices remain in every extracted file. The original
source archive accompanies RunClock under app/third_party. Only the resident
renderer uses this CRT; the coordinator and other helpers use the stock SDK.
"""
from pathlib import Path
import hashlib, json, tarfile

SDK_ARCHIVE_SHA256 = 'f6b427ce23c56437cb9201685e72d83e88c67e13ae178e61b12fcf8274a44f7d'
SOURCES = ['crt.c', 'syscall.c', 'klog.c', 'nid.c', 'kernel.c', 'rtld.c',
           'rtld_so.c', 'rtld_sprx.c', 'rtld_payload.c', 'rtld_dlfcn.c',
           'mdbg.c', 'patch.c', 'kernel_procio.c', 'kernel_iommu.c', '_start.S']

def remove_function(text, signature):
    assert text.count(signature) == 1
    start = text.index(signature)
    begin = text.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        if text[end] == '{': depth += 1
        elif text[end] == '}': depth -= 1
        end += 1
    return text[:start] + text[end:]

def patch_kernel(text):
    old = '''  if(!(KERNEL_ADDRESS_IOMMU_SOFTC=kernel_find_iommu_softc())) {
    return -ENOSYS;
  }'''
    new = '''  // RunClock resident: use the fresh, independently verified coordinator
  // address. Do not scan pointer-shaped kernel data inside ShellUI.
  extern int pm_deck_seed_iommu(payload_args_t*, unsigned long*);
  if(pm_deck_seed_iommu(args, &KERNEL_ADDRESS_IOMMU_SOFTC)) {
    return -ENOSYS;
  }'''
    assert text.count(old) == 1
    text = text.replace(old, new)
    text = remove_function(text, 'static unsigned long\nkernel_find_iommu_softc(void)')
    text = remove_function(text, 'static int\nkernel_is_heap_addr(unsigned long kaddr)')
    assert 'kernel_find_iommu_softc' not in text
    return text

def patch_syscall(text):
    # This body manually returns and reads argument six at [rsp+8]. A C
    # prologue would move both the return address and that argument. The SDK
    # binary omits the frame by convention; make that requirement explicit
    # when rebuilding its source with a different compiler configuration.
    original = 'void\n__crt_syscall() {'
    assert text.count(original) == 1
    return text.replace(original, '__attribute__((naked))\n'+original)

def patch_crt(text):
    # This existing SDK write is between kernel/log setup and __patch_init.
    # Shared-record markers do not resolve imports or change its behavior.
    original = '  if(!KERNEL_DLSYM(0x2, __isthreaded)) {'
    assert text.count(original) == 1
    text = text.replace(original, '  extern void pm_deck_crt_phase(unsigned);\n'
                        '  pm_deck_crt_phase(210);\n'+original)
    original = '  *__isthreaded = 1;'
    assert text.count(original) == 1
    return text.replace(original, '  pm_deck_crt_phase(211);\n'+original+
                        '\n  pm_deck_crt_phase(212);')

def prepare(destination=None):
    engine = Path(__file__).resolve().parents[1]
    source = engine.parent/'app/third_party/ps5-payload-sdk-v0.43-source.tar.gz'
    assert hashlib.sha256(source.read_bytes()).hexdigest() == SDK_ARCHIVE_SHA256, 'SDK source pin mismatch'
    destination = Path(destination) if destination else engine/'build/resident-crt'
    destination.mkdir(parents=True, exist_ok=True)
    with tarfile.open(source) as archive:
        for member in archive.getmembers():
            path = Path(member.name)
            if member.isfile() and path.parent.as_posix() == 'crt' and path.suffix in {'.c', '.h', '.S'}:
                data = archive.extractfile(member).read()
                if path.name == 'kernel.c': data = patch_kernel(data.decode()).encode()
                if path.name == 'syscall.c': data = patch_syscall(data.decode()).encode()
                if path.name == 'crt.c': data = patch_crt(data.decode()).encode()
                (destination/path.name).write_bytes(data)
    manifest = {'sdk_archive_sha256': SDK_ARCHIVE_SHA256,
                'resident_only': True, 'iommu_scan': False,
                'syscall_bridge_naked': True,
                'libc_thread_flag_phase_markers': True,
                'sources': SOURCES,
                'kernel_source_sha256': hashlib.sha256((destination/'kernel.c').read_bytes()).hexdigest(),
                'syscall_source_sha256': hashlib.sha256((destination/'syscall.c').read_bytes()).hexdigest()}
    manifest['crt_source_sha256'] = hashlib.sha256((destination/'crt.c').read_bytes()).hexdigest()
    (destination/'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n')
    return destination

if __name__ == '__main__':
    print(prepare())
