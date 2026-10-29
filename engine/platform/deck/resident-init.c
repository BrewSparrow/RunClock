// SPDX-License-Identifier: GPL-3.0-or-later
#include <ps5/payload.h>
#include <stddef.h>
#include "deck-bootstrap.h"
#include "resident-trace.h"
#ifdef PROSPEROMENU_BOOTSTRAP_SCAN
#include <ps5/kernel.h>
// SDK v0.43 exports these CRT globals but omits them from its public header.
extern uintptr_t KERNEL_ADDRESS_DMAP_BASE,KERNEL_ADDRESS_IOMMU_SOFTC;
static void kernel_snapshot(volatile PmDeckBoot* t){
 t->kernelDataBase=KERNEL_ADDRESS_DATA_BASE;t->kernelAllproc=KERNEL_ADDRESS_ALLPROC;
 t->kernelDmap=KERNEL_ADDRESS_DMAP_BASE;t->kernelPrison=KERNEL_ADDRESS_PRISON0;
 t->kernelIommu=KERNEL_ADDRESS_IOMMU_SOFTC;
}
#endif
// This code runs before libc imports resolve: no logging, allocation, errno,
// or imported calls. Only annotate the injector-owned shared record.
static volatile PmDeckBoot* trace;
static void phase(unsigned value){if(trace)trace->phase=value;}
void pm_deck_crt_phase(unsigned value){phase(value);}
void pm_deck_trace_checkpoint(volatile PmDeckBoot* record){
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
 if(record&&record->traceArmed==PM_DECK_TRACE_ARMED&&record->traceRelease==PM_DECK_TRACE_RELEASE){
  __asm__ volatile("lea 1f(%%rip),%%rax; mov %%rax,%0; int3; 1:" : "=m"(record->traceCheckpoint) : : "rax","memory");
 }
#else
 (void)record;
#endif
}
extern int __real___crt_syscall_init(payload_args_t*);
extern int __real___kernel_init(payload_args_t*);
extern int __real___rtld_init(void);
extern int __real___klog_init(void);
extern long __crt_syscall(long,...);
// Returning an early error to SDK payload_terminate can reach its trap when
// kernel-backed symbol resolution is incomplete. This is a pthread inside
// ShellUI: publish a clean rejection and exit only that new native thread.
static _Noreturn void reject_thread(unsigned error,int result){
 if(trace){trace->kernelInitResult=result;trace->error=error;trace->count=0;trace->state=3;}
 pm_deck_trace_checkpoint(trace);
 __crt_syscall(431,(long)0); // FreeBSD SYS_thr_exit(NULL)
 struct {long sec,nsec;} delay={1,0};
 for(;;)__crt_syscall(240,&delay,0); // SYS_nanosleep if thread exit is refused
}
int __wrap___crt_syscall_init(payload_args_t* args){
 trace=(volatile PmDeckBoot*)((unsigned char*)args+PM_DECK_BOOT_OFFSET);
 if(trace->magic!=PM_DECK_BOOT_MAGIC||trace->version!=PM_DECK_BOOT_VERSION)trace=0;
 if(trace){trace->kernelReadAddress=(uintptr_t)&trace;trace->kernelReadLength=(uintptr_t)trace;}
 phase(201);int result=__real___crt_syscall_init(args);phase(202);return result;
}
int __wrap___kernel_init(payload_args_t* args){
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
 // Private diagnostic build only. Wait for the parent stager to finish its
 // breakpoint/register restoration before the resident enters kernel init.
 extern long __crt_syscall(long,...);
 struct {long sec,nsec;} delay={0,1000000};
 unsigned wait=0;
 while(trace&&trace->traceRelease!=PM_DECK_TRACE_RELEASE&&wait++<5000)__crt_syscall(240,&delay,0);
 if(!trace||trace->traceRelease!=PM_DECK_TRACE_RELEASE)reject_thread(138,-1);
#endif
#ifdef PROSPEROMENU_BOOTSTRAP_SCAN
 // The first firmware syscall must also be covered by the observer.
 if(trace){trace->bootstrapFirmware=kernel_get_fw_version();trace->kernelInitResult=INT32_MIN;}
#endif
 phase(203);int result=__real___kernel_init(args);phase(204);
 if(trace)trace->kernelInitResult=result;
 if(result)reject_thread(136,result);
#ifdef PROSPEROMENU_BOOTSTRAP_SCAN
 if(trace){trace->kernelInitResult=result;kernel_snapshot(trace);}
#endif
 return result;
}
int __wrap___klog_init(void){
 phase(208);int result=__real___klog_init();phase(209);
 if(result)reject_thread(139,result);return result;
}
// A resident thread shares ShellUI credentials and syscall policy. Preserve
// them, matching the supported etaHEN resident initialization path.
int __wrap___patch_init(void){phase(205);return 0;}
int __wrap___rtld_init(void){
 phase(206);int result=__real___rtld_init();phase(207);
 if(result)reject_thread(137,result);return result;
}

// The SDK uses this primitive to discover its kernel mappings. Record progress
// in shared memory only, without adding syscalls or touching process policy.
extern int __real_kernel_copyout(uintptr_t,void*,size_t);
int __wrap_kernel_copyout(uintptr_t address,void* output,size_t length){
 int tracked=trace&&trace->phase==203;
 if(tracked){trace->kernelReadAddress=address;trace->kernelReadLength=length;
  trace->kernelReadCount++;trace->reservedTrace=1;}
#ifdef PROSPEROMENU_BOOTSTRAP_SCAN
 volatile PmDeckKernelRead* read=0;
 if(tracked){
  kernel_snapshot(trace);read=&trace->kernelReads[(trace->kernelReadCount-1)%PM_DECK_READ_HISTORY];
  read->sequence=0;read->complete=0;read->address=address;read->length=length;read->result=INT32_MIN;
  read->sequence=trace->kernelReadCount;
 }
#endif
 int result=__real_kernel_copyout(address,output,length);
#ifdef PROSPEROMENU_BOOTSTRAP_SCAN
 if(read){read->result=result;read->complete=1;}
#endif
 if(tracked){trace->kernelReadResult=result;trace->reservedTrace=2;}
 return result;
}
