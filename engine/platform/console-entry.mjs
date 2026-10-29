// SPDX-License-Identifier: GPL-3.0-or-later
// Self-contained startup: own renderer, timing worker and loopback bridge.
import './runtime/globals.mjs';
import './runtime/url.mjs';
import {createHash,randomBytes} from './runtime/crypto.mjs';
import * as fs from './runtime/fs.mjs';
import {StateStore} from '../src/store.mjs';
import {FileStorage} from '../src/storage.mjs';
import {PmRuntime} from '../src/runtime.mjs';
import {OverlayRegistry} from '../src/overlays/registry.mjs';
import {OverlayHost} from '../src/overlays/host.mjs';
import {installConsoleDeck} from '../src/native-deck.mjs';
import {installGameLifecycle} from '../src/game-lifecycle.mjs';
import {installGameInput} from '../src/game-input.mjs';
import {installGameInputRelay} from '../src/game-input-relay.mjs';
import {serveRunClock} from '../src/runclock-server.mjs';
import {recoverRunClockStartup,startupFailure} from '../src/runclock-startup.mjs';
import seeds from '../build/overlay-seeds.json';
import brand from '../build/brand.json';
const ROOT='/data/RunClock/runtime';
globalThis.__ready=(async()=>{
 const id=()=>randomBytes(16).toString('hex');
 const store=await new StateStore(await new FileStorage(ROOT+'/state').init(),v=>createHash('sha256').update(v).digest('hex')).open();
 const runtime=await new PmRuntime(store,{
  operationLog:e=>console.log('RunClock operation: '+JSON.stringify(e)),
  identity:async()=>({mode:'console',ip:'127.0.0.1',firmware:__native.firmware(),hardwareVerified:false}),
  async install(r){
   r.recovery={pending:false,permits:node=>node==='native-deck',permitsJob:kind=>['deck.start','game.input.prepare'].includes(kind),snapshot:()=>null};
   r.graph.register({id:'coordinator',start:async()=>({session:r.session}),verify:e=>e.session===r.session});await r.graph.start('coordinator');
   const registry=new OverlayRegistry({fs,root:ROOT,seeds:[]});await registry.ensure();
   const seed=seeds.find(s=>s.id==='runclock');if(!seed)throw Error('Embedded timing worker missing');
   await fs.writeFile(ROOT+'/logo.png',Buffer.from(brand.logo,'base64'));
   const folder=ROOT+'/overlays/runclock';await fs.mkdir(folder,{recursive:true});
   // Refresh only app-owned runtime files. User route/history stay outside.
   for(const name of ['overlay.json','runclock.ovl']){
    const data=Buffer.from(seed.files[name].base64,'base64');
    await fs.writeFile(folder+'/'+name+'.tmp',data);await fs.rename(folder+'/'+name+'.tmp',folder+'/'+name);
   }
   await registry.scan();
   const host=new OverlayHost({registry,fs,native:__native,random:randomBytes,port:8891,notify:text=>__native.notify(text)});await host.start();
   r.overlays={registry,host,fs};
   installConsoleDeck(r,__native);
   installGameLifecycle(r,__native);installGameInput(r,__native,{automatic:true});installGameInputRelay(r,__native);
  }
 },{id}).start();
 await recoverRunClockStartup(runtime,__native);
 await serveRunClock(runtime);
 await runtime.graph.start('native-deck');runtime.nativeDeck.startMonitoring();
 await runtime.overlays.host.open(runtime.overlays.registry.get('runclock'));
 const until=Date.now()+20000;
 while(runtime.overlays.host.session('runclock')?.state!=='connected'){
  if(Date.now()>until)throw Error('Private timing worker did not connect');
  await new Promise(resolve=>setTimeout(resolve,100));
 }
 runtime.gameLifecycle.start();runtime.gameInput.start();runtime.gameInputRelay.start();
 __native.notify('RunClock v1.0.2-beta.3 by BrewSparrow ready. Hold L3 + R3 to open.');
 globalThis.__runclock=runtime;
})().catch(e=>{const failure=startupFailure(e);console.log(failure.log);__native.notify(failure.notification);__native.stop();});
