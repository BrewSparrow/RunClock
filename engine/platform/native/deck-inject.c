// SPDX-License-Identifier: GPL-3.0-or-later
// Thread stager adapted from libNineS / John Törnblom and the supported etaHEN
// port. Original publication/checking is isolated from ShellUI's UI thread.
#include "deck-inject.h"
#include "deck-launch.h"
#include "deck-bootstrap.h"
extern uintptr_t KERNEL_ADDRESS_IOMMU_SOFTC;
#include "publication.h"
#include "deck-protection.h"
#include "pt.h"
#include <ps5/kernel.h>
#include <sys/mman.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
#include <signal.h>
#include "bootstrap-observer.h"
#endif
extern intptr_t pm_elf_map(pid_t,const uint8_t*,size_t);
extern intptr_t pm_payload_args(pid_t);
extern int pt_getlwps(pid_t,int*,size_t);
typedef struct { pid_t pid;intptr_t address;int(*identity)(void*);void* cookie;uint64_t pages[PM_DECK_MAX_PATCHES*2];int protections[PM_DECK_MAX_PATCHES*2];unsigned pageCount,patchCount;int attached,allowFallback;PmDeckPatch patches[PM_DECK_MAX_PATCHES]; } Publication;
static int attach(void* p){Publication* c=p;
 if(!c->identity(c->cookie)||(!c->attached&&pt_attach_owned(c->pid,&c->attached)))return -1;
 return c->identity(c->cookie)?0:-1;}
static int detach(void* p){Publication* c=p;if(!c->attached)return 0;
 if(pt_detach(c->pid,0))return -1;c->attached=0;return 0;}
static int threads(void* p,uint64_t* pcs,size_t capacity){
 Publication* c=p;int ids[2048];int n=pt_getlwps(c->pid,ids,capacity);if(n<=0)return -1;
 for(int i=0;i<n;i++){struct reg r;if(pt_getregs(ids[i],&r))return -1;pcs[i]=r.r_rip;}return n;
}
static int readbytes(void* p,uint64_t a,void* b,size_t n){return pt_copyout(((Publication*)p)->pid,a,b,n);}
static int writebytes(void* p,uint64_t a,const void* b,size_t n){
 Publication* c=p;unsigned char check[PM_DECK_PATCH_MAX];
 const PmDeckPatch* patch=NULL;
 for(unsigned i=0;i<c->patchCount;i++)if(c->patches[i].address==a)patch=&c->patches[i];
 if(!patch||n!=pm_deck_patch_length(patch))return -1;
 for(size_t offset=0;offset<n;){
  uint64_t at=a+offset,page=at&~UINT64_C(0x3fff);size_t length=0x4000-(at-page);if(length>n-offset)length=n-offset;
  unsigned slot=0;while(slot<c->pageCount&&c->pages[slot]!=page)slot++;
  if(slot==c->pageCount){if(slot==PM_DECK_MAX_PATCHES*2)return -1;int original=kernel_get_vmem_protection(c->pid,at,length);
   if(original<0||!(original&PROT_EXEC))return -1;c->pages[slot]=page;c->protections[slot]=original;c->pageCount++;}
  int protection=c->protections[slot];
  int final=pm_deck_final_protection(protection,patch,a,b,n);if(final<0)return -1;
  if(kernel_mprotect(c->pid,at,length,protection|PROT_READ|PROT_WRITE))return -1;
  // Only PT_IO writes, never the SDK's physical fallback on shared code pages.
  int written=pt_copyin(c->pid,(const unsigned char*)b+offset,at,length),restored=kernel_mprotect(c->pid,at,length,final);
  if(restored)restored=kernel_mprotect(c->pid,at,length,final);
  if(written||restored)return -1;offset+=length;
 }
 if(pt_copyout(c->pid,a,check,n)||memcmp(check,b,n))return -1;return 0;
}
static int ack(void* p){Publication* c=p;uint32_t state=2,count=c->patchCount;
 if(pt_copyin(c->pid,&count,c->address+offsetof(PmDeckBoot,count),4))return -1;
 return pt_copyin(c->pid,&state,c->address+offsetof(PmDeckBoot,state),4);}
