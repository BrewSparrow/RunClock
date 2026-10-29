// SPDX-License-Identifier: GPL-3.0-or-later
// Exercise the real state store, job queue, native adapters and service graph.
// Only PS5 probes/injection are substituted; this is not ShellUI emulation.
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { StateStore } from '../engine/src/store.mjs';
import { PmRuntime } from '../engine/src/runtime.mjs';
import { installConsoleDeck } from '../engine/src/native-deck.mjs';
import { installGameInput } from '../engine/src/game-input.mjs';
import { recoverRunClockStartup, startupFailure } from '../engine/src/runclock-startup.mjs';

const previousBoot = '1000:0', boot = '2000:0', session = 'f'.repeat(32);
let serial = 0;
const delay = ms => new Promise(resolve => setTimeout(resolve, Math.min(ms, 2)));
const oldJob = (id, { kind = 'deck.start', status = 'running', oldBoot = previousBoot } = {}) => ({
  id: id.repeat(32), session, kind, status, key: 'old_' + id.repeat(8),
  args: { pid: 123, generation: oldBoot + '/123/10:0', boot: oldBoot },
  signature: '{}', requestSignature: '{}', resources: ['native-payload-dispatch', 'shellui-hooks'],
  result: null, error: null, progress: null, createdAt: 1, updatedAt: 1
});
async function fixture(jobs, { oldReceipt = null, probeBusy = 0, probeBoot = boot, probeStart = 20, ownerOverride = null, rejectOnStart = null } = {}) {
  const files = new Map();
  const store = await new StateStore({
    read: async name => files.get(name) ?? null,
    replace: async (name, data) => { files.set(name, data); }
  }, text => createHash('sha256').update(text).digest('hex')).open();
  await store.transaction(state => { state.jobs = structuredClone(jobs); });
  let starts = 0, receipt = oldReceipt, probes = 0;
  const ownerState=(pid,generation)=>ownerOverride||(!probeBoot?'unknown':pid===123&&generation===probeBoot+'/123/'+probeStart+':0'?'current':'ended');
  const native = {
    systemProbe() {
      if (++probes <= probeBusy) return { busy: true };
      return { boot: probeBoot, kstuff: { state: 'installed', injectionReady: true },
        processes: [{ name: 'SceShellUI', pid: 123, generation: probeBoot + '/123/'+probeStart+':0' }] };
    },
    deckOwnerState: ownerState,
    deckPoll: request => receipt?.request === request ? {...structuredClone(receipt),ownerState:ownerState(receipt.pid,receipt.generation)} : null,
    deckStart(request, pid, generation) {
      starts++;
      receipt = { request, pid, generation, boot: probeBoot, stage: 3, ready: true, noSideEffects: false };
      if(rejectOnStart)receipt={...receipt,stage:rejectOnStart==='clean'?4:5,ready:false,noSideEffects:rejectOnStart==='clean',message:'Preparation rejected; resident startup stopped.'};
    },
    gameInputPoll: () => null,
    gameInputStart: () => { throw Error('An old input operation was replayed'); }
  };
  const runtime = await new PmRuntime(store, {
    identity: async () => ({ mode: 'console', firmware: '13.60' }),
    async install(r) {
      r.graph.register({ id: 'coordinator', start: async () => ({ session: r.session }), verify: e => e.session === r.session });
      await r.graph.start('coordinator');
      installConsoleDeck(r, native, { delay, timeout: 500, kstuffWait: 0 });
      installGameInput(r, native, { delay });
    }
  }, { id: () => (++serial).toString(16).padStart(32, '0') }).start();
  return { runtime, native, store, starts: () => starts };
}
const recover = f => recoverRunClockStartup(f.runtime, f.native, { delay, log() {} });
const start = f => f.runtime.graph.start('native-deck');
const receiptFor = (job, extra = {}) => ({ request: job.id, pid: job.args.pid, generation: job.args.generation,
  boot: job.args.boot, stage: 3, ready: false, noSideEffects: false, ...extra });

