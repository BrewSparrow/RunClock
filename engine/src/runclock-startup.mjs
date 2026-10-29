// SPDX-License-Identifier: GPL-3.0-or-later
import { PmError } from './errors.mjs';
import { waitForNativeProbe } from './native-readiness.mjs';

export const RESTART_REQUIRED = 'Restart the PS5, then load RunClock once through Payload Manager. An earlier instance may still own menu hooks in this boot.';

// These are the only native operations installed by standalone RunClock.
// Reconcile their durable reservations before enqueueing a new renderer. A
// reboot destroys the hooks, but JobQueue.recover() deliberately retains the
// reservations until the native adapter proves that their boot/process ended.
export async function recoverRunClockStartup(runtime, native, {
  delay = ms => new Promise(resolve => setTimeout(resolve, ms)),
  log = message => console.log(message)
} = {}) {
  const startup = job => ['deck.start', 'game.input.prepare'].includes(job.kind) && job.session !== runtime.session;
  const prior = runtime.jobs.snapshot().filter(startup);
  if (!prior.length) return;
  const probe=await waitForNativeProbe(native, { delay });
  log('RunClock startup environment: '+JSON.stringify({boot:probe.boot||null,kstuff:probe.kstuff||null,shellui:probe.processes?.filter(p=>p.name==='SceShellUI').map(p=>({pid:p.pid,generation:p.generation}))||[]}));
  for (const job of prior) {
    if (job.status === 'held' && job.error?.code === 'RESTART_HELD') {
      // A queued intent has not been dispatched. Do not replay it using its old
      // ShellUI/game identity, and do not let it mask the last actual attempt.
      await runtime.jobs.cancel(job.id);
      log('RunClock startup recovery: cancelled an undispatched ' + job.kind + ' intent');
    } else if (['interrupted', 'uncertain'].includes(job.status)) {
      try {
        await runtime.jobs.reconcile(job.id);
        const settled = runtime.jobs.snapshot().find(item => item.id === job.id);
        log('RunClock startup recovery: ' + job.kind + ' ' + (settled?.error?.code || settled?.status || 'checked'));
      } catch (error) {
        if (error.code !== 'OUTCOME_UNKNOWN') throw error;
        // An unavailable/busy probe is not proof of a reboot. Retain ownership
        // and give the caller the actionable restart message below.
        log('RunClock startup recovery: unresolved ' + job.kind + ' from an earlier coordinator');
        if(job.kind==='deck.start'){
          const status=native.deckPoll(job.id);
          log('RunClock unresolved owner: '+JSON.stringify({stage:status?.stage??null,owner:status?.ownerState||'unavailable',boot:status?.boot||job.args?.boot||null,pid:status?.pid||job.args?.pid||null}));
        }
      }
    }
  }
  const blocked = runtime.jobs.snapshot().find(job => startup(job) &&
    ['interrupted', 'uncertain'].includes(job.status) && job.resources?.includes('native-payload-dispatch'));
  if (blocked) throw new PmError('DECK_RESTART_REQUIRED', RESTART_REQUIRED);
}

export function startupFailure(error) {
  const code = typeof error?.code === 'string' ? error.code : 'STARTUP_FAILED';
  const message = typeof error?.message === 'string' && error.message ? error.message : String(error);
  const restart = ['DECK_RESTART_REQUIRED', 'DECK_ALREADY_ATTEMPTED'].includes(code);
  return {
    log: 'RunClock startup failed [' + code + ']: ' + message +
      (error?.stack ? '\n' + String(error.stack) : ''),
    notification: restart
      ? 'RunClock: restart the PS5, then load RunClock once. Earlier menu hooks may still be active.'
      : 'RunClock could not start [' + code + ']. ' + message.slice(0, 160) + ' See /data/RunClock/runtime/runtime.log.'
  };
}