static int reject(void* p){Publication* c=p;uint32_t state=c->allowFallback?1:3;return pt_copyin(c->pid,&state,c->address+offsetof(PmDeckBoot,state),4);}
static void pause_thread(void* p){(void)p;usleep(1000);}
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
static void log_kernel_scan(const PmDeckBoot* s){
 fprintf(stderr,"ProsperoMenu bootstrap kernel snapshot: firmware=0x%08x initResult=%d data=0x%llx allproc=0x%llx dmap=0x%llx prison=0x%llx iommu=0x%llx reads=%u\n",
  s->bootstrapFirmware,s->kernelInitResult,(unsigned long long)s->kernelDataBase,(unsigned long long)s->kernelAllproc,
  (unsigned long long)s->kernelDmap,(unsigned long long)s->kernelPrison,(unsigned long long)s->kernelIommu,s->kernelReadCount);
 uint32_t count=s->kernelReadCount<PM_DECK_READ_HISTORY?s->kernelReadCount:PM_DECK_READ_HISTORY;
 for(uint32_t i=0;i<count;i++){
  uint32_t sequence=s->kernelReadCount-count+i+1;const PmDeckKernelRead* r=&s->kernelReads[(sequence-1)%PM_DECK_READ_HISTORY];
  if(r->sequence!=sequence)continue;
  fprintf(stderr,"ProsperoMenu bootstrap read: seq=%u address=0x%llx length=%llu complete=%u result=%d\n",r->sequence,
   (unsigned long long)r->address,(unsigned long long)r->length,r->complete,r->result);
 }
}
// Diagnostic only: observe through full preparation, before hook publication.
// Keep all captured bytes local until detach; preserve every real signal.
typedef struct {
 pid_t pid;intptr_t control;int* attached;
 PmBootstrapObservation observation;
 int tid,code,haveRegs,haveBytes,haveState;
 uintptr_t fault;struct reg regs;PmDeckBoot state;uint8_t bytes[32],stack[8192];
 size_t stackBytes;int controlProtection,ripProtection,rspProtection;
} BootstrapCapture;
static int trace_release(void* context){
 BootstrapCapture* c=context;uint32_t armed=PM_DECK_TRACE_ARMED,release=PM_DECK_TRACE_RELEASE;
 return pt_copyin(c->pid,&armed,c->control+offsetof(PmDeckBoot,traceArmed),sizeof armed)||
  pt_copyin(c->pid,&release,c->control+offsetof(PmDeckBoot,traceRelease),sizeof release);
}
static int trace_resume(void* context){return pt_continue(((BootstrapCapture*)context)->pid,0);}
static int trace_signal_wait(void* context,unsigned timeout,int* signal){
 if(!pt_wait_signal(((BootstrapCapture*)context)->pid,timeout,signal))return 0;
 return errno==ETIMEDOUT?1:-1;
}
static int trace_interrupt(void* context){return kill(((BootstrapCapture*)context)->pid,SIGSTOP);}
static int trace_disarm(void* context){
 BootstrapCapture* c=context;uint32_t off=0;
 return pt_copyin(c->pid,&off,c->control+offsetof(PmDeckBoot,traceArmed),sizeof off);
}
static int trace_detach(void* context,int signal){
 BootstrapCapture* c=context;if(pt_detach(c->pid,signal))return -1;*c->attached=0;return 0;
}
static int trace_capture(void* context,int* signal){
 BootstrapCapture* c=context;int actualSignal=0;
 int info=!pt_stop_info(c->pid,&c->tid,&actualSignal,&c->code,&c->fault);
 if(actualSignal)*signal=actualSignal;
 c->haveRegs=info&&c->tid>0&&!pt_getregs(c->tid,&c->regs);
 c->haveState=!pt_copyout(c->pid,c->control,&c->state,sizeof c->state);
 if(c->haveState&&(c->state.magic!=PM_DECK_BOOT_MAGIC||c->state.version!=PM_DECK_BOOT_VERSION))c->haveState=0;
 if(c->haveRegs)c->haveBytes=!pt_copyout(c->pid,c->regs.r_rip,c->bytes,sizeof c->bytes);
 uint8_t opcode=0;
 int own=info&&c->haveRegs&&c->haveState&&*signal==SIGTRAP&&
  c->state.traceArmed==PM_DECK_TRACE_ARMED&&c->state.traceRelease==PM_DECK_TRACE_RELEASE&&
  ((c->state.phase==23&&c->state.state==0)||c->state.state==3)&&
  c->state.traceCheckpoint&&c->regs.r_rip==c->state.traceCheckpoint&&
  !pt_copyout(c->pid,c->regs.r_rip-1,&opcode,1)&&opcode==0xcc;
 if(!own&&c->haveRegs){
  size_t available=0x4000-(c->regs.r_rsp&0x3fff);if(available>sizeof c->stack)available=sizeof c->stack;
  if(!pt_copyout(c->pid,c->regs.r_rsp,c->stack,available))c->stackBytes=available;
  c->controlProtection=kernel_get_vmem_protection(c->pid,c->control,sizeof c->state);
  c->ripProtection=kernel_get_vmem_protection(c->pid,c->regs.r_rip,1);
  c->rspProtection=kernel_get_vmem_protection(c->pid,c->regs.r_rsp,1);
 }
 return own;
}
static int trace_bootstrap(BootstrapCapture* c){
 PmBootstrapObserverOps ops={c,trace_release,trace_resume,trace_signal_wait,trace_interrupt,trace_capture,trace_disarm,trace_detach};
 return pm_bootstrap_observe(&ops,&c->observation);
}
static void log_bootstrap_capture(const BootstrapCapture* c){
 const PmBootstrapObservation* r=&c->observation;
 fprintf(stderr,"RunClock bootstrap first stop: tid=%d signal=%d code=%d fault=0x%llx timeout=%d checkpoint=%d phase=%u regs=%d disarmed=%d detached=%d deliver=%d\n",
  c->tid,r->signal,c->code,(unsigned long long)c->fault,r->timedOut,r->checkpoint,c->haveState?c->state.phase:0,c->haveRegs,r->disarmed,r->detached,r->deliver);
 if(c->haveState)log_kernel_scan(&c->state);
 if(!r->checkpoint){
  fprintf(stderr,"RunClock bootstrap fault capture: controlProtection=0x%x ripProtection=0x%x rspProtection=0x%x stackAddress=0x%llx stackBytes=%zu\n",c->controlProtection,c->ripProtection,c->rspProtection,(unsigned long long)c->regs.r_rsp,c->stackBytes);
  if(c->stackBytes){FILE* file=fopen("/data/RunClock/runtime/bootstrap-stack.bin","wb");int saved=0;if(file){size_t wrote=fwrite(c->stack,1,c->stackBytes,file);int closed=fclose(file);saved=wrote==c->stackBytes&&!closed;}
   fprintf(stderr,"RunClock bootstrap stack saved: ok=%d bytes=%zu\n",saved,c->stackBytes);}
 }
 if(c->haveRegs)fprintf(stderr,"RunClock bootstrap registers: rip=0x%llx rsp=0x%llx rbp=0x%llx rax=0x%llx rbx=0x%llx rcx=0x%llx rdx=0x%llx rsi=0x%llx rdi=0x%llx r12=0x%llx r13=0x%llx r14=0x%llx r15=0x%llx\n",
  (unsigned long long)c->regs.r_rip,(unsigned long long)c->regs.r_rsp,(unsigned long long)c->regs.r_rbp,(unsigned long long)c->regs.r_rax,(unsigned long long)c->regs.r_rbx,(unsigned long long)c->regs.r_rcx,(unsigned long long)c->regs.r_rdx,(unsigned long long)c->regs.r_rsi,(unsigned long long)c->regs.r_rdi,(unsigned long long)c->regs.r_r12,(unsigned long long)c->regs.r_r13,(unsigned long long)c->regs.r_r14,(unsigned long long)c->regs.r_r15);
 if(c->haveBytes){fputs("RunClock bootstrap instruction:",stderr);for(unsigned i=0;i<sizeof c->bytes;i++)fprintf(stderr," %02x",c->bytes[i]);fputc('\n',stderr);}
 fflush(stderr);
}
#endif
int pm_deck_inject(pid_t pid,const uint8_t* image,size_t bytes,const char credential[33],intptr_t* control,int(*identity)(void*),void* cookie,int* resumed){
 // First thing: prove the injector was entered at all. The end of this function
 // overwrites this with the full outcome, so a file that still says "entered"
 // means the injection hung or crashed inside.
 {FILE* entered=fopen("/data/RunClock/runtime/deck-status.txt","w");if(entered){fputs("deck injector entered\n",entered);fclose(entered);}}
 *control=0;*resumed=1;int attached=0,ok=0;intptr_t code=0,params=0,entry=0;PmDeckLaunch launch={0};
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
 BootstrapCapture capture={.pid=pid,.attached=&attached,.controlProtection=-1,.ripProtection=-1,.rspProtection=-1};int traceStarted=0;
 fprintf(stderr,"RunClock v1.0.2-beta.3 startup observer: observing through preparation before hook publication\n");fflush(stderr);
 // Pair fresh runtime evidence with a fresh stack; keep one previous capture.
 rename("/data/RunClock/runtime/bootstrap-stack.bin","/data/RunClock/runtime/bootstrap-stack.previous.bin");
#endif
 Publication context={.pid=pid,.identity=identity,.cookie=cookie};
 const char* check="verify resident kernel seed";PmDeckBoot last={0};int haveStatus=0;
 PmDeckBoot initial={0};initial.magic=PM_DECK_BOOT_MAGIC;initial.version=PM_DECK_BOOT_VERSION;memcpy(initial.credential,credential,33);
 check="verify resident kernel seed";
 initial.kernelSeed=(PmDeckKernelSeed){.magic=PM_DECK_KERNEL_SEED_MAGIC,.version=PM_DECK_KERNEL_SEED_VERSION,
  .dataBase=KERNEL_ADDRESS_DATA_BASE,.iommu=KERNEL_ADDRESS_IOMMU_SOFTC,.firmware=kernel_get_fw_version(),.pid=(uint32_t)pid};
 uint64_t device=0;
 if(!pm_deck_seed_valid(&initial.kernelSeed,KERNEL_ADDRESS_DATA_BASE,kernel_get_fw_version(),(uint32_t)pid)||
    kernel_copyout((intptr_t)(initial.kernelSeed.iommu+0x48),&device,sizeof device)||device!=PM_DECK_IOMMU_ID)goto done;
 fprintf(stderr,"RunClock resident SDK context: fresh coordinator seed verified for pid=%d; kernel scan disabled in resident\n",pid);fflush(stderr);
 check="attach";if(pt_attach_owned(pid,&attached))goto done;
 check="attached process identity";if(!identity||!identity(cookie))goto done;
 check="map deck image";entry=pm_elf_map(pid,image,bytes);if(!entry)goto done;
 check="create payload arguments";intptr_t args=pm_payload_args(pid);if(!args)goto done;
 *control=args+PM_DECK_BOOT_OFFSET;
 check="initialize bootstrap record";if(pt_copyin(pid,&initial,*control,sizeof initial))goto done;
 check="resolve pthread_create";intptr_t create=kernel_dynlib_dlsym(pid,0x1,"pthread_create");if(!create)create=kernel_dynlib_dlsym(pid,0x2001,"pthread_create");
 if(!create)goto done;
 // SysV pthread_create(&thread,NULL,entry,args), followed by an observed int3.
 const unsigned char stager[]={0x55,0x48,0x89,0xe5,0x48,0x83,0xec,0x10,0x49,0x89,0xf8,
  0x48,0x8d,0x7d,0xf8,0x31,0xf6,0x49,0x8b,0x10,0x49,0x8b,0x48,0x08,0x41,0xff,0x50,0x10,0xcc,0xc9,0xc3};
 uint64_t pointers[3]={(uintptr_t)entry,(uintptr_t)args,(uintptr_t)create};
 check="stage bootstrap thread";code=pt_mmap(pid,0,0x4000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 params=pt_mmap(pid,0,0x4000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
 if(code<=0||params<=0||pt_copyin(pid,stager,code,sizeof stager)||pt_copyin(pid,pointers,params,sizeof pointers))goto done;
 // Adjacent anonymous RW allocations can share one VM entry. The SDK's
 // kernel_mprotect changes that entire entry, without splitting at our range.
 // Use native mprotect first to isolate this code page from args and state.
 check="isolate bootstrap code";if(pt_mprotect(pid,code,0x4000,PROT_READ))goto done;
 check="protect bootstrap code";if(kernel_mprotect(pid,code,0x4000,PROT_READ|PROT_EXEC))goto done;
 check="verify bootstrap memory permissions";
 int controlProt=kernel_get_vmem_protection(pid,*control,sizeof initial);
 int paramsProt=kernel_get_vmem_protection(pid,params,sizeof pointers);
 int codeProt=kernel_get_vmem_protection(pid,code,sizeof stager);
 if(controlProt<0||paramsProt<0||codeProt<0||
    (controlProt&(PROT_READ|PROT_WRITE))!=(PROT_READ|PROT_WRITE)||
    (paramsProt&(PROT_READ|PROT_WRITE))!=(PROT_READ|PROT_WRITE)||
    (codeProt&(PROT_READ|PROT_WRITE|PROT_EXEC))!=(PROT_READ|PROT_EXEC))goto done;
 check="create bootstrap thread";if(pm_deck_launch(pid,code,code+sizeof stager-2,(uintptr_t)params,&launch))goto done;
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
 check="trace bootstrap first signal";capture.control=*control;traceStarted=1;
 int traceResult=trace_bootstrap(&capture);launch.deliver=capture.observation.deliver;
 if(capture.haveState){last=capture.state;haveStatus=1;}
 if(traceResult)goto done;
#else
 // The new thread can already be inside CRT initialization. Detach directly,
 // matching the working libNineS resident path: do not commandeer the host
 // thread for cleanup syscalls while its new sibling is starting. These two
 // small mappings remain owned by this ShellUI generation until process exit.
 // The durable receipt prevents repeated allocations/reinjection this boot.
 check="resume ShellUI";if(pt_detach(pid,0))goto done;attached=0;
#endif
 for(unsigned tick=0;tick<180;tick++){
  check="bootstrap process identity";
  if(!identity(cookie))goto done;
  check="read bootstrap record";PmDeckBoot status;if(kernel_proc_copyout(pid,*control,&status,sizeof status))goto done;
  check="bootstrap record identity";if(status.magic!=PM_DECK_BOOT_MAGIC||status.version!=PM_DECK_BOOT_VERSION)goto done;
  if(!haveStatus||status.state!=last.state||status.phase!=last.phase||status.error!=last.error||
     (status.renderPhase&&!last.renderPhase)||(status.inputPhase&&!last.inputPhase)){
   fprintf(stderr,"ProsperoMenu deck bootstrap: state=%u phase=%u(%s) error=%u render=%u input=%u\n",status.state,status.phase,pm_deck_boot_phase(status.phase),status.error,status.renderPhase,status.inputPhase);fflush(stderr);
  }
  if(status.kernelReadCount&&(!haveStatus||status.kernelReadCount!=last.kernelReadCount||status.reservedTrace!=last.reservedTrace)){
   fprintf(stderr,"ProsperoMenu kernel init: reads=%u readPhase=%u address=0x%llx length=%llu result=%d\n",
    status.kernelReadCount,status.reservedTrace,(unsigned long long)status.kernelReadAddress,(unsigned long long)status.kernelReadLength,status.kernelReadResult);fflush(stderr);
  }
  if(status.codeMemory.stage&&(!haveStatus||status.codeMemory.stage!=last.codeMemory.stage||
     status.codeMemory.error!=last.codeMemory.error||status.state!=last.state)){
   fprintf(stderr,"RunClock code protection: stage=%u native=%d kernel=%d observed=0x%x errno=%d\n",
    status.codeMemory.stage,status.codeMemory.nativeResult,status.codeMemory.kernelResult,
    status.codeMemory.observedProtection,status.codeMemory.error);fflush(stderr);
  }
  last=status;haveStatus=1;
  if(status.state==3){fprintf(stderr,"ProsperoMenu deck bootstrap rejected: check=%s error=%u count=%u\n",pm_deck_prepare_error(status.error),status.error,status.count);goto done;}
  if(status.state==1){
   if(status.count!=2&&status.count!=5&&status.count!=9&&status.count!=PM_DECK_MAX_PATCHES)goto done;
   context.patchCount=status.count;
   memcpy(context.patches,status.patches,sizeof context.patches);
   context.address=*control;PmPublicationOps ops={&context,attach,detach,threads,readbytes,writebytes,ack,reject,pause_thread};
   context.allowFallback=status.count>2;
   check="publish hook batch";int published=pm_publish_hooks(status.patches,status.count,&ops);
   // Only a verified complete rollback permits this fallback. The bootstrap
   // stays waiting at state 1; an uncertain write/detach never retries.
   if(published==1&&context.allowFallback){
    context.allowFallback=0;context.patchCount=2;
    published=pm_publish_hooks(status.patches,2,&ops);
   }
   if(published){
    uint32_t failed=3;kernel_proc_copyin(pid,&failed,*control+offsetof(PmDeckBoot,state),4);goto done;
   }
  }
  if(status.state==4){fprintf(stderr,"ProsperoMenu dashboard integration: hooks=%u available=%d preparationError=%u\n",status.count,status.count>=5,status.dashboardError);ok=1;break;}check="bootstrap wait";usleep(100000);
 }
done:
 if(attached){
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
  // A failed detach must preserve the pending signal. A failed disarm must
  // never expose a future, now-untraced checkpoint by blindly detaching.
  int safeToDetach=!traceStarted||capture.observation.disarmed||!trace_disarm(&capture);
  if(traceStarted&&safeToDetach)capture.observation.disarmed=1;
  if(!safeToDetach)ok=0;else
#endif
  if(pt_detach(pid,launch.deliver))ok=0;else {attached=0;
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
   if(traceStarted)capture.observation.detached=1;
#endif
  }
 }
 if(detach(&context))ok=0;
 *resumed=!attached&&!context.attached;
 // Filesystem logging only after both trace owners confirmed detach.
 if(*resumed){fprintf(stderr,"ProsperoMenu launch stop: caller=%d thread=%d signal=%d rip=0x%llx restored=%d\n",launch.caller,launch.stoppedThread,launch.signal,(unsigned long long)launch.stoppedRip,launch.restored);
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
 if(traceStarted)log_bootstrap_capture(&capture);
 uint64_t imageEntry=0;if(bytes>=32)memcpy(&imageEntry,image+24,sizeof imageEntry);
 fprintf(stderr,"ProsperoMenu diagnostic deck mapping: pid=%d entry=0x%llx imageEntry=0x%llx loadBias=0x%llx control=0x%llx\n",(int)pid,
  (unsigned long long)entry,(unsigned long long)imageEntry,(unsigned long long)((uint64_t)entry-imageEntry),(unsigned long long)*control);
#endif
 fprintf(stderr,"ProsperoMenu deck injection: check=%s ok=%d observed=%d state=%u phase=%u(%s) error=%u render=%u input=%u\n",check,ok,haveStatus,last.state,last.phase,pm_deck_boot_phase(last.phase),last.error,last.renderPhase,last.inputPhase);fflush(stderr);}
 // The console cannot show this, and stderr goes nowhere the owner can read:
 // write the outcome to a small file on the data folder, readable over FTP.
 if(*resumed){
  FILE* outcome=fopen("/data/RunClock/runtime/deck-status.txt","w");
  if(outcome){
   fprintf(outcome,"deck injection check=%s ok=%d observed=%d state=%u phase=%u(%s) error=%u(%s) render=%u input=%u hooks=%u memoryStage=%u native=%d kernel=%d protection=0x%x errno=%d\n",
    check,ok,haveStatus,haveStatus?last.state:0,haveStatus?last.phase:0,haveStatus?pm_deck_boot_phase(last.phase):"unobserved",haveStatus?last.error:0,
    haveStatus?pm_deck_prepare_error(last.error):"none",haveStatus?last.renderPhase:0,haveStatus?last.inputPhase:0,
    haveStatus?last.count:0,haveStatus?last.codeMemory.stage:0,
    haveStatus?last.codeMemory.nativeResult:0,haveStatus?last.codeMemory.kernelResult:0,
    haveStatus?last.codeMemory.observedProtection:0,haveStatus?last.codeMemory.error:0);
#ifdef PROSPEROMENU_BOOTSTRAP_TRACE
   if(traceStarted)fprintf(outcome,"bootstrap trace signal=%d code=%d fault=0x%llx rip=0x%llx checkpoint=%d timeout=%d disarmed=%d detached=%d stackBytes=%zu\n",
    capture.observation.signal,capture.code,(unsigned long long)capture.fault,(unsigned long long)capture.regs.r_rip,
    capture.observation.checkpoint,capture.observation.timedOut,capture.observation.disarmed,capture.observation.detached,capture.stackBytes);
#endif
   fclose(outcome);
  }
 }
 return ok?0:-1;
}
