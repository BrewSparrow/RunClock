// SPDX-License-Identifier: GPL-3.0-or-later
#include "deck-receipt.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 // An intent can expire cleanly only after its worker has stopped. A prepared,
 // published or uncertain receipt never permits retry, even with a bad flag.
 assert(pm_deck_receipt_clean(1, 1, 0));
 assert(!pm_deck_receipt_clean(1, 1, 1));
 assert(!pm_deck_receipt_clean(1, 0, 0));
 for (unsigned stage = 2; stage <= 5; stage++) {
  assert(pm_deck_receipt_clean(stage, 1, 0) == (stage == 4));
  assert(!pm_deck_receipt_clean(stage, 0, 0));
 }
 assert(!pm_deck_receipt_clean(0, 1, 0));
 assert(!pm_deck_receipt_clean(6, 1, 0));
 for(unsigned stage=1;stage<=5;stage++){
  assert(!pm_deck_receipt_blocks(stage,0,0,0)); // verified original process ended
  assert(pm_deck_receipt_blocks(stage,0,0,1));  // current owner
  assert(pm_deck_receipt_blocks(stage,0,0,-1)); // failed process inspection
 }
 assert(!pm_deck_receipt_blocks(1,1,0,-1));
 assert(pm_deck_receipt_blocks(1,1,1,1));
 puts("PASS: native receipt retry policy preserves published and uncertain hooks.");
 return 0;
}
