// SPDX-License-Identifier: GPL-3.0-or-later
import http from 'node:http';
import {timingSafeEqual} from 'node:crypto';
import {encodeDeck} from './deck.mjs';
import {syncDeck} from './deck-sync.mjs';
export async function serveRunClock(runtime,{port=8890,host='127.0.0.1'}={}){
 const server=http.createServer(async(req,res)=>{
  try{
   const token=req.headers['x-runclock-native']||'';
   if(req.method!=='POST'||req.url!=='/native/deck/sync'||req.headers.origin||
      !['127.0.0.1','::1','::ffff:127.0.0.1'].includes(req.socket.remoteAddress)||
      !/^[a-f0-9]{32}$/.test(token)||!timingSafeEqual(Buffer.from(token),Buffer.from(runtime.deckCredential))){res.writeHead(403);return res.end();}
   if(!/^application\/json(?:;|$)/i.test(req.headers['content-type']||'')){res.writeHead(400);return res.end();}
   let bytes=0,chunks=[];
   for await(const chunk of req){bytes+=chunk.length;if(bytes>4096)throw Error('Request too large');chunks.push(chunk);}
   const input=JSON.parse(Buffer.concat(chunks).toString('utf8'));
   runtime.gameInputRelay?.receive(input.feedback);
   if(typeof input.menuOpen==='boolean')runtime.consoleDeck.setVisible(input.menuOpen);
   const {frame,status}=await syncDeck(runtime.consoleDeck,input.action);
   const body=encodeDeck(frame);runtime.gameInputRelay?.delivered?.(frame.input);
   res.writeHead(status,{'Content-Type':'application/octet-stream','Content-Length':body.length,'Cache-Control':'no-store'});res.end(body);
  }catch(error){console.log('RunClock bridge: '+String(error.code||error.message));if(!res.headersSent)res.writeHead(400);res.end();}
 });
 await new Promise((resolve,reject)=>{server.once('error',reject);server.listen(port,host,resolve);});return server;
}
