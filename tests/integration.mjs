// SPDX-License-Identifier: GPL-3.0-or-later
// Real standalone UI protocol plus the C timing/storage worker over loopback.
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';import os from 'node:os';import path from 'node:path';
import {randomBytes} from 'node:crypto';import {spawn} from 'node:child_process';import {fileURLToPath} from 'node:url';
import {ConsoleDeck,encodeDeck} from '../engine/src/deck.mjs';
import {serveRunClock} from '../engine/src/runclock-server.mjs';
import {OverlayHost} from '../engine/src/overlays/host.mjs';import {OverlayRegistry} from '../engine/src/overlays/registry.mjs';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const dir=await fs.mkdtemp(path.join(os.tmpdir(),'runclock-standalone-'));
const sleep=ms=>new Promise(r=>setTimeout(r,ms));const semantic=id=>String(id||'').replace(/^p\d+:/,'');
let proc,host,deck,pump,server,stderr='';
async function until(fn,why){const end=Date.now()+5000;while(Date.now()<end){if(fn())return;await sleep(10);}throw Error(why+'\n'+stderr);}
try{
 const folder=path.join(dir,'overlays/runclock');await fs.mkdir(folder,{recursive:true});
 await fs.copyFile(path.join(root,'app/native/overlay.json'),path.join(folder,'overlay.json'));await fs.writeFile(path.join(folder,'runclock.ovl'),'host-fixture');
 const registry=new OverlayRegistry({fs,root:dir});await registry.ensure();await registry.scan();host=new OverlayHost({registry,fs,random:randomBytes,port:0});await host.start();await host.open(registry.get('runclock'));
 const runtime={overlays:{registry,host},id:()=>randomBytes(16).toString('hex'),deckCredential:randomBytes(16).toString('hex')};
 deck=new ConsoleDeck(runtime);runtime.consoleDeck=deck;deck.setVisible(true);server=await serveRunClock(runtime,{port:0});
 const bridge='http://127.0.0.1:'+server.address().port+'/native/deck/sync';
 const sync=(body,credential=runtime.deckCredential)=>fetch(bridge,{method:'POST',headers:{'Content-Type':'application/json','X-RunClock-Native':credential},body:JSON.stringify(body)});
 assert.equal((await fetch(bridge)).status,403);assert.equal((await sync({},'0'.repeat(32))).status,403);
 assert.equal((await sync({padding:'x'.repeat(5000)})).status,400);
 proc=spawn(path.join(root,'app/build/runclock-host'),[],{env:{...process.env,PMO_ROOT:dir,RUNCLOCK_DATA_DIR:path.join(dir,'records')},stdio:['ignore','ignore','pipe']});proc.stderr.on('data',d=>stderr+=d.toString());
 const session=()=>host.session('runclock');const item=id=>session()?.page?.items.find(x=>semantic(x.id)===id);
 await until(()=>item('state'),'initial worker page');await sleep(100);pump=setInterval(()=>deck.frame(),20);
 const inputFor=id=>{const f=deck.frame(),row=f.rows.findIndex(r=>semantic(r.args?.id)===id);assert(row>=0,'Missing '+id);assert(f.rows[row].enabled,'Disabled '+id);return {kind:'select',sequence:f.sequence,row,request:randomBytes(16).toString('hex')};};
 async function select(id,delay=350){const input=inputFor(id),page=session().page;await sleep(delay);await deck.action(input);await until(()=>session().page!==page,'response to '+id);}
 async function back(){const f=deck.frame(),page=session().page;await sleep(350);await deck.action({kind:'back',sequence:f.sequence,row:0,request:randomBytes(16).toString('hex')});await until(()=>session().page!==page,'back response');}
 const stale=input=>assert.rejects(()=>deck.action(input),e=>e.code==='DECK_STALE');
 assert.equal(session().page.items.filter(i=>/^split\d+$/.test(i.id)).length,10);assert(!item('refresh'));
 const initial=inputFor('primary');await select('primary');assert.equal(item('state').value,'RUNNING');await stale(initial);
 const start=deck.frame(),clock=item('clock').value,rta=item('rta').value;
 const pause=inputFor('pause');await sleep(700);
 assert.notEqual(item('clock').value,clock,'Active clock does not tick live');assert.notEqual(item('rta').value,rta,'RTA does not tick live');
 const tick=deck.frame();assert.equal(tick.sequence,start.sequence,'Clock redraw invalidated controls');assert.notEqual(tick.metricsSequence,start.metricsSequence,'No separate redraw revision');
 const out=path.join(root,'app/build/live-wire');await fs.mkdir(out,{recursive:true});await fs.writeFile(path.join(out,'before.bin'),encodeDeck(start));await fs.writeFile(path.join(out,'after.bin'),encodeDeck(tick));await fs.writeFile(path.join(out,'pause-row.txt'),String(pause.row));
 const response=await sync({menuOpen:true,action:pause,feedback:{}});assert.equal(response.status,200);const wire=Buffer.from(await response.arrayBuffer());assert.equal(wire.length,19024);assert.equal(wire.readUInt32LE(0),0x54414344);
 await until(()=>item('state').value==='PAUSED','delayed Pause');
 const frozen=item('clock').value,pausedRta=item('rta').value;await sleep(400);assert.equal(item('clock').value,frozen);assert.notEqual(item('rta').value,pausedRta);
 await select('primary',900);assert.equal(item('state').value,'RUNNING');
 const once=inputFor('primary'),page=session().page;await Promise.all([deck.action(once),deck.action(once)]);await until(()=>session().page!==page,'deduplicated Split');assert.equal(item('split1').value,'--','Duplicate input split twice');
 await select('undo');assert.equal(item('split0').value,'--');await select('primary');
 let old=inputFor('finish');item('clock').label='Changed label';deck.frame();await stale(old);item('clock').label='Active time';deck.frame();
 old=inputFor('finish');const token=session().token;session().token='changed-session';deck.frame();await stale(old);session().token=token;deck.frame();
 const finish=inputFor('finish');await select('finish');await stale(finish);assert.equal(item('state').value,'COMPLETE');
 const final=item('clock').value,finalRta=item('rta').value;await sleep(350);assert.equal(item('clock').value,final);assert.equal(item('rta').value,finalRta);
 await select('records');assert.equal(item('finished').value,'1');assert.equal(item('pb').value,final);await select('record0');assert.equal(item('saved-active').value,final);assert.equal(item('saved1').value,'--');
 const oldDelete=inputFor('delete');assert.equal(deck.frame().rows[oldDelete.row].triangle,true);await fs.writeFile(path.join(out,'delete.bin'),encodeDeck(deck.frame()));
 await select('delete');await stale(oldDelete);const oldConfirm=inputFor('delete-confirm');assert.equal(deck.frame().rows[oldConfirm.row].triangle,true);assert.equal(semantic(deck.frame().rows[0].args.id),'cancel-delete');await fs.writeFile(path.join(out,'confirm.bin'),encodeDeck(deck.frame()));
 await back();await stale(oldConfirm);const historyFile=path.join(dir,'records',(await fs.readdir(path.join(dir,'records'))).find(x=>/^[a-f0-9]{16}\.jsonl$/.test(x)));const beforeDelete=await fs.readFile(historyFile);
 await fs.mkdir(historyFile+'.delete.tmp');await select('delete');await select('delete-confirm');assert.match(item('records-message').detail,/Could not delete/);assert.equal(item('pb').value,final);assert.deepEqual(await fs.readFile(historyFile),beforeDelete);await fs.rmdir(historyFile+'.delete.tmp');
 await select('record0');await select('delete');await select('delete-confirm');assert.equal(item('finished').value,'0');assert.equal(item('pb').value,'--');
 await back();await select('reset');assert.equal(item('clock').value,'00:00:00.00');assert.equal(item('rta').value,'00:00:00.00');assert.equal(item('state').value,'READY');
 await select('primary');for(let i=0;i<10;i++)await select('primary',20);assert.equal(item('state').value,'RUNNING');assert.equal(item('primary').enabled,false);await select('pause');await select('finish');assert.equal(item('state').value,'COMPLETE');
 await select('reset');await select('primary');const oldReset=inputFor('reset');await select('reset');await stale(oldReset);await select('reset');assert.equal(item('state').value,'READY');assert.equal(item('rta').value,'00:00:00.00');
 await select('primary');deck.setVisible(false);await sleep(300);deck.setVisible(true);await until(()=>item('clock').value!=='00:00:00.00','reopen after hidden timing');assert.equal(deck.frame().canBack,false,'Main Circle should close locally');await select('pause');
 const history=path.join(dir,'records',(await fs.readdir(path.join(dir,'records'))).find(x=>x.endsWith('.jsonl')));const rows=(await fs.readFile(history,'utf8')).trim().split('\n').map(JSON.parse);assert.equal(rows.filter(x=>x.event==='complete').length,1);assert.equal(rows.find(x=>x.event==='complete').splits.length,10);
 console.log('PASS: standalone live active/RTA clocks, delayed controls across redraws, pause/resume, ten splits, independent Finish, deduplication, stale targets/session/confirmation protection, reset, PB/deletion and hide/reopen.');
}finally{
 clearInterval(pump);if(host&&proc){host.stop('runclock');await Promise.race([new Promise(r=>proc.once('exit',r)),sleep(2000)]);if(proc.exitCode===null)proc.kill();}host?.close();await new Promise(r=>server?server.close(r):r());await fs.rm(dir,{recursive:true,force:true});
}
