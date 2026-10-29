// SPDX-License-Identifier: GPL-3.0-or-later
// Real seed validation and early-CRT wrapper, with SDK/syscall boundaries
// substituted. No PS5 kernel access or process injection is emulated.
#include <ps5/payload.h>
#include "deck-bootstrap.h"
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
extern int pm_deck_seed_iommu(payload_args_t*,unsigned long*);
extern int __wrap___crt_syscall_init(payload_args_t*);
extern int __wrap___kernel_init(payload_args_t*);
extern int __wrap___rtld_init(void);
extern int __wrap___klog_init(void);
static _Alignas(16) unsigned char owned[0x4000];
static payload_args_t* args=(payload_args_t*)owned;
static PmDeckBoot* boot;
static const uint64_t base=UINT64_C(0xffffffff80000000),iommu=UINT64_C(0xffff800010000000);
static uint64_t objectId;
static uint32_t firmware,pid;
static int reads,readFail,initResult,rtldResult,klogResult,threadExits;
static jmp_buf exited;
uint32_t kernel_get_fw_version(void){return firmware;}
int kernel_copyout(intptr_t address,void* out,size_t length){
 assert((uint64_t)address==iommu+0x48&&length==8);reads++;
 if(readFail)return -1;memcpy(out,&objectId,8);return 0;
}
long __crt_syscall(long op,...){
 if(op==20)return pid;
 if(op==431){threadExits++;longjmp(exited,1);}
 assert(!"unexpected early CRT syscall");return -1;
}
int __real___crt_syscall_init(payload_args_t* p){assert(p==args);return 0;}
int __real___kernel_init(payload_args_t* p){assert(p==args);return initResult;}
int __real___rtld_init(void){return rtldResult;}
int __real___klog_init(void){assert(boot->phase==208);return klogResult;}
int __real_kernel_copyout(uintptr_t at,void* out,size_t length){return kernel_copyout((intptr_t)at,out,length);}
static void reset(void){
 memset(owned,0,sizeof owned);boot=(PmDeckBoot*)(owned+PM_DECK_BOOT_OFFSET);
 args->kdata_base_addr=(long)base;firmware=0x13600007;pid=59;objectId=PM_DECK_IOMMU_ID;
 boot->magic=PM_DECK_BOOT_MAGIC;boot->version=PM_DECK_BOOT_VERSION;
 boot->kernelSeed=(PmDeckKernelSeed){.magic=PM_DECK_KERNEL_SEED_MAGIC,.version=PM_DECK_KERNEL_SEED_VERSION,
  .dataBase=base,.iommu=iommu,.firmware=firmware,.pid=pid};
 reads=readFail=initResult=rtldResult=klogResult=threadExits=0;
}
static void invalid_without_read(void){unsigned long out=123;assert(pm_deck_seed_iommu(args,&out));assert(out==123&&!reads);}
int main(void){
 reset();unsigned long out=0;assert(!pm_deck_seed_iommu(args,&out));assert(out==iommu&&reads==1);
 reset();boot->kernelSeed.magic++;invalid_without_read();
 reset();boot->kernelSeed.version++;invalid_without_read();
 reset();boot->kernelSeed.dataBase+=0x4000;invalid_without_read();
 reset();boot->kernelSeed.firmware=0x13400000;invalid_without_read();
 reset();boot->kernelSeed.pid++;invalid_without_read();
 reset();boot->kernelSeed.reserved=1;invalid_without_read();
 reset();boot->kernelSeed.iommu=0xffffffff80100000;invalid_without_read();
 reset();boot->kernelSeed.iommu++;invalid_without_read();
 reset();boot->version--;invalid_without_read();
 reset();boot->state=1;invalid_without_read();
 reset();readFail=1;out=123;assert(pm_deck_seed_iommu(args,&out));assert(reads==1&&out==123);
 reset();objectId=0;out=123;assert(pm_deck_seed_iommu(args,&out));assert(reads==1&&out==123);
 reset();assert(!__wrap___crt_syscall_init(args));assert(!__wrap___kernel_init(args));assert(boot->phase==204&&!boot->state&&!threadExits);
 reset();assert(!__wrap___crt_syscall_init(args));initResult=-78;
 if(!setjmp(exited)){__wrap___kernel_init(args);assert(!"failed SDK init returned to its process terminator");}
 assert(threadExits==1&&boot->state==3&&boot->count==0&&boot->error==136&&boot->kernelInitResult==-78);
 reset();assert(!__wrap___crt_syscall_init(args));rtldResult=-1;
 if(!setjmp(exited)){__wrap___rtld_init();assert(!"failed rtld init returned to its process terminator");}
 assert(threadExits==1&&boot->state==3&&boot->error==137);
 reset();assert(!__wrap___crt_syscall_init(args));assert(!__wrap___klog_init());assert(boot->phase==209);
 reset();assert(!__wrap___crt_syscall_init(args));klogResult=-1;
 if(!setjmp(exited)){__wrap___klog_init();assert(!"failed klog init returned to its process terminator");}
 assert(threadExits==1&&boot->state==3&&boot->error==139);
 puts("PASS: fresh resident SDK seed, bounded object validation, wrong-context rejection and thread-only early failure.");
 return 0;
}
