// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Platform adapter owns registers/memory; this controller owns signal and
// detach ordering. Capture must not perform filesystem I/O while stopped.
typedef struct {
 void* context;
 int (*release)(void*);
 int (*resume)(void*);
 // 0: stopped, 1: timeout, -1: process gone/other error.
 int (*wait)(void*,unsigned,int*);
 int (*interrupt)(void*);
 int (*capture)(void*,int*); // returns 1 only for an exact owned checkpoint
 int (*disarm)(void*);
 int (*detach)(void*,int);
} PmBootstrapObserverOps;
typedef struct {
 int stopped,signal,checkpoint,timedOut,disarmed,detached,deliver;
} PmBootstrapObservation;
int pm_bootstrap_observe(const PmBootstrapObserverOps*,PmBootstrapObservation*);
