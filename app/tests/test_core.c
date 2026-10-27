/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "runclock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    rc_timer t; char text[40];
    rc_init(&t, 3);
    assert(!rc_finish(&t, 90) && !rc_split(&t, 90) && !rc_pause(&t, 90) && !rc_resume(&t, 90));
    assert(rc_start(&t, 1000)); assert(!rc_start(&t, 1001));
    assert(rc_elapsed(&t, 2100) == 1100);
    /* Sampling gaps / a hidden menu do not slow the clock. */
    assert(rc_elapsed(&t, 61000) == 60000);
    assert(rc_split(&t, 61000)); assert(t.splits[0] == 60000);
    assert(rc_pause(&t, 62000));
    assert(rc_elapsed(&t, 90000) == 61000);
    assert(rc_realtime(&t, 90000) == 89000);
    assert(!rc_split(&t, 90000));
    assert(rc_resume(&t, 92000));
    assert(rc_split(&t, 94000)); assert(t.splits[1] == 63000);
    assert(rc_undo(&t)); assert(t.next == 1 && t.splits[1] == RC_UNSET);
    assert(rc_split(&t, 95000)); assert(rc_split(&t, 96000));
    assert(t.phase == RC_RUNNING && !rc_split(&t, 96001));
    assert(rc_finish(&t, 96000));
    assert(t.phase == RC_COMPLETE && rc_elapsed(&t, 999999) == 65000);
    assert(rc_realtime(&t, 999999) == 95000);
    assert(!rc_undo(&t) && !rc_split(&t, 999999) && !rc_resume(&t, 999999));
    assert(rc_validate_splits(t.splits, 3));
    uint64_t bad[] = {100, 99}; assert(!rc_validate_splits(bad, 2));
    assert(!rc_validate_splits(bad, 0));
    rc_init(&t, 0); assert(!rc_start(&t, 0));
    rc_init(&t, 999); assert(t.count == RC_MAX_SPLITS);
    rc_init(&t, 1); assert(rc_start(&t, 0)); assert(rc_realtime(&t, 7) == 7);
    assert(rc_elapsed(&t, 0) == 0); assert(rc_split(&t, 5));
    rc_init(&t, 10); assert(rc_start(&t, 100));
    for (size_t i = 0; i < 10; ++i) assert(rc_split(&t, 200 + i * 100));
    assert(t.phase == RC_RUNNING && t.next == 10 && !rc_split(&t, 1200));
    assert(rc_finish(&t, 1500));
    assert(t.splits[9] == 1000 && rc_elapsed(&t, 999999) == 1400);
    assert(!rc_finish(&t, 2000));
    rc_init(&t, 10); assert(rc_start(&t, 100));
    assert(rc_split(&t, 125)); assert(rc_pause(&t, 150));
    assert(rc_finish(&t, 200)); assert(t.next == 1 && t.splits[0] == 25);
    assert(rc_elapsed(&t, 9999) == 50 && rc_realtime(&t, 9999) == 100);
    rc_init(&t, 10); assert(rc_start(&t, 0)); assert(rc_finish(&t, 50));
    assert(t.next == 0 && t.splits[0] == RC_UNSET && rc_elapsed(&t, 99) == 50);
    rc_init(&t, 10); assert(rc_elapsed(&t, 999) == 0 && rc_realtime(&t, 999) == 0);
    rc_format(3723456, text, sizeof text); assert(!strcmp(text, "01:02:03.45"));
    rc_route r = {.title="Game", .category="Any%", .names={"A", "B"}, .count=2};
    uint64_t id = rc_route_id(&r); strcpy(r.names[1], "C"); assert(id != rc_route_id(&r));
    puts("RunClock core: all timing, state, route and boundary checks passed.");
}
