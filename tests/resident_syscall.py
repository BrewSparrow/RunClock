#!/usr/bin/env python3
"""Execute the real SDK syscall bridge through an assembly mock gateway.

No operating-system syscall is issued by the mock. Test the old C prologue as
an isolated negative case, then verify the naked bridge with frame pointers on.
"""
from pathlib import Path
import importlib.util, subprocess, tarfile, resource, os, shutil

root=Path(__file__).resolve().parents[1]
def load(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module
prepare=load('resident_prepare',root/'engine/scripts/prepare-resident-crt.py')
verify=load('resident_verify',root/'engine/scripts/verify-resident-syscall.py')
out=root/'app/build/resident-syscall-test';out.mkdir(parents=True,exist_ok=True)
prepared=prepare.prepare(out/'sources')
with tarfile.open(root/'app/third_party/ps5-payload-sdk-v0.43-source.tar.gz') as archive:
    (out/'old-syscall.c').write_bytes(archive.extractfile('crt/syscall.c').read())
(out/'test.c').write_text(r'''
#include "payload.h"
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
extern long __crt_syscall(long,...);
extern int __crt_syscall_init(payload_args_t*);
extern void gateway_stub(void);
uint64_t seen[7];
static int resolve(int handle,const char* name,void* result){
 assert(handle==1);
 if(!strcmp(name,"sceKernelDlsym"))*(void**)result=(void*)resolve;
 else{assert(!strcmp(name,"getpid"));*(void**)result=(void*)gateway_stub;}
 return 0;
}
int main(void){
 payload_args_t args={.sys_dynlib_dlsym=resolve};assert(!__crt_syscall_init(&args));
 for(long n=1;n<=8;n++){
  uintptr_t before,after;
  __asm__ volatile("mov %0, rbp":"=r"(before));
  long value=__crt_syscall(0x1200+n,0x101L+n,0x202L+n,0x303L+n,0x404L+n,0x505L+n,0x606L+n);
  __asm__ volatile("mov %0, rbp":"=r"(after));
  assert(before==after&&value==0x12345678L);
  assert(seen[0]==(uint64_t)(0x1200+n));
  for(long i=1;i<=6;i++)assert(seen[i]==(uint64_t)(i*0x101+n));
 }
 puts("PASS: actual SDK bridge returns safely, preserves the frame pointer, and forwards the number plus all six arguments.");
 return 0;
}
''')
(out/'gateway.S').write_text(r'''
.intel_syntax noprefix
.text
.global gateway_stub
.type gateway_stub,@function
gateway_stub:
.rept 10
 nop
.endr
 mov QWORD PTR [rip+seen],rax
 mov QWORD PTR [rip+seen+8],rdi
 mov QWORD PTR [rip+seen+16],rsi
 mov QWORD PTR [rip+seen+24],rdx
 mov QWORD PTR [rip+seen+32],r10
 mov QWORD PTR [rip+seen+40],r8
 mov QWORD PTR [rip+seen+48],r9
 mov eax,0x12345678
 ret
.size gateway_stub,.-gateway_stub
.section .note.GNU-stack,"",@progbits
''')
def compile(source,exe):
    zig=os.environ.get('ZIG','zig')
    if not shutil.which(zig):raise SystemExit('Set ZIG to Zig 0.14.1 for the resident syscall execution regression.')
    obj=exe.with_suffix('.o')
    # Compile the bridge with the resident target and options, then execute it
    # with a host mock gateway. This source contains no host/kernel imports.
    subprocess.run([zig,'cc','-O1','-ffreestanding','-fno-builtin','-fPIC','-fms-extensions',
                    '-target','x86_64-linux-none','-U__linux__','-D__FreeBSD__=11','-D__PS5__',
                    '-fno-omit-frame-pointer','-fno-plt','-fno-stack-protector','-femulated-tls','-nostdlib',
                    '-I'+str(prepared),'-c',str(source),'-o',str(obj)],check=True)
    subprocess.run(['cc','-O1','-fno-omit-frame-pointer','-fno-stack-protector','-masm=intel',
                    '-I'+str(prepared),str(obj),str(out/'test.c'),str(out/'gateway.S'),'-o',str(exe)],check=True)
def no_core():resource.setrlimit(resource.RLIMIT_CORE,(0,0))
old=out/'old-bridge';compile(out/'old-syscall.c',old)
failed=subprocess.run([str(old)],stdout=subprocess.PIPE,stderr=subprocess.PIPE,preexec_fn=no_core)
assert failed.returncode!=0,'Old bridge unexpectedly survived a forced C stack frame'
try:verify.verify(old.read_bytes())
except ValueError:pass
else:raise AssertionError('ELF guard accepted the old stack prologue')
fixed=out/'fixed-bridge';compile(prepared/'syscall.c',fixed)
subprocess.run([str(fixed)],check=True)
verify.verify(fixed.read_bytes())
print('PASS: old prologue fails execution and the ELF guard; naked bridge passes both with frame pointers forced on.')
