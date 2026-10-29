/* SPDX-License-Identifier: GPL-3.0-or-later
 * Menu companion for ProsperoMenu API 1. All IPC stays on 127.0.0.1.
 * No game hooks, networking service, external connections or memory patches.
 */
#include "pmo.h"
#include "runclock.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define RC_VERSION "1.0.2-beta.3"
#define RC_TITLE "RunClock v" RC_VERSION
#define RC_AUTHOR "BrewSparrow"
#define RECENT_LIMIT 20
typedef struct {
    unsigned attempt;
    char event[16];
    long long unix_time;
    uint64_t active, rta, splits[RC_MAX_SPLITS];
    size_t count;
} saved_run;
static rc_route route;
static rc_timer timer;
static uint64_t best[RC_MAX_SPLITS], best_total = RC_UNSET;
static size_t best_count;
static unsigned attempts, finished, abandoned, interrupted;
static saved_run recent[RECENT_LIMIT];
static size_t recent_count, record_index;
enum { VIEW_TIMER, VIEW_RECORDS, VIEW_RUN, VIEW_DELETE };
static int reset_confirm, storage_ok = 1, view;
static unsigned page_epoch;
static saved_run delete_target;
static int delete_from;
static char records_message[96];
static char last_action[32] = "Ready";
static char base[512], best_path[640], history_path[640];
/* Stock API 1 has no focus notification. Keep each rendered page stable until
 * an action or lifecycle event requests a new snapshot. Bind actions to that
 * snapshot so a queued selection cannot confirm a newly displayed prompt. */
static void next_page(void) { if (!++page_epoch) ++page_epoch; }
static void action(pmo_t *o, const char *id, const char *label,
                   const char *value, const char *detail) {
    char token[48]; snprintf(token, sizeof token, "p%u:%s", page_epoch, id);
    pmo_item(o, token, label, value, detail);
}
static const char *action_name(const char *id) {
    if (*id++ != 'p') return NULL;
    char *end; errno = 0;
    unsigned long epoch = strtoul(id, &end, 10);
    if (errno || end == id || *end != ':' || !end[1] || epoch != page_epoch) return NULL;
    return end + 1;
}
static uint64_t now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts)) { perror("RunClock monotonic clock"); exit(1); }
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static int line(FILE *f, char *out, size_t n) {
    if (!fgets(out, (int)n, f)) return 0;
    size_t len = strcspn(out, "\r\n");
    if (len == n - 1 && out[len - 1] != '\n' && !feof(f)) {
        int ch; while ((ch = fgetc(f)) != '\n' && ch != EOF) { }
        return 0;
    }
    out[len] = 0; return len > 0;
}
static void load_route(pmo_t *o) {
    char path[640]; snprintf(path, sizeof path, "%s/route.txt", pmo_dir(o));
#ifdef __PS5__
    snprintf(path, sizeof path, "/data/RunClock/route.txt");
#endif
    FILE *f = fopen(path, "r");
    if (f) {
        if (!line(f, route.title, sizeof route.title) || !line(f, route.category, sizeof route.category)) route.count = 0;
        else while (route.count < RC_MAX_SPLITS && line(f, route.names[route.count], RC_NAME)) ++route.count;
        fclose(f);
    }
    if (!route.count) {
        memset(&route, 0, sizeof route); strcpy(route.title, "My first game");
        strcpy(route.category, "Any% practice"); route.count = 10;
        for (size_t i = 0; i < route.count; ++i) snprintf(route.names[i], RC_NAME, "Split %zu", i + 1);
    }
    rc_init(&timer, route.count);
}
static void remember(const saved_run *run) {
    if (!strcmp(run->event, "complete")) ++finished;
    else if (!strcmp(run->event, "abandon")) ++abandoned;
    else if (!strcmp(run->event, "interrupted")) ++interrupted;
    else return;
    size_t n = recent_count < RECENT_LIMIT ? recent_count++ : RECENT_LIMIT - 1;
    memmove(recent + 1, recent, n * sizeof *recent);
    recent[0] = *run;
}
/* Read the v1 JSONL format. A partial
 * final line from a failed write is ignored; it never becomes a saved run. */
