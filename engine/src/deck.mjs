// SPDX-License-Identifier: GPL-3.0-or-later
// RunClock owns this presentation and action protocol. Clock redraws have an
// independent revision and never invalidate the currently displayed controls.
import {invariant} from './errors.mjs';
import {pageRows} from './overlays/protocol.mjs';
export {encodeDeck,DECK_WIRE} from './deck-wire.mjs';
const bump=x=>(x+1)>>>0||1;
export class ConsoleDeck {
  constructor(runtime){
    this.runtime=runtime;this.offset=0;this.sequence=0;this.metricsSequence=0;
    this.actionKey='';this.displayKey='';this.tokens=[];this.requests=new Map();this.tail=Promise.resolve();this.visible=false;
    this.dashboard={open(){},frame:()=>({enabled:false,epoch:1,ack:0,items:[]})};
    this.prospero={active:()=>true,location:{view:'overlay',id:'runclock'},open:async()=>this.runtime.overlays.host.open(this.runtime.overlays.registry.get('runclock'))};
  }
  setVisible(value){this.visible=!!value;this.runtime.overlays.host.setVisible(this.visible);}
  stop(){}
  frame(){
    const session=this.runtime.overlays.host.session('runclock'),page=session?.page;
    const all=page?pageRows(page):[{label:'Starting RunClock',detail:session?.message||'Preparing the private timing worker.',enabled:false}];
    this.offset=Math.min(this.offset,Math.max(0,all.length-32));
    const selected=all.slice(this.offset,this.offset+32).map(r=>({...r,triangle:r.enabled&&/^p\d+:(delete|delete-confirm)$/.test(r.args?.id||'')}));
    const base={title:page?.title||'RunClock v1.0.0-beta.1',subtitle:page?.subtitle||'BrewSparrow',footer:page?.footer||'Hold L3 + R3 to show/hide',rows:selected};
    const controls=JSON.stringify([session?.token||'',this.offset,{...base,rows:selected.map(r=>r.enabled?r:{...r,value:null})}]);
    if(controls!==this.actionKey){this.actionKey=controls;this.sequence=bump(this.sequence);this.tokens=selected.map(()=>this.runtime.id());}
    const display=JSON.stringify(base);
    if(display!==this.displayKey){this.displayKey=display;this.metricsSequence=bump(this.metricsSequence);}
    const onTimer=!!page?.items.some(i=>i.id==='state');
    return {ready:true,canBack:!onTimer,page:0,grid:false,reducedMotion:true,accent:'mint',ipVisible:false,ip:'',
      ...base,sequence:this.sequence,metricsSequence:this.metricsSequence,metrics:'',
      rows:selected.map((r,i)=>({...r,token:this.tokens[i]})),windowStart:this.offset,total:all.length,focus:0xffff,
      logoPath:'/user/data/RunClock/runtime/logo.png',notices:{sequence:1,current:null},dashboard:this.dashboard.frame(),input:this.runtime.gameInputRelay?.frame()};
  }
  action(input){
    invariant(input&&/^[a-f0-9]{32}$/.test(input.request||''),'DECK_REQUEST','Invalid action request.');
    const key=JSON.stringify([input.kind,input.sequence,input.row]);
    if(this.requests.has(input.request)){const old=this.requests.get(input.request);invariant(old.key===key,'DECK_REQUEST_REUSED','Request reused for another action.');return old.result;}
    const run=async()=>{
      if(input.kind==='window'){const f=this.frame();this.offset=Math.max(0,Math.min(input.row|0,Math.max(0,f.total-32)));return this.frame();}
      const f=this.frame();
      invariant(Number.isInteger(input.sequence)&&input.sequence===f.sequence,'DECK_STALE','The controls changed; select again.');
      if(input.kind==='back'){
        const page=this.runtime.overlays.host.session('runclock')?.page;
        if(page?.items.some(i=>i.id==='state'))this.setVisible(false);
        else this.runtime.overlays.host.back();
        return this.frame();
      }
      if(input.kind==='next'||input.kind==='previous')return this.frame();
      invariant(input.kind==='select'&&Number.isInteger(input.row),'DECK_ACTION','Unsupported action.');
      const row=f.rows[input.row];invariant(row?.enabled&&row.args?.id,'DECK_DISABLED','Select an enabled control.');
      this.runtime.overlays.host.event(row.args.id,'select');return this.frame();
    };
    const result=this.tail.then(run,run);this.tail=result.catch(()=>{});
    this.requests.set(input.request,{key,result});while(this.requests.size>256)this.requests.delete(this.requests.keys().next().value);
    return result;
  }
}
