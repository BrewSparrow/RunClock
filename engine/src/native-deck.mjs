// SPDX-License-Identifier: GPL-3.0-or-later
import { ConsoleDeck } from './deck.mjs';
import { invariant, PmError } from './errors.mjs';
import { expiredStartup, sameExpiry } from './boot-recovery.mjs';
import { RESTART_REQUIRED } from './runclock-startup.mjs';
const sleep = ms => new Promise(resolve => setTimeout(resolve, ms));
// The menu no longer starts kstuff itself (users autoload their own copy), and
// autoloaders commonly start ProsperoMenu before kstuff. Injection requires
// kstuff, so the deck waits for it instead of failing once at boot.
export const KSTUFF_WAIT_MS = 120000;
export function installConsoleDeck(runtime, native, { delay = sleep, timeout = 30000, kstuffWait = KSTUFF_WAIT_MS, now = Date.now } = {}) {
  runtime.consoleDeck = new ConsoleDeck(runtime); runtime.deckCredential = runtime.id();
  const prior = runtime.jobs.snapshot().filter(j => j.kind === 'deck.start' &&
    !['queued', 'held'].includes(j.status) &&
    (j.status !== 'cancelled' || j.error?.code === 'RESERVATION_RELEASED')).at(-1);
  let request = prior?.id || null, monitor = null, misses = 0;
  function enable(ready) {
    for (const feature of ['deck', 'ip-overlay']) runtime.capability(feature, ready,
      ready ? 'Native ShellUI renderer heartbeat verified.' : 'Native renderer is not ready. Use the phone interface.');
    // The separate game-input relay owns game-overlay readiness. ShellUI's
    // heartbeat alone cannot establish game-process input isolation.
  }
  enable(false);
  function ownerState(job,status){
    if(status?.ownerState)return status.ownerState;
    if(job?.args?.pid&&job.args.generation&&typeof native.deckOwnerState==='function')
      return native.deckOwnerState(job.args.pid,job.args.generation);
    return null;
  }
  function inspect(requestId = request) {
    if (!requestId) return { absent: true, ready: false };
    const status = native.deckPoll(requestId), job = runtime.jobs.snapshot().find(j => j.id === requestId);
    const owner=ownerState(job,status);
    if (owner === 'ended') return { ...status, absent: true, ready: false, processEnded: true };
    if (job && job.session !== runtime.session && !owner && expiredStartup(native, job, status)) return { absent: true, ready: false, expired: true };
    // Ordinary failed jobs were verified to have no side effects. Their native
    // receipt may have been superseded by a later clean retry. Native start()
    // independently checks the durable receipt before any new injection.
    if (!status && job?.status === 'failed') return { absent: true, ready: false };
    if (job && job.session !== runtime.session && !(status?.stage === 4 && status.noSideEffects))
      return { ...status, absent: false, ready: false, restartRequired: true, reason: RESTART_REQUIRED };
    return status ? { ...status, absent: status.stage === 4 && status.noSideEffects } : { ready: false, absent: true };
  }
  function target(input) {
    const state = native.systemProbe();
    invariant(!state.busy && state.kstuff?.state === 'installed' && state.kstuff.injectionReady,
      'DECK_DEPENDENCY', 'Verified kstuff must be ready before the native deck starts.');
    const shells = state.processes.filter(p => p.name === 'SceShellUI');
    invariant(shells.length === 1 && shells[0].generation, 'SHELLUI_IDENTITY', 'ShellUI process identity could not be verified.');
    // A process name does not establish ownership of our hook sites.
    // The native preparer checks their actual instructions before publication.
    if (input?.generation) invariant(input.pid === shells[0].pid && input.generation === shells[0].generation,
      'SHELLUI_CHANGED', 'ShellUI restarted after the native deck was requested.');
    return { pid: shells[0].pid, generation: shells[0].generation, ...(state.boot ? { boot: state.boot } : {}) };
  }
  runtime.jobs.register('deck.start', {
    validate: target, resources: () => ['native-payload-dispatch', 'shellui-hooks'], timeout: timeout + 30000, cancellable: false,
    async run(input, context) {
      try { target(input); } catch (error) { error.noSideEffects = true; throw error; }
      request = context.id;
      try { native.deckStart(request, input.pid, input.generation, runtime.deckCredential); }
      catch (error) {
        // The native binding creates a plain JS Error, not a PmError. Preserve
        // its known check/errno instead of replacing it with OPERATION_FAILED.
        if (['DECK_NATIVE', 'DECK_ALREADY_ATTEMPTED', 'NATIVE_BUSY'].includes(error?.code)) {
          console.log('ProsperoMenu deck startup: ' + error.code + ' ' + error.message);
          throw new PmError(error.code, error.message, { action: 'Review Activity before retrying. Native deck startup was not confirmed.' });
        }
        console.log('ProsperoMenu deck startup exception: ' + String(error?.stack || error?.message || error).slice(0, 220));
        throw error;
      }
      const deadline = Date.now() + timeout;
      while (Date.now() < deadline && !context.signal.aborted) {
        const status = inspect();
        context.evidence?.(status);
        invariant(status.request === request && status.pid === input.pid && status.generation === input.generation,
          'DECK_RECEIPT', 'Native deck receipt does not match this ShellUI process.');
        if (status.ready) return { completed: true, evidence: status };
        if (status.stage === 4 && status.noSideEffects) throw Object.assign(new PmError('DECK_REJECTED', status.message), { noSideEffects: true });
        invariant(status.stage !== 5, 'DECK_UNCERTAIN', status.message || 'Deck startup needs inspection before retrying.');
        await context.progress({ completed: 0, total: 1, phase: status.message || 'Waiting for native renderer' });
        await delay(250);
      }
      throw new PmError('DECK_NOT_READY', 'The native deck did not produce a fresh render heartbeat. Use the phone interface; no reinjection was issued.');
    },
    verify(result, input) { const status = inspect(); return status.ready && status.pid === input.pid && status.generation === input.generation && result.evidence.request === status.request; },
    reconcile(job) {
      const receipt = native.deckPoll(job.id), owner=ownerState(job,receipt);
      console.log('RunClock deck recovery: ' + JSON.stringify({request:job.id.slice(0,12),job:job.status,stage:receipt?.stage??null,owner:owner||'unavailable',boot:receipt?.boot||job.args?.boot||null,pid:receipt?.pid||job.args?.pid||null,generation:receipt?.generation||job.args?.generation||null}));
      if (owner === 'ended') return { absent: true, message: 'The recorded ShellUI process ended. Its resident hooks no longer exist.' };
      const expired = !owner && expiredStartup(native, job, receipt);if (expired) return expired;
      const status = inspect(job.id); if (status.absent) return { absent: true };
      return status.ready && status.pid === job.args.pid && status.generation === job.args.generation ? { completed: true, evidence: status } : null;
    },
    verifyExpired: (outcome, job) => sameExpiry(outcome, expiredStartup(native, job, native.deckPoll(job.id)))
  });
  // kstuff is an injection precondition, checked by target() and the native
  // worker. Its monitoring state must not invalidate a healthy resident menu.
  function kstuffReady() {
    try { const state = native.systemProbe();return !state.busy && state.kstuff?.state === 'installed' && state.kstuff.injectionReady === true; }
    catch { return false; }
  }
  runtime.graph.register({ id: 'native-deck', dependencies: ['coordinator'], optional: true, timeout: timeout + 55000 + kstuffWait,
    inspect, verify: evidence => evidence?.request === inspect().request && inspect().ready,
    async start({ signal }) {
      // stdlib console.error goes to stderr, which never reaches runtime.log, so
      // a deck that fails to start left no trace at all. Every branch below is
      // logged to stdout on purpose.
      console.log('ProsperoMenu deck: node start requested');
      const current = inspect(); if (current.ready) { enable(true); return current; }
      console.log('ProsperoMenu deck: current state ' + JSON.stringify({ absent: !!current.absent, ready: !!current.ready, reason: current.reason || null, stage: current.stage ?? null }));
      invariant(current.absent, current.restartRequired ? 'DECK_RESTART_REQUIRED' : 'DECK_UNCERTAIN',
        current.reason || 'The previous native deck attempt needs inspection.');
      if (!kstuffReady()) {
        console.log('ProsperoMenu deck: waiting for kstuff from the autoloader');
        const until = now() + kstuffWait;
        while (!signal.aborted && now() < until && !kstuffReady()) await delay(500);
        console.log('ProsperoMenu deck: kstuff ' + (kstuffReady() ? 'ready' : 'still not ready; trying anyway'));
      }
      // While the autoloader is still injecting other payloads, one process
      // census can miss ShellUI's identity (seen on 13.60 the moment kstuff
      // became ready). That check runs before anything is injected, so retry it.
      for (const until = now() + 20000; !signal.aborted; await delay(500)) {
        try { target();break; }
        catch (error) {
          if (!['SHELLUI_IDENTITY', 'DECK_DEPENDENCY'].includes(error.code) || now() >= until) break;
          console.log('ProsperoMenu deck: ' + error.code + ' before injection; checking again');
        }
      }
      // Exactly one native preparation per launch. Even a rejection before
      // hooks can retain SDK/Mono state; another injected image is not a
      // readiness probe. Pre-injection target checks above remain retryable.
      {
        const job = await runtime.jobs.enqueue('deck.start', {}, 'deck_' + (runtime.recovery?.attempt || runtime.session));
        console.log('ProsperoMenu deck: job enqueued ' + job.id.slice(0, 12));
        let ended = null;
        while (!signal.aborted) {
          const state = runtime.jobs.snapshot().find(j => j.id === job.id);
          if (state.status === 'completed') { enable(true); return state.result.evidence; }
          if (['failed', 'interrupted', 'cancelled', 'held', 'uncertain'].includes(state.status)) { ended = state; break; }
          await delay(250);
        }
        if (!ended) throw new PmError('DECK_WAIT_EXPIRED', 'Deck startup remains recorded in Activity.');
        console.log('ProsperoMenu deck: job ended ' + ended.status + ' ' + (ended.error?.code || '') + ' ' + (ended.error?.message || ''));
        for (const feature of ['deck', 'ip-overlay']) runtime.capability(feature, false, ended.error?.message || 'Native renderer startup failed. Review Activity.');
        throw new PmError(ended.error?.code || 'DECK_STOPPED', ended.error?.message || 'Native deck startup needs inspection.');
      }
    }
  });
  async function check() {
    if (!request || runtime.jobs.running.size) return;
    const status = inspect();
    if (status.busy) return;
    const wasReady = runtime.capabilities.get('deck').available;
    // One missed heartbeat is not a lost renderer. Dropping the capability
    // cleared the in-game input target and closed an open menu; require two
    // consecutive misses (about 4 s) before disabling a working deck.
    if (status.ready) misses = 0;
    else if (wasReady && ++misses < 2) return;
    if (wasReady !== Boolean(status.ready)) enable(Boolean(status.ready));
    // A temporarily absent scene can recover naturally; never publish the
    // hooks again. The same injection's new render heartbeat re-enables it.
  }
  return runtime.nativeDeck = {
    inspect, check,
    startMonitoring() { if (!monitor) monitor = setInterval(() => { try { check().catch(() => enable(false)); } catch { enable(false); } }, 2000); },
    stopMonitoring() { clearInterval(monitor); monitor = null; }
  };
}
