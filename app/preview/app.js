/* SPDX-License-Identifier: GPL-3.0-or-later */
(() => {
  'use strict';
  const $ = id => document.getElementById(id), {Timer, format, key} = RunClock;
  const STORE = 'runclock.desktop.v1', MAX_RECORDS = 5000;
  const initial = () => ({version:1, route:{title:'My first game',category:'Any% practice',names:Array.from({length:10},(_,i)=>`Split ${i+1}`)}, records:[], pending:null, contrast:false});
  let db = initial(), canSave = true, nextImport = null;
  function validRoute(r) { return r && typeof r.title === 'string' && r.title.trim() && r.title.length <= 80 && typeof r.category === 'string' && r.category.trim() && r.category.length <= 80 && Array.isArray(r.names) && r.names.length > 0 && r.names.length <= 32 && r.names.every(n => typeof n === 'string' && n.trim() && n.length <= 80); }
  const safeTime = n => Number.isSafeInteger(n) && n >= 0;
  function validRecord(r) { return r && typeof r.routeKey === 'string' && r.routeKey.length <= 4000 && ['complete','abandoned','interrupted'].includes(r.status) && typeof r.date === 'string' && !isNaN(Date.parse(r.date)) && safeTime(r.active) && safeTime(r.rta) && r.rta >= r.active && Array.isArray(r.splits) && r.splits.length <= 32 && r.splits.every((s,i) => safeTime(s) && (i === 0 || s >= r.splits[i-1])) && r.splits.every(s=>s<=r.active); }
  function validDB(v) { return v && v.version === 1 && validRoute(v.route) && Array.isArray(v.records) && v.records.length <= MAX_RECORDS && v.records.every(validRecord) && (v.pending === null || validRecord(v.pending)); }
  try {
    const raw = localStorage.getItem(STORE);
    if (raw) { const parsed = JSON.parse(raw); if (!validDB(parsed)) throw Error('Invalid saved data'); db = parsed; }
  } catch (e) { canSave = false; $('save-status').textContent = 'Local saving unavailable or damaged · export a backup'; }
  if (db.pending) { db.records.unshift({...db.pending,status:'interrupted'}); db.records=db.records.slice(0,MAX_RECORDS); db.pending = null; }
  let timer = new Timer(db.route.names.length), savedComplete = false, toastTimeout;
  function notify(text) { $('toast').textContent = text; $('toast').classList.add('show'); clearTimeout(toastTimeout); toastTimeout = setTimeout(()=>$('toast').classList.remove('show'), 3500); }
  function save() {
    if (!canSave) return false;
    try { localStorage.setItem(STORE, JSON.stringify(db)); return true; }
    catch(e) { canSave = false; $('save-status').textContent = 'Local saving unavailable · export a backup'; notify('Saving failed. Export a backup to keep your records.'); return false; }
  }
  function record(status, now = performance.now()) { return {routeKey:key(db.route), date:new Date().toISOString(), status, active:Math.floor(timer.elapsed(now)), rta:Math.floor(timer.realtime(now)), splits:timer.splits.slice(0,timer.next).map(Math.floor)}; }
  function checkpoint() { db.pending = ['running','paused'].includes(timer.phase) ? record('interrupted') : null; save(); }
  function addRecord(status) { db.records.unshift(record(status)); db.records = db.records.slice(0, MAX_RECORDS); db.pending = null; save(); }
  function routeRecords() { return db.records.filter(r=>r.routeKey===key(db.route)); }
  function personalBest() { return routeRecords().filter(r=>r.status==='complete').reduce((a,r)=>!a||r.active<a.active?r:a,null); }
  function render() {
    const pb = personalBest(), phase = timer.phase, running = phase==='running', paused=phase==='paused', complete=phase==='complete';
    $('game-title').textContent = db.route.title; $('category').textContent = db.route.category;
    $('phase').textContent = phase.toUpperCase(); $('phase').className = 'phase '+phase;
    $('primary-label').textContent = phase==='ready'?'Start run':paused?'Resume':complete?'New attempt':timer.next>=timer.count?'All splits recorded':'Split';
    $('primary').disabled=running&&timer.next>=timer.count; $('finish').disabled=!(running||paused);
    $('pause-label').textContent = paused?'Resume':'Pause'; $('pause').disabled = !(running||paused);
    $('undo').disabled = !(running||paused)||!timer.next; $('reset').disabled = false;
    $('edit-route').disabled = running||paused;
    $('split-count').textContent = String(timer.count).padStart(2,'0');
    $('next-name').textContent = complete ? 'Route complete. Take a breath. Go again.' : timer.next>=timer.count ? 'All splits recorded. Select Finish run.' : `Next · ${db.route.names[timer.next]}`;
    $('progress-fill').style.width = `${timer.next/timer.count*100}%`;
    const list = $('splits'); list.replaceChildren();
    db.route.names.forEach((name,i)=>{
      const li=document.createElement('li'); li.className='split'+(i===timer.next&&!complete?' active':'')+(i<timer.next?' done':'');
      const label=document.createElement('div'); label.className='split-name';
      const number=document.createElement('span'); number.className='split-number'; number.textContent=i<timer.next?'✓':String(i+1).padStart(2,'0');
      const text=document.createElement('strong'); text.textContent=name; label.append(number,text);
      const times=document.createElement('div'); times.className='split-times'; times.textContent=format(timer.splits[i]);
      const best=document.createElement('small'); best.textContent='PB '+format(pb?.splits[i]??null); times.append(best); li.append(label,times); list.append(li);
    });
    const rr=routeRecords(); $('pb').textContent=pb?format(pb.active):'No completed runs yet';
    $('attempts').textContent=String(rr.length+(['running','paused'].includes(phase)?1:0));
    $('completed').textContent=String(rr.filter(r=>r.status==='complete').length);
    document.body.classList.toggle('high-contrast',!!db.contrast);
    updateClock();
  }
  function updateClock() { const now=performance.now(), v=format(timer.elapsed(now)), dot=v.lastIndexOf('.'); $('clock').replaceChildren(document.createTextNode(v.slice(0,dot))); const cents=document.createElement('span'); cents.textContent=v.slice(dot); $('clock').append(cents); $('rta').textContent=format(timer.realtime(now)); }
  function primary() {
    const now=performance.now();
    if(timer.phase==='ready') { timer.start(now); savedComplete=false; }
    else if(timer.phase==='paused') timer.resume(now);
    else if(timer.phase==='complete') { timer.reset(); savedComplete=false; }
    else timer.split(now);
    checkpoint(); render();
  }
  function finish() {
    const previous=personalBest();
    if(timer.finish(performance.now())&&!savedComplete){savedComplete=true;addRecord('complete');notify(!previous||timer.active<previous.active?'Personal best saved. Well played.':'Run complete. Your attempt is saved.');checkpoint();render();}
  }
  function pause() { if(timer.phase==='running') timer.pause(performance.now()); else if(timer.phase==='paused') timer.resume(performance.now()); checkpoint(); render(); }
  function undo() { if(timer.undo()) { checkpoint(); render(); } }
  function resetRequest() { if(['running','paused'].includes(timer.phase)) $('reset-dialog').showModal(); else {timer.reset();savedComplete=false;checkpoint();render();} }
  $('finish').onclick=finish; $('primary').onclick=primary; $('pause').onclick=pause; $('undo').onclick=undo; $('reset').onclick=resetRequest;
  $('reset-confirm').onclick=()=>{addRecord('abandoned');timer.reset();$('reset-dialog').close();render();notify('Attempt reset. Ready when you are.');};
  document.querySelectorAll('[data-close]').forEach(b=>b.onclick=()=>$(b.dataset.close).close());
  $('edit-route').onclick=()=>{ if(['running','paused'].includes(timer.phase)) return; $('route-title').value=db.route.title; $('route-category').value=db.route.category; $('route-names').value=db.route.names.join('\n'); $('route-error').textContent=''; $('route-dialog').showModal(); };
  $('route-form').onsubmit=e=>{e.preventDefault(); const route={title:$('route-title').value.trim(),category:$('route-category').value.trim(),names:$('route-names').value.split(/\r?\n/).map(n=>n.trim()).filter(Boolean)}; if(!validRoute(route)) {$('route-error').textContent='Enter a title, category, and 1–32 checkpoints of up to 80 characters each.';return;} db.route=route;timer=new Timer(route.names.length);db.pending=null;save();$('route-dialog').close();render();notify('Route saved. Your records are grouped by route.');};
  $('contrast').onclick=()=>{db.contrast=!db.contrast;save();render();};
  $('history-open').onclick=()=>{ const list=$('history-list');list.replaceChildren();const rows=routeRecords(); if(!rows.length){const p=document.createElement('p');p.className='form-note';p.textContent='Your first attempt is waiting. Completed and abandoned runs will appear here.';list.append(p);} rows.slice(0,150).forEach(r=>{const row=document.createElement('div');row.className='history-row';const left=document.createElement('div'),time=document.createElement('strong'),desc=document.createElement('small'),status=document.createElement('span');time.textContent=format(r.active);desc.textContent=`${new Date(r.date).toLocaleString()} · real ${format(r.rta)}`;status.className=r.status;status.textContent=r.status.toUpperCase();left.append(time,desc);row.append(left,status);list.append(row);});$('history-dialog').showModal();};
  $('export').onclick=()=>{ checkpoint(); const a=document.createElement('a'),url=URL.createObjectURL(new Blob([JSON.stringify(db,null,2)],{type:'application/json'})); a.href=url;a.download='RunClock-backup.json';a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);notify('Backup exported.'); };
  $('import').onclick=()=>{if(['running','paused'].includes(timer.phase)){notify('Finish or reset the current attempt before importing.');return;}$('import-file').value='';$('import-file').click();};
  $('import-file').onchange=async e=>{const f=e.target.files[0];if(!f)return;if(f.size>5*1024*1024){notify('Backup is too large. Maximum size: 5 MB.');return;}try{const parsed=JSON.parse(await f.text());if(!validDB(parsed))throw Error('Invalid backup');nextImport=parsed;$('import-dialog').showModal();}catch(e){notify('That file is not a valid RunClock desktop backup.');}};
  $('import-confirm').onclick=()=>{if(!nextImport)return;db=nextImport;if(db.pending){db.records.unshift({...db.pending,status:'interrupted'});db.pending=null;}db.records=db.records.slice(0,MAX_RECORDS);nextImport=null;timer=new Timer(db.route.names.length);canSave=true;save();$('import-dialog').close();render();notify('Backup imported.');};
  function modalOpen(){return !!document.querySelector('dialog[open]');}
  document.addEventListener('keydown',e=>{if(e.repeat||modalOpen()||['INPUT','TEXTAREA'].includes(document.activeElement?.tagName))return;const k=e.key.toLowerCase();if(k==='enter'||k===' '){if(['BUTTON','A'].includes(document.activeElement?.tagName))return;e.preventDefault();primary();}else if(k==='f')finish();else if(k==='p')pause();else if(k==='u')undo();else if(k==='r')resetRequest();});
  let previousButtons=[],lastPad='';
  function pollPad(){const pads=navigator.getGamepads?navigator.getGamepads():[],pad=Array.from(pads).find(Boolean);if(pad&&pad.mapping==='standard'){if(pad.id!==lastPad){lastPad=pad.id;$('controller-hint').textContent='Controller connected · Cross start/split · Triangle pause · Square undo · Options reset';}pad.buttons.forEach((b,i)=>{if(b.pressed&&!previousButtons[i]){if(modalOpen()){const dialog=document.querySelector('dialog[open]');if(i===1)dialog.close();else if(i===0&&document.activeElement?.tagName==='BUTTON')document.activeElement.click();else if(i>=12&&i<=15){const buttons=[...dialog.querySelectorAll('button:not(:disabled)')],at=buttons.indexOf(document.activeElement),dir=i===12||i===14?-1:1;buttons[(at+dir+buttons.length)%buttons.length]?.focus();}}else if(i===0)primary();else if(i===3)pause();else if(i===2)undo();else if(i===9)resetRequest();}previousButtons[i]=b.pressed;});}else{previousButtons=[];lastPad='';}requestAnimationFrame(pollPad);}
  window.addEventListener('pagehide',checkpoint);document.addEventListener('visibilitychange',()=>{if(document.hidden)checkpoint();});
  setInterval(checkpoint,5000);setInterval(updateClock,33);render();save();requestAnimationFrame(pollPad);
  // Exposed only for local automated checks; no network transport.
  window.RunClockPreview={getTimer:()=>timer,getData:()=>db,primary,pause,undo,finish,validDB};
})();