static int parse_record(const char *text, saved_run *run) {
    int used = 0;
    memset(run, 0, sizeof *run);
    if (sscanf(text, "{\"event\":\"%15[^\"]\",\"unix_time\":%lld,\"active_ms\":%" SCNu64 ",\"rta_ms\":%" SCNu64 ",\"splits\":[%n",
               run->event, &run->unix_time, &run->active, &run->rta, &used) != 4 || !used) return 0;
    const char *p = text + used;
    while (*p != ']') {
        if (*p < '0' || *p > '9' || run->count == route.count) return 0;
        char *end; errno = 0;
        uint64_t value = strtoull(p, &end, 10);
        if (errno || value == RC_UNSET || (run->count && value < run->splits[run->count - 1]) || value > run->active) return 0;
        run->splits[run->count++] = value;
        p = end;
        if (*p == ']') break;
        if (*p++ != ',') return 0;
    }
    if (p[0] != ']' || p[1] != '}' || (p[2] && strcmp(p + 2, "\n") && strcmp(p + 2, "\r\n"))) return 0;
    if (run->active == RC_UNSET || run->rta == RC_UNSET || run->active > run->rta) return 0;
    return 1;
}
static void load_records(void) {
    attempts = finished = abandoned = interrupted = 0; recent_count = 0;
    best_total = RC_UNSET; best_count = 0;
    for (size_t i = 0; i < RC_MAX_SPLITS; ++i) best[i] = RC_UNSET;
    /* History is authoritative. A finish may follow any number of splits;
     * its total is independent of its last checkpoint. Never invent splits. */
    FILE *f = fopen(history_path, "r");
    if (f) {
        char buf[2048]; saved_run run;
        while (fgets(buf, sizeof buf, f)) {
            if (!parse_record(buf, &run)) continue;
            if (!strcmp(run.event, "start")) ++attempts;
            else {
                run.attempt = attempts; remember(&run);
                if (!strcmp(run.event, "complete") && run.active < best_total) {
                    best_total = run.active; best_count = run.count;
                    for (size_t i = 0; i < RC_MAX_SPLITS; ++i) best[i] = RC_UNSET;
                    memcpy(best, run.splits, run.count * sizeof *best);
                }
            }
        }
        if (ferror(f)) storage_ok = 0;
        fclose(f);
    } else if (errno != ENOENT) storage_ok = 0;
}
static int atomic_best(void) {
    char tmp[680]; snprintf(tmp, sizeof tmp, "%s.tmp", best_path);
    FILE *f = fopen(tmp, "w"); if (!f) return 0;
    int ok = fprintf(f, "RUNCLOCK 2 %016" PRIx64 " %zu %" PRIu64 " %zu\n", rc_route_id(&route), route.count, best_total, best_count) >= 0;
    for (size_t i = 0; i < route.count; ++i) if (fprintf(f, "%" PRIu64 "\n", best[i]) < 0) ok = 0;
    if (fflush(f)) ok = 0;
    if (fsync(fileno(f))) ok = 0;
    if (fclose(f)) ok = 0;
    if (!ok || rename(tmp, best_path)) { unlink(tmp); return 0; }
    return 1;
}
static void event_record(const char *event, uint64_t stamp) {
    FILE *f = fopen(history_path, "a"); if (!f) { storage_ok = 0; return; }
    long long unix_time = (long long)time(NULL);
    int ok = fprintf(f, "{\"event\":\"%s\",\"unix_time\":%lld,\"active_ms\":%" PRIu64 ",\"rta_ms\":%" PRIu64 ",\"splits\":[",
                     event, unix_time, rc_elapsed(&timer, stamp), rc_realtime(&timer, stamp)) >= 0;
    for (size_t i = 0; i < timer.next; ++i) if (fprintf(f, "%s%" PRIu64, i ? "," : "", timer.splits[i]) < 0) ok = 0;
    if (fprintf(f, "]}\n") < 0) ok = 0;
    if (fflush(f) || fsync(fileno(f))) ok = 0;
    if (fclose(f)) ok = 0;
    if (!ok) storage_ok = 0;
    else {
        saved_run run = { .attempt = attempts, .unix_time = unix_time,
                          .active = rc_elapsed(&timer, stamp), .rta = rc_realtime(&timer, stamp), .count = timer.next };
        snprintf(run.event, sizeof run.event, "%s", event);
        memcpy(run.splits, timer.splits, timer.next * sizeof *timer.splits);
        remember(&run);
    }
}
static int same_saved_run(const saved_run *a, const saved_run *b) {
    return a->attempt == b->attempt && !strcmp(a->event, b->event) && a->unix_time == b->unix_time &&
           a->active == b->active && a->rta == b->rta && a->count == b->count &&
           !memcmp(a->splits, b->splits, a->count * sizeof *a->splits);
}
/* Remove only the confirmed result. Keep its zero-time start entry so the
 * lifetime start count and all later attempt numbers remain stable. No final
 * time/split payload or hidden tombstone for the deleted result is retained. */
