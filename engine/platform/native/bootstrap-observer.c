// SPDX-License-Identifier: GPL-3.0-or-later
#include "bootstrap-observer.h"
#include <signal.h>

int pm_bootstrap_observe(const PmBootstrapObserverOps* o,PmBootstrapObservation* r){
 *r=(PmBootstrapObservation){0};
 if(o->release(o->context)||o->resume(o->context))goto finish;
 int result=o->wait(o->context,8000,&r->signal);
 if(result==1){
  r->timedOut=1;
  if(o->interrupt(o->context))goto finish;
  result=o->wait(o->context,1000,&r->signal);
 }
 if(result)goto finish;
 r->stopped=1;
 r->checkpoint=o->capture(o->context,&r->signal)==1;
finish:
 // Never swallow a real fault that raced our timeout SIGSTOP. Only the
 // address-checked checkpoint or our own timeout stop may be suppressed.
 r->deliver=r->checkpoint||(r->timedOut&&r->signal==SIGSTOP)?0:r->signal;
 // A checkpoint still armed after detach would be an untraced int3 later.
 // Disarm before every detach, including faults, timeouts and setup errors.
 if(o->disarm(o->context))return -1;
 r->disarmed=1;
 if(o->detach(o->context,r->deliver))return -1;
 r->detached=1;
 return r->checkpoint?0:-1;
}
