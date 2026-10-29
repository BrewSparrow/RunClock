// SPDX-License-Identifier: GPL-3.0-or-later
// Runs during CRT initialization, before imported libc functions resolve.
#include <ps5/payload.h>
#include <ps5/kernel.h>
#include "deck-bootstrap.h"
extern long __crt_syscall(long,...);
int pm_deck_seed_iommu(payload_args_t* args,unsigned long* out){
 if(!args||!out)return -1;
 volatile PmDeckBoot* boot=(volatile PmDeckBoot*)((unsigned char*)args+PM_DECK_BOOT_OFFSET);
 if(boot->magic!=PM_DECK_BOOT_MAGIC||boot->version!=PM_DECK_BOOT_VERSION||boot->state)return -1;
 // Copy only this owned record; no imported memcpy or persisted boot cache.
 PmDeckKernelSeed seed;
 seed.magic=boot->kernelSeed.magic;seed.dataBase=boot->kernelSeed.dataBase;
 seed.iommu=boot->kernelSeed.iommu;seed.version=boot->kernelSeed.version;
 seed.firmware=boot->kernelSeed.firmware;seed.pid=boot->kernelSeed.pid;
 seed.reserved=boot->kernelSeed.reserved;
 uint32_t pid=(uint32_t)__crt_syscall(20); // FreeBSD SYS_getpid
 if(!pm_deck_seed_valid(&seed,(uint64_t)args->kdata_base_addr,kernel_get_fw_version(),pid))return -1;
 // Verify the single coordinator-resolved object, rather than walking kernel
 // data and dereferencing every pointer-shaped value in the ShellUI thread.
 uint64_t device=0;
 if(kernel_copyout((intptr_t)(seed.iommu+0x48),&device,sizeof device)||device!=PM_DECK_IOMMU_ID)return -1;
 *out=(unsigned long)seed.iommu;return 0;
}