static int delete_saved_run(const saved_run *target) {
    char tmp[680], buf[2048]; saved_run run; unsigned attempt = 0;
    snprintf(tmp, sizeof tmp, "%s.delete.tmp", history_path);
    FILE *in = fopen(history_path, "r"); if (!in) return 0;
    FILE *out = fopen(tmp, "w"); if (!out) { fclose(in); return 0; }
    int ok = 1, removed = 0;
    while (fgets(buf, sizeof buf, in)) {
        if (parse_record(buf, &run)) {
            if (!strcmp(run.event, "start")) ++attempt;
            else {
                run.attempt = attempt;
                if (!removed && same_saved_run(&run, target)) { removed = 1; continue; }
            }
        }
        if (fputs(buf, out) == EOF) ok = 0;
    }
    if (ferror(in)) ok = 0;
    if (fclose(in)) ok = 0;
    if (fflush(out)) ok = 0;
    if (fsync(fileno(out))) ok = 0;
    if (fclose(out)) ok = 0;
    if (!ok || !removed) { unlink(tmp); return 0; }
    if (rename(tmp, history_path)) { unlink(tmp); return 0; }
    load_records();
    if (!atomic_best()) storage_ok = 0; /* Recovered from JSONL on next load if interrupted. */
    return 1;
}
static void arm_delete(const saved_run *run) {
    delete_target = *run; delete_from = view; view = VIEW_DELETE;
    records_message[0] = 0;
}
static const char *phase_name(void) {
    switch (timer.phase) { case RC_RUNNING: return "RUNNING"; case RC_PAUSED: return "PAUSED"; case RC_COMPLETE: return "COMPLETE"; default: return "READY"; }
}
static const char *outcome(const saved_run *run) {
    return !strcmp(run->event, "complete") ? "Finished" : !strcmp(run->event, "abandon") ? "Reset" : "Interrupted";
}
static void record_date(const saved_run *run, char *out, size_t size) {
    time_t stamp = (time_t)run->unix_time;
    struct tm tm;
    if (!gmtime_r(&stamp, &tm) || !strftime(out, size, "%Y-%m-%d %H:%M UTC", &tm)) snprintf(out, size, "Date unavailable");
}
static void draw_records(pmo_t *o) {
    char value[48], detail[160], id[48], label[96];
    next_page();
    if (view == VIEW_DELETE) {
        pmo_page_begin(o, RC_TITLE " | Delete saved attempt?", RC_AUTHOR);
        snprintf(label, sizeof label, "Attempt %u | %s", delete_target.attempt, outcome(&delete_target));
        rc_format(delete_target.active, value, sizeof value);
        action(o, "cancel-delete", "Cancel", NULL, "Keep this saved attempt. Circle also cancels.");
        action(o, "delete-confirm", "Confirm delete", value, label);
        pmo_item_disabled(o, "delete-note", "Saved result will be removed", NULL, "Its saved times/splits will be deleted. PB is recalculated. Lifetime attempts started stays unchanged.");
        pmo_footer(o, "v" RC_VERSION " | Triangle Confirm delete | Circle Cancel");
        pmo_page_end(o); return;
    }
    pmo_page_begin(o, view == VIEW_RUN ? RC_TITLE " | Saved attempt" : RC_TITLE " | Records", RC_AUTHOR);
    action(o, "back", view == VIEW_RUN ? "Back to records" : "Back to timer", NULL, "Timing continues while browsing records.");
    if (records_message[0]) pmo_item_disabled(o, "records-message", "Last record action", NULL, records_message);
    if (view == VIEW_RUN && record_index < recent_count) {
        const saved_run *run = &recent[record_index];
        snprintf(label, sizeof label, "Attempt %u | %s", run->attempt, outcome(run));
        record_date(run, detail, sizeof detail); pmo_header(o, label);
        pmo_item_disabled(o, "date", "Saved", NULL, detail);
        rc_format(run->active, value, sizeof value); pmo_item_disabled(o, "saved-active", "Active time", value, "Manual pauses excluded.");
        rc_format(run->rta, value, sizeof value); pmo_item_disabled(o, "saved-rta", "Real elapsed time", value, "Includes manual pauses.");
        action(o, "delete", "Delete attempt", NULL, "Triangle opens confirmation; press Triangle again to delete. Circle cancels.");
        pmo_header(o, "Recorded splits");
        for (size_t i = 0; i < route.count; ++i) {
            snprintf(id, sizeof id, "saved%zu", i);
            rc_format(i < run->count ? run->splits[i] : RC_UNSET, value, sizeof value);
            pmo_item_disabled(o, id, route.names[i], value, i < run->count ? "Cumulative active time." : "Checkpoint not recorded.");
        }
    } else {
        pmo_header(o, "This route");
        snprintf(value, sizeof value, "%u", attempts); pmo_item_disabled(o, "attempts", "Attempts started", value, "Includes finished, reset, interrupted and ongoing attempts.");
        snprintf(value, sizeof value, "%u", finished); pmo_item_disabled(o, "finished", "Finished runs", value, "Successfully saved completions.");
        snprintf(value, sizeof value, "%u", abandoned); pmo_item_disabled(o, "abandoned", "Reset attempts", value, "Successfully saved manual resets.");
        snprintf(value, sizeof value, "%u", interrupted); pmo_item_disabled(o, "interrupted", "Interrupted attempts", value, "Cleanly stopped unfinished attempts. Crashes may leave only a start entry.");
        rc_format(best_total, value, sizeof value);
        pmo_item_disabled(o, "pb", "Personal best", value, "Fastest completed active time for this route.");
        pmo_header(o, "Personal best splits");
        for (size_t i = 0; i < route.count; ++i) {
            snprintf(id, sizeof id, "pb%zu", i); rc_format(best[i], value, sizeof value);
            pmo_item_disabled(o, id, route.names[i], value, "Cumulative time from the personal best run.");
        }
        pmo_header(o, "Recent saved attempts (newest first)");
        if (!recent_count) pmo_item_disabled(o, "empty", "No saved attempts yet", NULL, "Finish or reset an attempt to create a record.");
        for (size_t i = 0; i < recent_count; ++i) {
            snprintf(id, sizeof id, "record%zu", i);
            snprintf(label, sizeof label, "Attempt %u | %s", recent[i].attempt, outcome(&recent[i]));
            rc_format(recent[i].active, value, sizeof value); record_date(&recent[i], detail, sizeof detail);
            action(o, id, label, value, detail);
        }
        pmo_item_disabled(o, "storage", "Storage", storage_ok ? "Saved locally" : "Storage error", "All history is kept in /data/RunClock. This list shows the latest 20 saved attempts.");
    }
    pmo_footer(o, view == VIEW_RUN ? "v" RC_VERSION " | Triangle Delete | X Select | Circle Back" : "v" RC_VERSION " | X Open/select | Circle Back");
    pmo_page_end(o);
}
static void draw(pmo_t *o) {
    char value[48], detail[160], id[24];
    uint64_t stamp = now_ms();
    if (view != VIEW_TIMER) { draw_records(o); return; }
    next_page();
    pmo_page_begin(o, RC_TITLE, RC_AUTHOR);
    pmo_header(o, "Live timer");
    rc_format(rc_elapsed(&timer, stamp), value, sizeof value);
    pmo_item_disabled(o, "clock", "Active time", value, "Manual pauses excluded. This clock updates automatically while the timer is open.");
    rc_format(rc_realtime(&timer, stamp), value, sizeof value);
    pmo_item_disabled(o, "rta", "Real elapsed time", value, "Includes manual pauses. Both clocks stop at Finish.");
    pmo_item_disabled(o, "state", "Status", phase_name(), "The timer continues while the menu is closed or Records is open.");
    pmo_item_disabled(o, "last", "Last action", last_action, "Confirms the most recent action RunClock received.");
    pmo_header(o, "Controls");
    if (timer.phase == RC_READY) action(o, "primary", "Start run", NULL, "Starts both timers at zero.");
    else if (timer.phase == RC_RUNNING && timer.next < route.count) action(o, "primary", "Split", NULL, route.names[timer.next]);
    else if (timer.phase == RC_RUNNING) pmo_item_disabled(o, "primary", "All splits recorded", NULL, "Timing continues. Select Finish run when the run ends.");
    else if (timer.phase == RC_PAUSED) action(o, "primary", "Resume", NULL, "Continue the active timer.");
    else action(o, "primary", "New attempt", NULL, "Clear both clocks and return to ready. Saved results remain.");
    if (timer.phase == RC_RUNNING) action(o, "pause", "Pause active timer", NULL, "Real elapsed time keeps running.");
    if ((timer.phase == RC_RUNNING || timer.phase == RC_PAUSED) && timer.next)
        action(o, "undo", "Undo last split", NULL, "Does not rewind the clock.");
    if (timer.phase == RC_RUNNING || timer.phase == RC_PAUSED)
        action(o, "finish", "Finish run", NULL, "Freeze both clocks and save the actual splits recorded so far.");
    else pmo_item_disabled(o, "finish", "Finish run", NULL, "Start a run first. A completed run cannot be saved twice.");
    action(o, "reset", reset_confirm ? "Confirm reset clocks" : "Reset clocks", NULL,
           reset_confirm ? "Select again to save this unfinished attempt as reset and zero both clocks." : "Zero both clocks and return to ready. An unfinished run needs confirmation.");
    action(o, "records", "Records", NULL, "Open personal bests, attempt counts and recent saved runs.");
    pmo_header(o, "Splits / personal best");
    for (size_t i = 0; i < route.count; ++i) {
        char pb[40]; rc_format(timer.splits[i], value, sizeof value); rc_format(best[i], pb, sizeof pb);
        snprintf(id, sizeof id, "split%zu", i);
        snprintf(detail, sizeof detail, "%sPersonal best: %s", timer.next == i && timer.phase != RC_COMPLETE ? "Next checkpoint. " : "", pb);
        pmo_item_disabled(o, id, route.names[i], value, detail);
    }
    pmo_item_disabled(o, "route-info", "Current route", route.title, route.category);
    snprintf(detail, sizeof detail, "%u attempts started | %s", attempts, storage_ok ? "Saved locally" : "Storage error: check overlay.log and permissions");
    pmo_item_disabled(o, "storage", "Storage", NULL, detail);
    pmo_footer(o, "v" RC_VERSION " | X Select | Circle Back | Live clocks");
    pmo_page_end(o);
}
int main(void) {
    pmo_t *o = pmo_connect("runclock"); if (!o) return 1;
    load_route(o);
    const char *test_root = getenv("RUNCLOCK_DATA_DIR");
#ifdef __PS5__
    snprintf(base, sizeof base, "/data/RunClock");
#else
    snprintf(base, sizeof base, "%s", test_root && *test_root ? test_root : ".runclock-data");
#endif
    (void)test_root;
    if (mkdir(base, 0700) && errno != EEXIST) storage_ok = 0;
    snprintf(best_path, sizeof best_path, "%s/%016" PRIx64 ".best", base, rc_route_id(&route));
    snprintf(history_path, sizeof history_path, "%s/%016" PRIx64 ".jsonl", base, rc_route_id(&route));
    load_records(); draw(o);
    for (;;) {
        pmo_event_t ev; int r = pmo_wait(o, &ev, 100);
        if (r < 0 || (r && ev.kind == PMO_EV_QUIT)) break;
        if (r && ev.kind == PMO_EV_SHOW) { view = VIEW_TIMER; reset_confirm = 0; memset(&delete_target, 0, sizeof delete_target); draw(o); }
        if (r && ev.kind == PMO_EV_HIDE) { next_page(); reset_confirm = 0; if (view == VIEW_DELETE) view = VIEW_RECORDS; memset(&delete_target, 0, sizeof delete_target); }
        if (r && ev.kind == PMO_EV_BACK) {
            reset_confirm = 0;
            if (view == VIEW_DELETE) { view = delete_from; memset(&delete_target, 0, sizeof delete_target); draw(o); }
            else if (view != VIEW_TIMER) { view = view == VIEW_RUN ? VIEW_RECORDS : VIEW_TIMER; draw(o); } else pmo_exit(o);
        }
        if (r && ev.kind == PMO_EV_SELECT && pmo_visible(o)) {
            const char *id = action_name(ev.id);
            if (!id) continue;
            uint64_t stamp = now_ms();
            fprintf(stderr, "RunClock " RC_VERSION " input=%s phase=%s next=%zu\n", id, phase_name(), timer.next);
            fflush(stderr);
            if (strcmp(id, "reset")) reset_confirm = 0;
            if (view == VIEW_DELETE) {
                if (!strcmp(id, "cancel-delete")) view = delete_from;
                else if (!strcmp(id, "delete-confirm")) {
                    if (delete_target.attempt && delete_saved_run(&delete_target)) {
                        snprintf(records_message, sizeof records_message, "Saved attempt %u deleted.", delete_target.attempt);
                        strcpy(last_action, "Saved attempt deleted");
                    } else snprintf(records_message, sizeof records_message, "Could not delete this saved attempt. Records were not changed.");
                    view = VIEW_RECORDS;
                } else continue;
                memset(&delete_target, 0, sizeof delete_target); draw(o); continue;
            }
            if (view == VIEW_RUN && !strcmp(id, "delete") && record_index < recent_count) {
                arm_delete(&recent[record_index]); draw(o); continue;
            }
            if (!strcmp(id, "back")) { view = view == VIEW_RUN ? VIEW_RECORDS : VIEW_TIMER; draw(o); continue; }
            if (!strcmp(id, "records")) { view = VIEW_RECORDS; draw(o); continue; }
            if (view == VIEW_RECORDS && !strncmp(id, "record", 6)) {
                char *end; unsigned long index = strtoul(id + 6, &end, 10);
                if (end != id + 6 && !*end && index < recent_count) { record_index = index; view = VIEW_RUN; }
                draw(o); continue;
            }
            if (view != VIEW_TIMER) continue;
            if (!strcmp(id, "primary")) {
                if (timer.phase == RC_READY) { rc_start(&timer, stamp); ++attempts; event_record("start", stamp); strcpy(last_action, "Started"); }
                else if (timer.phase == RC_PAUSED) { rc_resume(&timer, stamp); strcpy(last_action, "Resumed"); }
                else if (timer.phase == RC_COMPLETE) { rc_init(&timer, route.count); strcpy(last_action, "Ready for next attempt"); }
                else if (rc_split(&timer, stamp)) strcpy(last_action, "Split recorded");
            } else if (!strcmp(id, "finish")) {
                if (rc_finish(&timer, stamp)) {
                    strcpy(last_action, "Finished");
                    unsigned previous_finished = finished;
                    event_record("complete", stamp);
                    if (finished != previous_finished && timer.accumulated_ms < best_total) {
                        best_total = timer.accumulated_ms; best_count = timer.next;
                        memcpy(best, timer.splits, sizeof best);
                        if (!atomic_best()) storage_ok = 0;
                        pmo_notice(o, "RunClock: personal best %llu ms", (unsigned long long)best_total);
                    } else if (finished == previous_finished) strcpy(last_action, "Finished: storage error");
                }
            } else if (!strcmp(id, "pause")) { if (rc_pause(&timer, stamp)) strcpy(last_action, "Paused"); }
            else if (!strcmp(id, "undo")) { if (rc_undo(&timer)) strcpy(last_action, "Split undone"); }
            else if (!strcmp(id, "reset")) {
                if ((timer.phase == RC_RUNNING || timer.phase == RC_PAUSED) && !reset_confirm) {
                    reset_confirm = 1; strcpy(last_action, "Reset pending confirmation");
                } else {
                    if (timer.phase == RC_RUNNING || timer.phase == RC_PAUSED) event_record("abandon", stamp);
                    rc_init(&timer, route.count); reset_confirm = 0; strcpy(last_action, "Both clocks reset");
                }
            }
            draw(o);
        }

        if (view == VIEW_TIMER && pmo_visible(o) &&
            (timer.phase == RC_RUNNING || timer.phase == RC_PAUSED)) {
            char value[40]; uint64_t tick = now_ms();
            rc_format(rc_elapsed(&timer, tick), value, sizeof value); pmo_patch_value(o, "clock", value);
            rc_format(rc_realtime(&timer, tick), value, sizeof value); pmo_patch_value(o, "rta", value);
        }

    }
    if (timer.phase == RC_RUNNING || timer.phase == RC_PAUSED) event_record("interrupted", now_ms());
    pmo_close(o); return 0;
}
