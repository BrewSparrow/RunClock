// SPDX-License-Identifier: GPL-3.0-or-later
// Real display frames plus the same input, route and render-state code used by
// the resident. These tests do not emulate Mono or inject a console process.
#include "deck-core.h"
#include "deck-route.h"
#include "deck-render-state.h"
#include "runtime-log.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void load(const char* path,PmDeckFrame* frame){
 FILE* in=fopen(path,"rb");assert(in);assert(fread(frame,1,sizeof *frame,in)==sizeof *frame);assert(fgetc(in)==EOF);assert(!fclose(in));
}
static void write_log(const char* root,const char* name,int number){
 char path[512];snprintf(path,sizeof path,"%s/%s",root,name);FILE* out=fopen(path,"w");assert(out);fprintf(out,"launch %d\n",number);assert(!fclose(out));
}
static void check_log(const char* root,const char* name,int number){
 char path[512],line[32],expected[32];snprintf(path,sizeof path,"%s/%s",root,name);FILE* in=fopen(path,"r");assert(in);assert(fgets(line,sizeof line,in));fclose(in);snprintf(expected,sizeof expected,"launch %d\n",number);assert(!strcmp(line,expected));
}
int main(int argc,char** argv){
 assert(argc==2);PmDeckFrame frame;load(argv[1],&frame);PmDeck d;pm_deck_init(&d);assert(pm_deck_receive(&d,&frame,sizeof frame,1000));
 const unsigned chord=PM_L3|PM_R3;
 pm_deck_input_sample(&d,0,chord,0,1100);
 pm_deck_input_sample(&d,0,0,1,1250); // skipped sample must not be a release
 pm_deck_input_sample(&d,0,chord,0,1450);assert(d.open);
 for(unsigned t=1460;t<2300;t+=10){
  pm_deck_input_sample(&d,0,0,1,t);pm_deck_input_sample(&d,0,chord,0,t+1);assert(d.open);
 }
 // Releasing only one stick must not arm the same held chord a second time.
 pm_deck_input_sample(&d,0,PM_L3,0,2350);
 pm_deck_input_sample(&d,0,chord,0,2400);
 pm_deck_input_sample(&d,0,chord,0,2750);assert(d.open);
 // A brief neutral/bounced sample must not re-arm it either.
 pm_deck_input_sample(&d,0,0,0,2760);
 pm_deck_input_sample(&d,0,chord,0,2780);
 pm_deck_input_sample(&d,0,chord,0,3130);assert(d.open);
 // A deliberate full release and a fresh hold still close the menu.
 pm_deck_input_sample(&d,0,0,0,3150);pm_deck_input_sample(&d,0,0,0,3260);
 pm_deck_input_sample(&d,0,chord,0,3300);pm_deck_input_sample(&d,0,chord,0,3650);assert(!d.open);

 PmDeck opening;pm_deck_init(&opening);assert(pm_deck_receive(&opening,&frame,sizeof frame,1000));opening.open=1;
 unsigned row=0;while(row<frame.count&&!(frame.rows[row].flags&PM_ROW_ENABLED))row++;assert(row<frame.count);
 opening.focus=row;opening.selected=(uint8_t)row;
 PmDeckRenderState pending=pm_deck_render_state(2,1,frame.sequence,1010);
 assert(pending.ready&&!pending.visible&&!pending.close&&pending.heartbeat==1010);
 PmDeckEvent event;pm_deck_input(&opening,0,PM_CROSS,1020);assert(!pm_deck_event(&opening,&event));
 PmDeckRenderState shown=pm_deck_render_state(1,1,frame.sequence,1030);
 assert(shown.ready&&shown.visible&&!shown.close);
 pm_deck_present(&opening,frame.sequence,1,row);pm_deck_input(&opening,0,0,1031);
 pm_deck_input(&opening,0,PM_CROSS,1032);assert(pm_deck_event(&opening,&event));assert(event.row==row);
 PmDeckRenderState failed=pm_deck_render_state(0,1,frame.sequence,1040);assert(!failed.ready&&!failed.visible&&failed.close);

 PmDeckInput in={.active=1,.epoch=1,.captured=1,.owner=7,.deadline=3000};
 pm_deck_route_focus(&in,-1);assert(in.active==1&&in.captured&&in.owner==7&&in.deadline==3000);
 pm_deck_route_focus(&in,-2);assert(in.active==1&&in.captured);
 pm_deck_route_focus(&in,0);assert(in.active==0&&!in.captured&&!in.deadline);
 in.active=2;pm_deck_route_focus(&in,1);assert(in.active==2&&!in.captured);

 // Open through the actual raw game route, then acknowledge capture while
 // the scene is pending. The route and menu stay live, actions await display.
 PmDeck game;PmDeckRoute route={0};pm_deck_init(&game);assert(pm_deck_receive(&game,&frame,sizeof frame,1000));
 in=(PmDeckInput){.active=1,.epoch=1,.serial=1,.deadline=1700,.owner=-1,.count=2};
 in.events[0]=(PmDeckPad){.buttons=6,.handle=7,.connection=0,.at=1000};
 in.events[1]=(PmDeckPad){.buttons=6,.handle=7,.connection=0,.at=1350};
 pm_deck_route_receive(&game,&route,&in,1350);assert(game.open&&!pm_deck_route_visible(&game,&route));
 PmDeckFeedback feedback=pm_deck_route_feedback(&game,&route,pending.ready,pending.heartbeat);
 assert(feedback.ready&&feedback.open&&feedback.owner==7);
 in.serial=2;in.captured=1;in.owner=7;in.captureAt=1370;in.count=1;in.events[0].buttons=0;in.events[0].at=1400;
 pm_deck_route_receive(&game,&route,&in,1400);assert(game.open&&pm_deck_route_visible(&game,&route));
 pm_deck_route_tick(&game,&route,1700);assert(!game.open&&route.active==2); // lease limit remains enforced

 char root[]="/tmp/runclock-logs-XXXXXX";assert(mkdtemp(root));
 for(int launch=1;launch<=5;launch++){
  assert(!pm_runtime_rotate_logs(root));write_log(root,"runtime.log",launch);
  if(launch>1)check_log(root,"runtime.previous.log",launch-1);
  if(launch>2)check_log(root,"runtime.2.log",launch-2);
  if(launch>3)check_log(root,"runtime.3.log",launch-3);
 }
 const char* names[]={"runtime.log","runtime.previous.log","runtime.2.log","runtime.3.log"};
 for(unsigned i=0;i<4;i++){char path[512];snprintf(path,sizeof path,"%s/%s",root,names[i]);assert(!unlink(path));}assert(!rmdir(root));
 puts("PASS: held/bounced/skipped L3+R3 input, pending-scene heartbeat/action gating, unknown focus, raw capture acknowledgement/expiry and launch-log preservation.");
 return 0;
}
