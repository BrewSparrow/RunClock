// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the real resident-memory implementation. The SDK operation is
// modeled as an edit of a whole coalesced VM entry, as in SDK v0.43 kernel.c.
// This is not a PS5 VM or native injection emulator.
#include "resident-memory.h"
#include <sys/mman.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <stdarg.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

static const intptr_t buffer=0x104000;
static const size_t bytes=0x4000;
static int left,right,center,split,nativeCalls,kernelCalls,verified,unmapped;
static int nativeFail,kernelFail,verifyFail,pidFail,mapFail;
static void reset(void){
 left=right=center=PROT_READ|PROT_WRITE;
 split=nativeCalls=kernelCalls=verified=unmapped=0;
 nativeFail=kernelFail=verifyFail=pidFail=mapFail=0;errno=0;
}
intptr_t __crt_syscall(intptr_t op,...){
 va_list args;va_start(args,op);intptr_t result=0;
 if(op==SYS_getpid)result=pidFail?-1:59;
 else if(op==SYS_mprotect){
  intptr_t at=va_arg(args,intptr_t),length=va_arg(args,intptr_t),prot=va_arg(args,intptr_t);
  assert(at==buffer&&(size_t)length==bytes);nativeCalls++;
  if(nativeFail){errno=ENOMEM;result=-1;}
  else{center=(int)prot;split=center!=(PROT_READ|PROT_WRITE);}
 }else if(op==SYS_mmap){
  intptr_t at=va_arg(args,intptr_t),length=va_arg(args,intptr_t),prot=va_arg(args,intptr_t);
  (void)va_arg(args,intptr_t);(void)va_arg(args,intptr_t);(void)va_arg(args,intptr_t);
  assert(!at&&(size_t)length==bytes&&!(prot&PROT_EXEC));
  center=(int)prot;split=center!=(PROT_READ|PROT_WRITE);result=mapFail?(intptr_t)MAP_FAILED:buffer;
 }else if(op==SYS_munmap){
  intptr_t at=va_arg(args,intptr_t),length=va_arg(args,intptr_t);
  assert(at==buffer&&(size_t)length==bytes);unmapped++;
 }else assert(!"unexpected resident syscall");
 va_end(args);return result;
}
int kernel_mprotect(pid_t pid,intptr_t at,size_t length,int prot){
 assert(pid==59&&at==buffer&&length==bytes);kernelCalls++;
 if(kernelFail)return -1;
 // SDK changes the full VM entry, not merely the requested byte range.
 // Without the native split, both neighboring ShellUI allocations lose RW.
 center=prot;if(!split)left=right=prot;return 0;
}
int kernel_get_vmem_protection(pid_t pid,intptr_t at,size_t length){
 assert(pid==59&&at==buffer&&length==bytes);verified++;
 return verifyFail?PROT_READ:center;
}
static void neighbors_writable(void){assert(left==(PROT_READ|PROT_WRITE)&&right==(PROT_READ|PROT_WRITE));}
int main(void){
 reset();assert(!pm_deck_mprotect((void*)buffer,bytes,PROT_READ|PROT_EXEC));
 neighbors_writable();assert(center==(PROT_READ|PROT_EXEC));assert(nativeCalls==1&&kernelCalls==1&&verified==1);

 reset();nativeFail=1;assert(pm_deck_mprotect((void*)buffer,bytes,PROT_READ|PROT_EXEC));
 neighbors_writable();assert(center==(PROT_READ|PROT_WRITE)&&!kernelCalls&&!verified);
 reset();kernelFail=1;assert(pm_deck_mprotect((void*)buffer,bytes,PROT_READ|PROT_EXEC));
 neighbors_writable();assert(center==PROT_READ&&nativeCalls==1&&kernelCalls==1&&!verified);
 reset();verifyFail=1;assert(pm_deck_mprotect((void*)buffer,bytes,PROT_READ|PROT_EXEC));neighbors_writable();
 reset();pidFail=1;assert(pm_deck_mprotect((void*)buffer,bytes,PROT_READ|PROT_EXEC));neighbors_writable();assert(!kernelCalls);

 reset();assert(!pm_deck_mprotect((void*)buffer,bytes,PROT_READ));neighbors_writable();assert(center==PROT_READ&&!kernelCalls);
 reset();assert(pm_deck_mprotect((void*)buffer,bytes,PROT_READ|PROT_WRITE|PROT_EXEC));assert(errno==EINVAL&&!nativeCalls&&!kernelCalls);neighbors_writable();

 reset();assert(pm_deck_mmap(NULL,bytes,PROT_READ|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)==(void*)buffer);
 neighbors_writable();assert(center==(PROT_READ|PROT_EXEC)&&verified==1&&!unmapped);
 reset();nativeFail=1;assert(pm_deck_mmap(NULL,bytes,PROT_READ|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)==MAP_FAILED);
 neighbors_writable();assert(unmapped==1&&!kernelCalls);
 reset();mapFail=1;assert(pm_deck_mmap(NULL,bytes,PROT_READ|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)==MAP_FAILED);assert(!nativeCalls&&!kernelCalls&&!unmapped);
 PmDeckMemoryTrace trace;
 reset();assert(!pm_deck_mprotect_traced((void*)buffer,bytes,PROT_READ|PROT_EXEC,&trace));
 assert(trace.stage==PM_MEMORY_READY&&!trace.nativeResult&&!trace.kernelResult&&trace.observedProtection==(PROT_READ|PROT_EXEC)&&!trace.error);
 reset();nativeFail=1;assert(pm_deck_mprotect_traced((void*)buffer,bytes,PROT_READ|PROT_EXEC,&trace));
 assert(trace.stage==PM_MEMORY_ISOLATE&&trace.nativeResult==-1&&trace.error==ENOMEM&&!kernelCalls);
 reset();kernelFail=1;assert(pm_deck_mprotect_traced((void*)buffer,bytes,PROT_READ|PROT_EXEC,&trace));
 assert(trace.stage==PM_MEMORY_SEAL&&trace.kernelResult==-1&&trace.observedProtection==-1&&!verified);
 reset();verifyFail=1;assert(pm_deck_mprotect_traced((void*)buffer,bytes,PROT_READ|PROT_EXEC,&trace));
 assert(trace.stage==PM_MEMORY_VERIFY&&trace.observedProtection==PROT_READ&&trace.error==EACCES);
 puts("PASS: resident code sealing preserves adjacent writable mappings; isolation/protection failures stop before publication.");
 return 0;
}
