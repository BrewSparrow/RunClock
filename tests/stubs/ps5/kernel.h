// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
int kernel_mprotect(pid_t,intptr_t,size_t,int);
int kernel_get_vmem_protection(pid_t,intptr_t,size_t);
uint32_t kernel_get_fw_version(void);
int kernel_copyout(intptr_t,void*,size_t);
extern uintptr_t KERNEL_ADDRESS_DATA_BASE,KERNEL_ADDRESS_ALLPROC,KERNEL_ADDRESS_PRISON0;
