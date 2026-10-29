// SPDX-License-Identifier: GPL-3.0-or-later
// Host fixture for the SDK v0.43 payload argument ABI.
#pragma once
typedef struct payload_args {
 int (*sys_dynlib_dlsym)(int,const char*,void*);
 int *rwpipe,*rwpair;
 long kpipe_addr,kdata_base_addr;
 int *payloadout;
} payload_args_t;
