/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RUNCLOCK_H
#define RUNCLOCK_H
#include <stddef.h>
#include <stdint.h>

#define RC_MAX_SPLITS 32
#define RC_NAME 96
#define RC_UNSET UINT64_MAX
typedef enum { RC_READY, RC_RUNNING, RC_PAUSED, RC_COMPLETE } rc_phase;
typedef struct {
    char title[RC_NAME], category[RC_NAME];
    char names[RC_MAX_SPLITS][RC_NAME];
    size_t count;
} rc_route;
typedef struct {
    rc_phase phase;
    size_t count, next;
    uint64_t started_ms, anchor_ms, accumulated_ms, ended_ms;
    uint64_t splits[RC_MAX_SPLITS];
} rc_timer;

void rc_init(rc_timer *t, size_t count);
uint64_t rc_elapsed(const rc_timer *t, uint64_t now_ms);
uint64_t rc_realtime(const rc_timer *t, uint64_t now_ms);
int rc_start(rc_timer *t, uint64_t now_ms);
int rc_pause(rc_timer *t, uint64_t now_ms);
int rc_resume(rc_timer *t, uint64_t now_ms);
int rc_split(rc_timer *t, uint64_t now_ms);
int rc_finish(rc_timer *t, uint64_t now_ms);
int rc_undo(rc_timer *t); /* Running/paused only; completed records are immutable. */
int rc_validate_splits(const uint64_t *times, size_t count);
uint64_t rc_route_id(const rc_route *r);
void rc_format(uint64_t ms, char *out, size_t size);
#endif
