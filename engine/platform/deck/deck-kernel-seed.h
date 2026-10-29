// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#define PM_DECK_KERNEL_SEED_MAGIC UINT64_C(0x52434b5345454431)
#define PM_DECK_KERNEL_SEED_VERSION 1
#define PM_DECK_IOMMU_ID UINT64_C(0xfdd80000)
typedef struct {
 uint64_t magic,dataBase,iommu;
 uint32_t version,firmware,pid,reserved;
} PmDeckKernelSeed;
static inline int pm_deck_seed_pointer(uint64_t p){
 unsigned high=(unsigned)((p>>32)&0xffff);
 return (p>>48)==0xffff&&high&&high!=0xffff&&!(p&7);
}
static inline int pm_deck_seed_valid(const PmDeckKernelSeed* s,uint64_t base,uint32_t firmware,uint32_t pid){
 return s&&s->magic==PM_DECK_KERNEL_SEED_MAGIC&&s->version==PM_DECK_KERNEL_SEED_VERSION&&
  s->dataBase==base&&(base>>48)==0xffff&&s->firmware==firmware&&firmware&&
  s->pid==pid&&pid>1&&!s->reserved&&pm_deck_seed_pointer(s->iommu);
}
