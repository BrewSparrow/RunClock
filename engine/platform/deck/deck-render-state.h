// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include "deck-core.h"
// Snapshot copied under the deck mutex, then consumed by the UI callback.
static inline void pm_deck_render_frame(const PmDeck* deck,PmDeckFrame* frame){
 *frame=deck->frame;
 // The local controller owns pane focus between coordinator replies. Do not
 // mutate the received frame or derive pane ownership from toolbar position.
 if(frame->flags&256)frame->browser.active=deck->browserPane;
}
typedef struct { int ready,visible,close;uint64_t heartbeat; } PmDeckRenderState;
static inline PmDeckRenderState pm_deck_render_state(int result,int open,uint32_t sequence,uint64_t now){
 // A valid closed/detached frame proves the UI callback is alive. Waiting for
 // the menu to be visible would deadlock readiness with its opening command.
 // A scene awaiting activation (2) is alive. Reporting it unready reset the
 // game-input epoch just as L3+R3 opened the menu. Actions remain gated by the
 // separately recorded, actually presented frame in pm_deck_input_at().
 int alive=result==1||result==2;
 return (PmDeckRenderState){alive,result==1&&open,result==0,alive&&sequence?now:0};
}
