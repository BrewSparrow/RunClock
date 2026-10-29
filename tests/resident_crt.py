#!/usr/bin/env python3
"""Run the actual patched SDK initializer with its native boundaries mocked."""
from pathlib import Path
import importlib.util, subprocess

root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('resident_crt',root/'engine/scripts/prepare-resident-crt.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
prepared=module.prepare(root/'app/build/resident-crt-source-fixture')
text=(prepared/'kernel.c').read_text()
assert 'kernel_find_iommu_softc' not in text
assert text.count('pm_deck_seed_iommu(args, &KERNEL_ADDRESS_IOMMU_SOFTC)')==1
# Substitute only OS-dependent boundaries. Keep the production __kernel_init
# body, argument setup and firmware table exactly as generated for the ELF.
for signature,name in [('unsigned int\nkernel_get_fw_version(void)','test_unused_fw_version'),
                       ('unsigned long\nkernel_dynlib_dlsym(','test_unused_dynlib_dlsym'),
                       ('unsigned long\nkernel_get_ucred_prison(','test_unused_prison')]:
    assert text.count(signature)==1
    head,tail=signature.split('\n',1)
    original=tail.split('(')[0]
    text=text.replace(signature,head+'\n'+tail.replace(original,name,1))
text=module.remove_function(text,'static unsigned long\nkernel_find_dmap_base(void)')
text=text.replace('int\n__kernel_init(payload_args_t* args)',
                  'static unsigned long kernel_find_dmap_base(void);\nint\n__kernel_init(payload_args_t* args)')
fixture=r'''
#include <assert.h>
#include <stdio.h>
static int seedCalls,seedFail,lookupCalls;
static int nativeErrno;
static unsigned int mockFirmware=0x13600007;
static int* mock_error(void){return &nativeErrno;}
unsigned int kernel_get_fw_version(void){return mockFirmware;}
unsigned long kernel_get_ucred_prison(int pid){assert(!pid);return 0xffff800010000000UL;}
static unsigned long kernel_find_dmap_base(void){return 0xffff900000000000UL;}
unsigned long kernel_dynlib_dlsym(int pid,unsigned int handle,const char* name){
 assert(pid==-1&&handle==1);assert(!strncmp(name,"__error",8));lookupCalls++;return (unsigned long)mock_error;
}
int pm_deck_seed_iommu(payload_args_t* args,unsigned long* out){
 assert(args->kdata_base_addr==(long)0xffffffff80000000UL);seedCalls++;
 if(seedFail)return -1;*out=0xffff800020000000UL;return 0;
}
int main(void){
 int pair[2]={9,10},pipe[2]={11,12};
 payload_args_t args={.rwpair=pair,.rwpipe=pipe,.kpipe_addr=(long)0xffff800030000000UL,.kdata_base_addr=(long)0xffffffff80000000UL};
 assert(!__kernel_init(&args));assert(seedCalls==1&&lookupCalls==1);
 assert(rw_pair[0]==9&&rw_pair[1]==10&&rw_pipe[0]==11&&rw_pipe[1]==12);
 assert(pipe_addr==(unsigned long)args.kpipe_addr&&KERNEL_ADDRESS_IOMMU_SOFTC==0xffff800020000000UL);
 assert(KERNEL_ADDRESS_ALLPROC==0xffffffff828c9e80UL);
 seedCalls=lookupCalls=0;seedFail=1;KERNEL_ADDRESS_IOMMU_SOFTC=0;
 assert(__kernel_init(&args)==-ENOSYS&&seedCalls==1&&!lookupCalls&&!KERNEL_ADDRESS_IOMMU_SOFTC);
 seedCalls=0;seedFail=0;pair[0]=-1;
 assert(__kernel_init(&args)==-EBADF&&!seedCalls);
 pair[0]=9;mockFirmware=0;
 assert(__kernel_init(&args)==-ENOSYS&&!seedCalls);
 puts("PASS: actual resident SDK initializer preserves handle/firmware setup, uses one checked seed, and refuses bad context without a scan.");
 return 0;
}
'''
out=root/'app/build/resident-crt-test';out.mkdir(parents=True,exist_ok=True)
unit=out/'sdk-init.c';unit.write_text('unsigned int kernel_get_fw_version(void);\n'+text+'\n'+fixture)
exe=out/'sdk-init'
subprocess.run(['cc','-std=c11','-O1','-ffreestanding','-fno-builtin','-ffunction-sections','-fdata-sections',
                '-I'+str(prepared),str(unit),'-Wl,--gc-sections','-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
