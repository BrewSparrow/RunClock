// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the production diagnostic lifecycle and the actual x86 checkpoint.
#define _GNU_SOURCE
#include "bootstrap-observer.h"
#include "resident-trace.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>

typedef struct {
 int released,resumed,waits,interrupted,captured,disarmed,detached;
 int releaseFail,resumeFail,waitFail,timeout,signal,checkpoint,disarmFail,detachFail;
 int deliver;char sequence[32];unsigned length;
} Fixture;
static void event(Fixture* f,char c){assert(f->length+1<sizeof f->sequence);f->sequence[f->length++]=c;}
static int release(void* p){Fixture* f=p;event(f,'r');f->released++;return f->releaseFail;}
static int resume(void* p){Fixture* f=p;assert(f->released);event(f,'c');f->resumed++;return f->resumeFail;}
static int wait_stop(void* p,unsigned timeout,int* signal){
 Fixture* f=p;assert(f->resumed);f->waits++;event(f,'w');
 if(f->timeout&&f->waits==1){assert(timeout==8000);return 1;}
 assert(timeout==(f->timeout?1000u:8000u));
 if(f->timeout)assert(f->interrupted);
 *signal=f->signal;return f->waitFail?-1:0;
}
static int interrupt(void* p){Fixture* f=p;assert(f->waits==1);event(f,'s');f->interrupted++;return 0;}
static int capture(void* p,int* signal){
 Fixture* f=p;assert(*signal==f->signal&&!f->disarmed&&!f->detached);event(f,'o');f->captured++;return f->checkpoint;
}
static int disarm(void* p){Fixture* f=p;event(f,'d');f->disarmed++;return f->disarmFail;}
static int detach(void* p,int signal){
 Fixture* f=p;assert(f->disarmed&&!f->disarmFail);event(f,'x');f->detached++;f->deliver=signal;return f->detachFail;
}
static int observe(Fixture* f,PmBootstrapObservation* r){
 PmBootstrapObserverOps ops={f,release,resume,wait_stop,interrupt,capture,disarm,detach};
 return pm_bootstrap_observe(&ops,r);
}
static volatile sig_atomic_t traps;
static uint64_t checkpointRip;
static void trap_handler(int signal,siginfo_t* info,void* context){
 (void)info;assert(signal==SIGTRAP);ucontext_t* u=context;
 checkpointRip=(uint64_t)u->uc_mcontext.gregs[REG_RIP];traps++;
}
int main(void){
 PmBootstrapObservation r;Fixture f={.signal=SIGTRAP,.checkpoint=1};
 assert(!observe(&f,&r));assert(!strcmp(f.sequence,"rcwodx"));
 assert(r.checkpoint&&r.detached&&r.disarmed&&!r.deliver&&!f.deliver);
 f=(Fixture){.signal=SIGSEGV};assert(observe(&f,&r));
 assert(r.stopped&&!r.checkpoint&&r.detached&&r.deliver==SIGSEGV&&f.deliver==SIGSEGV);
 f=(Fixture){.signal=SIGTRAP};assert(observe(&f,&r));assert(f.deliver==SIGTRAP);
 f=(Fixture){.signal=SIGSTOP,.timeout=1};assert(observe(&f,&r));
 assert(r.timedOut&&r.stopped&&!r.deliver);
 assert(!strcmp(f.sequence,"rcwswodx"));
 f=(Fixture){.signal=SIGBUS,.timeout=1};assert(observe(&f,&r));assert(r.timedOut&&f.deliver==SIGBUS);
 f=(Fixture){.signal=SIGSEGV,.detachFail=1};assert(observe(&f,&r));
 assert(r.disarmed&&!r.detached&&r.deliver==SIGSEGV);
 f=(Fixture){.signal=SIGSEGV,.disarmFail=1};assert(observe(&f,&r));
 assert(!r.disarmed&&!r.detached&&!f.detached&&r.deliver==SIGSEGV);
 f=(Fixture){.resumeFail=1};assert(observe(&f,&r));
 assert(!f.waits&&!f.captured&&r.disarmed&&r.detached&&!f.deliver);
 f=(Fixture){.releaseFail=1};assert(observe(&f,&r));assert(!f.resumed&&r.detached);
 f=(Fixture){.waitFail=1,.disarmFail=1};assert(observe(&f,&r));assert(!f.captured&&!f.detached);

 struct sigaction action={.sa_sigaction=trap_handler,.sa_flags=SA_SIGINFO};
 assert(!sigemptyset(&action.sa_mask));assert(!sigaction(SIGTRAP,&action,NULL));
 PmDeckBoot record={0};pm_deck_trace_checkpoint(&record);assert(!traps);
 record.traceArmed=PM_DECK_TRACE_ARMED;pm_deck_trace_checkpoint(&record);assert(!traps);
 record.traceRelease=PM_DECK_TRACE_RELEASE;pm_deck_trace_checkpoint(&record);
 assert(traps==1&&record.traceCheckpoint&&record.traceCheckpoint==checkpointRip);
 assert(*(const unsigned char*)(uintptr_t)(checkpointRip-1)==0xcc);
 record.traceArmed=0;pm_deck_trace_checkpoint(&record);assert(traps==1);
 puts("PASS: diagnostic ordering, real-signal preservation, timeout races, retained detach errors and actual x86 checkpoint/disarm.");
 return 0;
}
