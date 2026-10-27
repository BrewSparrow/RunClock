/* SPDX-License-Identifier: GPL-3.0-or-later. Desktop reference implementation of src/runclock.c. */
(function (root) {
  'use strict';
  class Timer {
    constructor(count) { this.count = count; this.reset(); }
    reset() { this.phase = 'ready'; this.next = 0; this.startAt = 0; this.anchor = 0; this.active = 0; this.endAt = 0; this.splits = Array(this.count).fill(null); }
    elapsed(now) { return this.active + (this.phase === 'running' ? Math.max(0, now - this.anchor) : 0); }
    realtime(now) { return this.phase === 'ready' ? 0 : Math.max(0, (this.phase === 'complete' ? this.endAt : now) - this.startAt); }
    start(now) { if (this.phase !== 'ready' || !this.count) return false; this.startAt = this.anchor = now; this.phase = 'running'; return true; }
    pause(now) { if (this.phase !== 'running') return false; this.active = this.elapsed(now); this.phase = 'paused'; return true; }
    resume(now) { if (this.phase !== 'paused') return false; this.anchor = now; this.phase = 'running'; return true; }
    split(now) {
      if (this.phase !== 'running' || this.next >= this.count) return false;
      const value = this.elapsed(now);
      if (this.next && value < this.splits[this.next - 1]) return false;
      this.splits[this.next++] = value;
      return true;
    }
    finish(now) {
      if (!['running','paused'].includes(this.phase)) return false;
      const value=this.elapsed(now);
      if(this.next && value<this.splits[this.next-1]) return false;
      this.active=value;this.endAt=now;this.phase='complete';return true;
    }
    undo() { if (!['running', 'paused'].includes(this.phase) || !this.next) return false; this.splits[--this.next] = null; return true; }
  }
  const format = ms => {
    if (ms == null) return '—';
    const s = Math.floor(ms / 1000), two = n => String(n).padStart(2, '0');
    return `${two(Math.floor(s / 3600))}:${two(Math.floor(s / 60) % 60)}:${two(s % 60)}.${two(Math.floor(ms % 1000 / 10))}`;
  };
  const key = r => JSON.stringify([r.title, r.category, r.names]);
  const api = {Timer, format, key};
  if (typeof module !== 'undefined' && module.exports) module.exports = api; else root.RunClock = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
