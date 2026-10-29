// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// Stage 1 is the durable intent before the worker can publish stage 2 and enter
// the injector. If its coordinator has exited, stage 1 + clean proves that no
// injection began. Stage 2 or later publication must never be inferred clean
// from a missing heartbeat. Stage 4 is the explicit clean-rejection receipt.
static inline int pm_deck_receipt_clean(unsigned stage, unsigned clean, int running) {
 return clean && (stage == 4 || (stage == 1 && !running));
}
static inline int pm_deck_receipt_blocks(unsigned stage,unsigned clean,int running,int owner) {
 return !pm_deck_receipt_clean(stage,clean,running) && owner!=0;
}
