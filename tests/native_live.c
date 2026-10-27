/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native input core accepts real encoded clock redraws while keeping action
 * generation stable; label/target/flag changes still require a new generation.
 */
#include "deck-core.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void read_frame(const char *path,PmDeckFrame *f){FILE *in=fopen(path,"rb");assert(in);assert(fread(f,1,sizeof *f,in)==sizeof *f);assert(fgetc(in)==EOF);fclose(in);}
int main(int argc,char **argv){
 assert(argc==6);PmDeckFrame a,b,bad;read_frame(argv[1],&a);read_frame(argv[2],&b);unsigned pause=(unsigned)strtoul(argv[3],NULL,10);
 assert(a.sequence==b.sequence && a.metricsSequence!=b.metricsSequence && pause<a.count);
 assert(a.rows[pause].flags&PM_ROW_ENABLED);assert(memcmp(a.rows,b.rows,sizeof a.rows));
 PmDeck d;pm_deck_init(&d);assert(pm_deck_receive(&d,&a,sizeof a,1000));d.open=1;d.focus=pause;d.selected=(uint8_t)pause;pm_deck_present(&d,a.sequence,1,pause);
 assert(pm_deck_receive(&d,&b,sizeof b,1010));
 PmDeckEvent e;assert(pm_deck_input(&d,0,PM_CROSS,1020));assert(pm_deck_event(&d,&e));assert(e.kind==PM_DECK_SELECT && e.sequence==a.sequence && e.row==pause);pm_deck_ack(&d,e.serial);
 assert(pm_deck_input(&d,0,PM_CROSS,1030));assert(!pm_deck_event(&d,&e));
 unsigned clock=0;while(clock<b.count&&strcmp(b.rows[clock].label,"Active time"))clock++;assert(clock<b.count);
 bad=b;bad.metricsSequence++;bad.rows[clock].label[0]='Z';assert(!pm_deck_receive(&d,&bad,sizeof bad,1040));
 bad=b;bad.metricsSequence++;bad.rows[pause].token[0]=bad.rows[pause].token[0]=='a'?'b':'a';assert(!pm_deck_receive(&d,&bad,sizeof bad,1041));
 bad=b;bad.metricsSequence++;bad.rows[pause].flags^=PM_ROW_ENABLED;assert(!pm_deck_receive(&d,&bad,sizeof bad,1042));
 bad=b;size_t n=strlen(bad.rows[clock].label)+1;bad.rows[clock].label[n]=bad.rows[clock].label[n]=='0'?'1':'0';assert(!pm_deck_receive(&d,&bad,sizeof bad,1043));
 pm_deck_input(&d,0,PM_TRIANGLE,1045);assert(!pm_deck_event(&d,&e));
 assert(!(b.flags&4));pm_deck_input(&d,0,PM_CIRCLE,1050);assert(!d.open);assert(!pm_deck_event(&d,&e));
 PmDeckFrame saved,confirm;read_frame(argv[4],&saved);read_frame(argv[5],&confirm);
 unsigned del=0,yes=0;while(del<saved.count&&!(saved.rows[del].flags&PM_ROW_TRIANGLE))del++;while(yes<confirm.count&&!(confirm.rows[yes].flags&PM_ROW_TRIANGLE))yes++;
 assert(del<saved.count&&yes<confirm.count&&del!=0&&yes!=0);assert(strcmp(confirm.rows[0].label,"Cancel")==0);
 PmDeck deletion;pm_deck_init(&deletion);assert(pm_deck_receive(&deletion,&saved,sizeof saved,2000));deletion.open=1;pm_deck_present(&deletion,saved.sequence,1,deletion.selected);
 pm_deck_input(&deletion,0,PM_TRIANGLE,2010);assert(pm_deck_event(&deletion,&e));assert(e.kind==PM_DECK_SELECT&&e.sequence==saved.sequence&&e.row==del);pm_deck_ack(&deletion,e.serial);
 assert(pm_deck_receive(&deletion,&confirm,sizeof confirm,2020));
 pm_deck_input(&deletion,0,0,2021);pm_deck_input(&deletion,0,PM_TRIANGLE,2022);assert(!pm_deck_event(&deletion,&e)); // unseen confirmation
 pm_deck_present(&deletion,confirm.sequence,1,deletion.selected);pm_deck_input(&deletion,0,PM_TRIANGLE,2023);assert(!pm_deck_event(&deletion,&e)); // held across redraw
 pm_deck_input(&deletion,0,0,2024);pm_deck_input(&deletion,0,PM_TRIANGLE,2025);assert(pm_deck_event(&deletion,&e));assert(e.kind==PM_DECK_SELECT&&e.sequence==confirm.sequence&&e.row==yes);pm_deck_ack(&deletion,e.serial);
 pm_deck_input(&deletion,0,PM_TRIANGLE,2026);assert(!pm_deck_event(&deletion,&e));
 bad=confirm;bad.rows[0].flags|=PM_ROW_TRIANGLE;assert(!pm_deck_decode(&bad,sizeof bad,&a));
 bad=confirm;bad.rows[yes].flags&=~PM_ROW_ENABLED;assert(!pm_deck_decode(&bad,sizeof bad,&a));
 puts("PASS: native live redraw keeps Cross; stale controls rejected; Triangle targets delete then confirmation, requires a rendered page and a new press, cannot repeat while held; main Circle closes locally.");
}
