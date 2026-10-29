// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdio.h>
#include <errno.h>

// Preserve the first failed launch when the next one reports only its saved
// ownership guard. No records/routes are touched; retain three older logs.
static inline int pm_runtime_rotate_logs(const char* root){
 char from[512],to[512];int error=0;
 const char* source[]={"runtime.2.log","runtime.previous.log","runtime.log"};
 const char* target[]={"runtime.3.log","runtime.2.log","runtime.previous.log"};
 for(unsigned i=0;i<3;i++){
  if(snprintf(from,sizeof from,"%s/%s",root,source[i])>=(int)sizeof from||
     snprintf(to,sizeof to,"%s/%s",root,target[i])>=(int)sizeof to){error=ENAMETOOLONG;break;}
  if(rename(from,to)&&errno!=ENOENT&&!error)error=errno;
 }
 return error;
}
