// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <ps5/kernel.h>
#include "resident-memory.h"
// The SDK mmap/mprotect overrides contain inline syscall instructions. A
// resident ShellUI module must use the initialized libkernel gateway instead.
// These explicit helpers are called only after CRT initialization; they must
// not globally replace the SDK memory functions during its own bootstrap.
extern intptr_t __crt_syscall(intptr_t,...);
int pm_deck_munmap(void* address,size_t length){
 return (int)__crt_syscall(SYS_munmap,(intptr_t)address,(intptr_t)length);
}
int pm_deck_mprotect_traced(void* address,size_t length,int protection,volatile PmDeckMemoryTrace* trace){
 if(trace){trace->stage=PM_MEMORY_NONE;trace->nativeResult=trace->kernelResult=trace->error=0;trace->observedProtection=-1;}
 if(!(protection&PROT_EXEC))
  return (int)__crt_syscall(SYS_mprotect,(intptr_t)address,(intptr_t)length,(intptr_t)protection);
 if(protection&PROT_WRITE){errno=EINVAL;if(trace)trace->error=EINVAL;return -1;}
 // Anonymous RW allocations can coalesce with ShellUI's writable mappings.
 // The SDK kernel helper edits whole VM entries; it does not split them at
 // address/length. First let native mprotect separate this buffer from every
 // adjacent writable mapping. Never call the kernel helper if that fails.
 if(trace)trace->stage=PM_MEMORY_ISOLATE;
 errno=0;int nativeResult=(int)__crt_syscall(SYS_mprotect,(intptr_t)address,(intptr_t)length,(intptr_t)(protection&~PROT_EXEC));
 if(trace){trace->nativeResult=nativeResult;trace->error=errno;}
 if(nativeResult)return -1;
 if(trace)trace->stage=PM_MEMORY_SEAL;
 pid_t pid=(pid_t)__crt_syscall(SYS_getpid);
 if(pid<=0){errno=ESRCH;if(trace)trace->error=ESRCH;return -1;}
 errno=0;int kernelResult=kernel_mprotect(pid,(intptr_t)address,length,protection);
 if(trace){trace->kernelResult=kernelResult;trace->error=errno;}
 if(kernelResult)return -1;
 if(trace)trace->stage=PM_MEMORY_VERIFY;
 errno=0;int observed=kernel_get_vmem_protection(pid,(intptr_t)address,length);
 if(trace){trace->observedProtection=observed;trace->error=errno;}
 if(observed!=protection){if(!errno)errno=EACCES;if(trace)trace->error=errno;return -1;}
 if(trace)trace->stage=PM_MEMORY_READY;
 return 0;
}
int pm_deck_mprotect(void* address,size_t length,int protection){return pm_deck_mprotect_traced(address,length,protection,NULL);}
void* pm_deck_mmap(void* address,size_t length,int protection,int flags,int fd,off_t offset){
 intptr_t mapped=__crt_syscall(SYS_mmap,(intptr_t)address,(intptr_t)length,
  (intptr_t)(protection&~PROT_EXEC),(intptr_t)flags,(intptr_t)fd,(intptr_t)offset);
 if(mapped==(intptr_t)MAP_FAILED)return MAP_FAILED;
 if((protection&PROT_EXEC)&&pm_deck_mprotect((void*)mapped,length,protection)){
  pm_deck_munmap((void*)mapped,length);errno=EPERM;return MAP_FAILED;
 }
 return (void*)mapped;
}