// The former implementation could enqueue a new job but never dispatch it:
// both interrupted records kept native-payload-dispatch reserved after reboot.
{
  const deck = oldJob('a'), game = oldJob('b', { kind: 'game.input.prepare' });
  const queued = oldJob('c', { status: 'queued' });
  const f = await fixture([deck, game, queued], { oldReceipt: receiptFor(deck) });
  assert.equal(f.runtime.jobs.snapshot()[2].status, 'held');
  await recover(f); await start(f);
  const saved = f.runtime.jobs.snapshot();
  assert.equal(saved[0].error.code, 'NOT_APPLIED');
  assert.equal(saved[1].result.phase, 'game-ended');
  assert.equal(saved[2].status, 'cancelled');
  assert.equal(saved.at(-1).status, 'completed');
  assert.equal(f.starts(), 1); assert.equal(f.runtime.capabilities.get('deck').available, true);
  // A second coordinator in that same boot must still refuse reinjection.
  const fresh = saved.at(-1);
  const again = await fixture(saved, { oldReceipt: receiptFor(fresh, { ready: true }) });
  await recover(again);
  await assert.rejects(() => start(again), e => e.code === 'DECK_RESTART_REQUIRED' && /Restart the PS5/.test(e.message));
  assert.equal(again.starts(), 0);
}
// A completed previous-boot job needs no reconciliation but must not block.
{
  const job = oldJob('a', { status: 'completed' });
  const f = await fixture([job], { oldReceipt: receiptFor(job) });
  await recover(f); await start(f); assert.equal(f.starts(), 1);
}
// An abandoned intent/explicit clean rejection is recoverable in the same boot.
{
  const job = oldJob('a', { oldBoot: boot });
  const f = await fixture([job], { oldReceipt: receiptFor(job, { stage: 4, noSideEffects: true }) });
  await recover(f); await start(f); assert.equal(f.starts(), 1);
  assert.equal(f.runtime.jobs.snapshot()[0].error.code, 'NOT_APPLIED');
}
// Clean failed jobs whose receipts were superseded are not hook ownership.
{
  const job = oldJob('a', { status: 'failed', oldBoot: boot });
  const f = await fixture([job]); await recover(f); await start(f); assert.equal(f.starts(), 1);
}
// Unknown same-boot outcomes and missing boot evidence retain their reservations.
for (const probeBoot of [boot, '']) {
  const job = oldJob('a', { oldBoot: boot });
  const f = await fixture([job], { oldReceipt: receiptFor(job, { stage: 2 }), probeBoot, probeStart:10 });
  await assert.rejects(() => recover(f), e => e.code === 'DECK_RESTART_REQUIRED');
  assert.equal(f.starts(), 0); assert.equal(f.runtime.jobs.snapshot()[0].status, 'interrupted');
}
// Transient contention must be waited out before determining boot ownership.
// A restarted ShellUI can have the same boot timestamp. It still must release
// the old reservation, including a job whose native receipt was superseded.
for (const missingReceipt of [false,true]) {
  const job=oldJob('a',{oldBoot:boot});
  const f=await fixture([job],{oldReceipt:missingReceipt?null:receiptFor(job),probeStart:20});
  await recover(f);await start(f);assert.equal(f.starts(),1);
  assert.equal(f.runtime.jobs.snapshot()[0].error.code,'NOT_APPLIED');
}
// A failed native process query must retain ownership even if the saved boot
// differs. The authoritative native probe says its outcome is unknown.
{
  const job=oldJob('a'),f=await fixture([job],{oldReceipt:receiptFor(job),ownerOverride:'unknown'});
  await assert.rejects(()=>recover(f),e=>e.code==='DECK_RESTART_REQUIRED');
  assert.equal(f.starts(),0);
}
// Transient contention must be waited out before determining boot ownership.
{
  const job = oldJob('a'), f = await fixture([job], { oldReceipt: receiptFor(job), probeBusy: 2 });
  await recover(f); await start(f); assert.equal(f.starts(), 1);
}
{
  const job = oldJob('a'), f = await fixture([job], { oldReceipt: receiptFor(job), probeBusy: 1000 });
  await assert.rejects(() => recover(f), e => e.code === 'NATIVE_BUSY');
  assert.equal(f.starts(), 0); assert.equal(f.runtime.jobs.snapshot()[0].status, 'interrupted');
}
// Recovery is restricted to RunClock's startup jobs, never file/save operations.
{
  const job = oldJob('a', { kind: 'save.extract' }), f = await fixture([job]);
  await recover(f); assert.equal(f.runtime.jobs.snapshot()[0].status, 'interrupted');
  assert.equal(f.starts(), 0);
}
// QuickJS stack strings can omit the error message. Always record code/message.
{
  const report = startupFailure({ code: 'DECK_RESTART_REQUIRED', message: 'Restart the PS5', stack: 'at start (runtime.js:1)' });
  assert.match(report.log, /\[DECK_RESTART_REQUIRED\]: Restart the PS5/);
  assert.match(report.notification, /restart the PS5/);
  assert.match(startupFailure(new Error('Worker missing')).notification, /Worker missing/);
}
// A native preparation is issued only once. Even an explicit clean rejection
// must not cause another image to start while its prior SDK state may remain.
for(const rejectOnStart of ['clean','retained']){
 const f=await fixture([],{rejectOnStart});await recover(f);
 await assert.rejects(()=>start(f),e=>e.code===(rejectOnStart==='clean'?'DECK_REJECTED':'DECK_UNCERTAIN'));
 assert.equal(f.starts(),1);
 assert.equal(f.runtime.jobs.snapshot().filter(j=>j.kind==='deck.start').length,1);
}
console.log('PASS: boot/process recovery, single native preparation on rejection, same-boot protection, contention and actionable startup diagnostics.');
