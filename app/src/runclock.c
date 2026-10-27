/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "runclock.h"
#include <stdio.h>
#include <string.h>

static uint64_t since(uint64_t a, uint64_t b) { return a >= b ? a - b : 0; }
void rc_init(rc_timer *t, size_t count) {
    memset(t, 0, sizeof *t);
    t->count = count > RC_MAX_SPLITS ? RC_MAX_SPLITS : count;
    for (size_t i = 0; i < RC_MAX_SPLITS; ++i) t->splits[i] = RC_UNSET;
}
uint64_t rc_elapsed(const rc_timer *t, uint64_t now) {
    return t->accumulated_ms + (t->phase == RC_RUNNING ? since(now, t->anchor_ms) : 0);
}
uint64_t rc_realtime(const rc_timer *t, uint64_t now) {
    if (t->phase == RC_READY) return 0;
    return since(t->phase == RC_COMPLETE ? t->ended_ms : now, t->started_ms);
}
int rc_start(rc_timer *t, uint64_t now) {
    if (t->phase != RC_READY || !t->count) return 0;
    t->phase = RC_RUNNING; t->started_ms = t->anchor_ms = now; return 1;
}
int rc_pause(rc_timer *t, uint64_t now) {
    if (t->phase != RC_RUNNING) return 0;
    t->accumulated_ms = rc_elapsed(t, now); t->phase = RC_PAUSED; return 1;
}
int rc_resume(rc_timer *t, uint64_t now) {
    if (t->phase != RC_PAUSED) return 0;
    t->anchor_ms = now; t->phase = RC_RUNNING; return 1;
}
int rc_split(rc_timer *t, uint64_t now) {
    if (t->phase != RC_RUNNING || t->next >= t->count) return 0;
    uint64_t value = rc_elapsed(t, now);
    if (t->next && value < t->splits[t->next - 1]) return 0;
    t->splits[t->next++] = value;
    return 1;
}
int rc_finish(rc_timer *t, uint64_t now) {
    if (t->phase != RC_RUNNING && t->phase != RC_PAUSED) return 0;
    uint64_t value = rc_elapsed(t, now);
    if (t->next && value < t->splits[t->next - 1]) return 0;
    t->accumulated_ms = value; t->ended_ms = now; t->phase = RC_COMPLETE;
    return 1;
}
int rc_undo(rc_timer *t) {
    if ((t->phase != RC_RUNNING && t->phase != RC_PAUSED) || !t->next) return 0;
    t->splits[--t->next] = RC_UNSET; return 1;
}
int rc_validate_splits(const uint64_t *times, size_t count) {
    if (!count || count > RC_MAX_SPLITS) return 0;
    for (size_t i = 0; i < count; ++i)
        if (times[i] == RC_UNSET || (i && times[i] < times[i - 1])) return 0;
    return 1;
}
static uint64_t hash_string(uint64_t h, const char *s) {
    for (; *s; ++s) { h ^= (unsigned char)*s; h *= UINT64_C(1099511628211); }
    h ^= 0; return h * UINT64_C(1099511628211);
}
uint64_t rc_route_id(const rc_route *r) {
    uint64_t h = hash_string(UINT64_C(14695981039346656037), r->title);
    h = hash_string(h, r->category);
    for (size_t i = 0; i < r->count; ++i) h = hash_string(h, r->names[i]);
    return h;
}
void rc_format(uint64_t ms, char *out, size_t size) {
    if (ms == RC_UNSET) { snprintf(out, size, "--"); return; }
    unsigned long long s = (unsigned long long)(ms / 1000);
    snprintf(out, size, "%02llu:%02llu:%02llu.%02u", s / 3600, (s / 60) % 60,
             s % 60, (unsigned)((ms % 1000) / 10));
}
