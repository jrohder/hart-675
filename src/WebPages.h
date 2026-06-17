#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

// Revision 3 single-page application served from flash. A hamburger drawer
// switches sections; an always-on top bar shows USB/TCP/HART/battery status.
// Static pages (HART Device, HART Maintenance, System Diagnostics, System
// Configuration, Profiles) always exist. Manufacturer-specific pages are
// generated dynamically from the active Wireless HART Profile (WHPF) JSON.
// No external CDN / libraries.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover">
<title>Wireless HART 67</title>
<style>
  :root{
    --bg:#0e1116; --panel:#161b22; --panel2:#1c2530; --line:#2a3441;
    --text:#e6edf3; --muted:#8b98a5; --accent:#4fc3f7; --green:#3fb950;
    --red:#f85149; --yellow:#d29922; --blue:#388bfd;
  }
  *{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
  body{background:var(--bg);color:var(--text);
    font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
    padding-bottom:env(safe-area-inset-bottom)}

  /* ---- Top bar ---- */
  .topbar{position:sticky;top:0;z-index:30;display:flex;align-items:center;gap:10px;
    background:var(--panel);border-bottom:1px solid var(--line);
    padding:10px 12px;padding-top:max(10px,env(safe-area-inset-top))}
  .ham{background:none;border:none;color:var(--text);font-size:22px;line-height:1;
    cursor:pointer;padding:6px 10px;border-radius:8px}
  .ham:active{background:var(--panel2)}
  .tbrand{font-size:15px;color:var(--accent);font-weight:700}
  .tbstatus{margin-left:auto;display:flex;align-items:center;gap:12px;
    font-size:13px;color:var(--muted);flex-wrap:wrap;justify-content:flex-end}
  .chip{display:flex;align-items:center;gap:5px;white-space:nowrap}
  .dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:#39424d}
  .dot.on{background:var(--green);box-shadow:0 0 8px var(--green)}
  .dot.usb{background:var(--blue);box-shadow:0 0 8px var(--blue)}
  .dot.warn{background:var(--yellow);box-shadow:0 0 8px var(--yellow)}
  .dot.off{background:#39424d}
  .pill{padding:2px 8px;border-radius:10px;font-size:12px;font-weight:600}
  .pill.online{background:rgba(63,185,80,.18);color:var(--green)}
  .pill.searching{background:rgba(210,153,34,.18);color:var(--yellow)}
  .pill.off{background:rgba(139,152,165,.15);color:var(--muted)}

  /* ---- Drawer ---- */
  .backdrop{position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:40;
    opacity:0;pointer-events:none;transition:opacity .2s}
  .backdrop.show{opacity:1;pointer-events:auto}
  .drawer{position:fixed;top:0;left:0;bottom:0;width:270px;max-width:82vw;z-index:50;
    background:var(--panel);border-right:1px solid var(--line);
    transform:translateX(-100%);transition:transform .22s ease;
    overflow-y:auto;padding-top:max(14px,env(safe-area-inset-top))}
  .drawer.show{transform:translateX(0)}
  .dhead{padding:6px 18px 14px;font-size:17px;font-weight:700;color:var(--accent)}
  .dgroup{padding:8px 18px 4px;font-size:11px;letter-spacing:.08em;
    text-transform:uppercase;color:var(--muted)}
  .navlink{display:block;padding:13px 18px;color:var(--text);text-decoration:none;
    font-size:15px;border-left:3px solid transparent;cursor:pointer}
  .navlink:active{background:var(--panel2)}
  .navlink.active{background:var(--panel2);border-left-color:var(--accent);color:var(--accent)}
  .navsep{height:1px;background:var(--line);margin:8px 0}

  /* ---- Content ---- */
  .content{padding:14px;max-width:760px;margin:0 auto}
  .page{display:none}
  .page.active{display:block}
  h2.title{font-size:20px;margin-bottom:4px}
  .sub{color:var(--muted);font-size:13px;margin-bottom:14px}

  .card{background:var(--panel);border:1px solid var(--line);border-radius:12px;
    padding:14px;margin-bottom:14px}
  .card h3{font-size:14px;color:var(--accent);margin-bottom:10px;
    text-transform:uppercase;letter-spacing:.04em}
  .row{display:flex;justify-content:space-between;gap:10px;padding:7px 0;
    border-bottom:1px solid var(--line);font-size:14px}
  .row:last-child{border-bottom:none}
  .row .k{color:var(--muted)}
  .row .v{font-weight:600;text-align:right;word-break:break-word}

  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));gap:10px}
  .metric{background:var(--panel2);border-radius:10px;padding:12px}
  .metric .ml{font-size:12px;color:var(--muted)}
  .metric .mv{font-size:22px;font-weight:700;margin-top:2px}
  .metric .mu{font-size:12px;color:var(--muted);margin-left:3px}

  label.fl{display:block;font-size:13px;color:var(--muted);margin:10px 0 4px}
  input,select{width:100%;background:var(--bg);border:1px solid var(--line);
    color:var(--text);border-radius:8px;padding:11px;font-size:15px}
  input:focus,select:focus{outline:none;border-color:var(--accent)}

  button.btn{width:100%;background:var(--accent);color:#06121b;border:none;
    border-radius:8px;padding:13px;font-size:15px;font-weight:700;cursor:pointer;
    margin-top:12px}
  button.btn:active{opacity:.85}
  button.btn.sec{background:var(--panel2);color:var(--text);border:1px solid var(--line)}
  button.btn.danger{background:var(--red);color:#fff}
  button.btn.warn{background:var(--yellow);color:#181203}
  .btnrow{display:grid;grid-template-columns:repeat(auto-fit,minmax(90px,1fr));gap:8px}
  .btnrow button{margin-top:0}

  .banner{background:rgba(210,153,34,.12);border:1px solid var(--yellow);
    border-radius:10px;padding:12px;font-size:14px;margin-bottom:14px;color:#f0d9a6}
  .ok{color:var(--green)} .bad{color:var(--red)} .mut{color:var(--muted)}
  canvas{width:100%;height:120px;display:block;background:var(--bg);border-radius:8px}
  .log{font-family:ui-monospace,Menlo,monospace;font-size:12px;white-space:pre-wrap;
    background:var(--bg);border-radius:8px;padding:10px;max-height:240px;overflow:auto;
    color:#9fb1c1}

  /* ---- Modal ---- */
  .modal{position:fixed;inset:0;z-index:60;background:rgba(0,0,0,.6);
    display:none;align-items:center;justify-content:center;padding:20px}
  .modal.show{display:flex}
  .modal .box{background:var(--panel);border:1px solid var(--line);border-radius:14px;
    padding:18px;width:100%;max-width:380px}
  .modal h3{font-size:17px;margin-bottom:10px}
  .modal .cmp{display:flex;justify-content:space-between;padding:8px 0;
    border-bottom:1px solid var(--line);font-size:15px}
  .modal .cmp .v{font-weight:700}

  /* ---- Toast ---- */
  .toast{position:fixed;left:50%;bottom:24px;transform:translateX(-50%);
    background:var(--panel2);border:1px solid var(--line);color:var(--text);
    padding:11px 16px;border-radius:10px;font-size:14px;z-index:70;
    opacity:0;transition:opacity .2s;max-width:90vw;text-align:center}
  .toast.show{opacity:1}
</style>
</head>
<body>

<div class="topbar">
  <button class="ham" id="ham">&#9776;</button>
  <span class="tbrand">Wireless HART 67</span>
  <div class="tbstatus">
    <span class="chip"><span class="dot" id="tUsb"></span>USB</span>
    <span class="chip"><span class="dot" id="tTcp"></span>TCP</span>
    <span class="pill off" id="tHart">--</span>
    <span class="chip"><span id="tBatt">--%</span></span>
  </div>
</div>

<div class="backdrop" id="backdrop"></div>
<nav class="drawer" id="drawer">
  <div class="dhead">&#9776; Menu</div>
  <a class="navlink active" data-page="device">HART Device</a>
  <a class="navlink" data-page="maint">HART Maintenance</a>
  <div id="dynNav"></div>
  <div class="navsep"></div>
  <div class="dgroup">System</div>
  <a class="navlink" data-page="monitor">HART Monitor</a>
  <a class="navlink" data-page="diag">System Diagnostics</a>
  <a class="navlink" data-page="config">System Configuration</a>
  <a class="navlink" data-page="profiles">Profiles</a>
</nav>

<div class="content">

  <!-- ============ HART DEVICE ============ -->
  <section class="page active" id="page-device">
    <h2 class="title">HART Device</h2>
    <div class="sub" id="devProfileNote">Generic HART information.</div>

    <div class="card">
      <h3>Process Variables</h3>
      <div class="grid" id="pvGrid"></div>
    </div>

    <div class="card">
      <h3>Device Identification</h3>
      <div id="identTbl"></div>
    </div>

    <div class="card">
      <h3>Communication</h3>
      <div id="commTbl"></div>
      <button class="btn sec" onclick="loadDevice()">Refresh</button>
    </div>
  </section>

  <!-- ============ HART MAINTENANCE ============ -->
  <section class="page" id="page-maint">
    <h2 class="title">HART Maintenance</h2>
    <div class="sub">Generic configuration and maintenance. Works with most HART transmitters.</div>
    <div id="maintWarn"></div>

    <div class="card">
      <h3>Configuration</h3>
      <div id="cfgTbl"></div>
      <label class="fl">PV Lower Range Value (LRV)</label>
      <input id="inLrv" type="number" step="any">
      <button class="btn" onclick="writeLrv()">Write LRV</button>
      <label class="fl">PV Upper Range Value (URV)</label>
      <input id="inUrv" type="number" step="any">
      <button class="btn" onclick="writeUrv()">Write URV</button>
      <label class="fl">PV Damping (seconds)</label>
      <input id="inDamp" type="number" step="any">
      <button class="btn" onclick="writeDamping()">Write Damping</button>
      <label class="fl">Polling Address (0-63)</label>
      <input id="inPoll" type="number" min="0" max="63">
      <button class="btn" onclick="writePollAddr()">Write Poll Address</button>
      <button class="btn sec" onclick="readConfig()">Read Configuration</button>
    </div>

    <div class="card">
      <h3>Loop Test (Fixed Current)</h3>
      <div class="btnrow">
        <button class="btn" onclick="loopTest(4)">4 mA</button>
        <button class="btn" onclick="loopTest(12)">12 mA</button>
        <button class="btn" onclick="loopTest(20)">20 mA</button>
      </div>
      <label class="fl">Custom mA</label>
      <input id="inMa" type="number" step="any" placeholder="e.g. 8.0">
      <button class="btn" onclick="loopTestCustom()">Apply Custom</button>
      <button class="btn warn" onclick="loopTestStop()">Stop Test (Exit Fixed Mode)</button>
    </div>

    <div class="card">
      <h3>Device Commands</h3>
      <button class="btn sec" onclick="refreshDevice()">Refresh Device</button>
      <button class="btn sec" onclick="readDynamic()">Read Dynamic Variables</button>
      <button class="btn warn" onclick="selfTest()">Self Test (advanced)</button>
      <button class="btn danger" onclick="deviceReset()">Device Reset</button>
    </div>
  </section>

  <!-- ============ HART MONITOR ============ -->
  <section class="page" id="page-monitor">
    <h2 class="title">HART Monitor</h2>
    <div class="sub">Raw AD5700 modem activity and carrier detect.</div>
    <div class="card"><div id="monTbl"></div></div>
    <div class="card"><h3>Recent Frames</h3><div class="log" id="monLog">--</div></div>
  </section>

  <!-- ============ SYSTEM DIAGNOSTICS ============ -->
  <section class="page" id="page-diag">
    <h2 class="title">System Diagnostics</h2>
    <div class="sub">Communicator health.</div>
    <div class="card"><h3>Trends</h3>
      <div class="mut" style="font-size:12px">PV</div><canvas id="gPv"></canvas>
      <div class="mut" style="font-size:12px;margin-top:8px">Loop Current (mA)</div><canvas id="gLoop"></canvas>
      <div class="mut" style="font-size:12px;margin-top:8px">Battery (%)</div><canvas id="gBatt"></canvas>
    </div>
    <div class="card"><h3>System</h3><div id="diagTbl"></div></div>
    <div class="card"><h3>System Log</h3><div class="log" id="diagLog">--</div></div>
  </section>

  <!-- ============ SYSTEM CONFIGURATION ============ -->
  <section class="page" id="page-config">
    <h2 class="title">System Configuration</h2>
    <div class="sub">Configure the communicator.</div>
    <div class="card">
      <h3>HART Master</h3>
      <div class="row"><span class="k">Auto-poll on connect</span>
        <span class="v"><input type="checkbox" id="cfgMaster" style="width:auto" onchange="toggleMaster()"></span></div>
      <label class="fl">HART Polling Address (0-63)</label>
      <input id="cfgPoll" type="number" min="0" max="63">
    </div>
    <div class="card">
      <h3>HART Hardware</h3>
      <div id="hartHwTbl"></div>
    </div>
    <div class="card">
      <h3>Device / Network</h3>
      <label class="fl">Device Name</label><input id="cfgName">
      <label class="fl">WiFi SSID</label><input id="cfgSsid">
      <label class="fl">WiFi Password</label><input id="cfgPass" type="text">
      <label class="fl">TCP Port</label><input id="cfgTcp" type="number">
      <label class="fl">Auto Sleep Timeout (s, 0=off)</label><input id="cfgSleep" type="number">
      <label class="fl">LED Brightness (0-255)</label><input id="cfgLed" type="number">
      <label class="fl">Refresh Rate (ms)</label><input id="cfgRefresh" type="number">
      <button class="btn" onclick="saveConfig()">Save Configuration</button>
    </div>
    <div class="card">
      <h3>System</h3>
      <button class="btn sec" onclick="doReboot()">Reboot Device</button>
      <button class="btn danger" onclick="doFactory()">Factory Reset</button>
    </div>
  </section>

  <!-- ============ PROFILES ============ -->
  <section class="page" id="page-profiles">
    <h2 class="title">Profiles</h2>
    <div class="sub">Manage installed device profiles.</div>
    <div class="card">
      <h3>Active Profile</h3>
      <div id="profActive"></div>
    </div>
    <div class="card">
      <h3>Upload Profile</h3>
      <div class="sub">Select a Wireless HART Profile (.json) from your phone.</div>
      <input type="file" id="profFile" accept=".json,application/json">
      <button class="btn" onclick="uploadProfile()">Upload</button>
    </div>
    <div class="card">
      <h3>Installed Profiles</h3>
      <div id="profList"></div>
    </div>
  </section>

</div>

<!-- Confirm modal -->
<div class="modal" id="modal">
  <div class="box">
    <h3 id="mTitle">Confirm</h3>
    <div id="mBody"></div>
    <button class="btn" id="mYes">YES</button>
    <button class="btn sec" id="mNo">NO</button>
  </div>
</div>

<div class="toast" id="toast"></div>

<script>
//==================== helpers ====================
const $=id=>document.getElementById(id);
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
let refreshMs=2000, statusTimer=null, devTimer=null, prof=null, curPage='device';

function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');
  clearTimeout(t._t);t._t=setTimeout(()=>t.classList.remove('show'),2600);}

function confirmDialog(title,rows){
  return new Promise(res=>{
    $('mTitle').textContent=title;
    let h='';rows.forEach(r=>{h+='<div class="cmp"><span class="mut">'+r[0]+
      '</span><span class="v">'+r[1]+'</span></div>';});
    $('mBody').innerHTML=h;
    const m=$('modal');m.classList.add('show');
    const done=v=>{m.classList.remove('show');$('mYes').onclick=null;$('mNo').onclick=null;res(v);};
    $('mYes').onclick=()=>done(true);$('mNo').onclick=()=>done(false);
  });
}

//---- byte/number coding ----
function hx2b(h){h=(h||'').replace(/[^0-9a-fA-F]/g,'');const a=[];
  for(let i=0;i+1<h.length;i+=2)a.push(parseInt(h.substr(i,2),16));return a;}
function b2hx(a){return a.map(x=>(x&255).toString(16).padStart(2,'0').toUpperCase()).join('');}
function rdFloat(a,o){o=o||0;if(o+4>a.length)return NaN;
  const dv=new DataView(new Uint8Array(a.slice(o,o+4)).buffer);return dv.getFloat32(0,false);}
function wrFloat(f){const b=new Uint8Array(4);new DataView(b.buffer).setFloat32(0,f,false);return Array.from(b);}
function rdU16(a,o){return (a[o]<<8)|a[o+1];}
function rdU32(a,o){return ((a[o]<<24)|(a[o+1]<<16)|(a[o+2]<<8)|a[o+3])>>>0;}
//---- HART packed ASCII (6-bit) ----
function unpackAscii(a,off,nbytes){let s='';let chars=Math.floor(nbytes*8/6);
  let bits=0,buf=0;for(let i=0;i<nbytes;i++){buf=(buf<<8)|a[off+i];bits+=8;
    while(bits>=6){bits-=6;let c=(buf>>bits)&0x3f;if(c<32)c+=64;s+=String.fromCharCode(c);}}
  return s.replace(/\s+$/,'');}

//==================== generic HART command engine ====================
async function hartCmd(command,dataHex){
  const body=new URLSearchParams();body.set('command',command);
  if(dataHex)body.set('data',dataHex);
  const r=await fetch('/api/hart/cmd',{method:'POST',body});
  const q=await r.json();
  if(!q.id){throw new Error(q.error||'busy');}
  for(let i=0;i<80;i++){
    await sleep(100);
    const rr=await fetch('/api/hart/cmd?id='+q.id);const res=await rr.json();
    if(res.state==='done')return {rc:res.rc,ds:res.deviceStatus,bytes:hx2b(res.data),hex:res.data};
    if(res.state==='error')throw new Error('device did not respond');
    if(res.state==='unknown')throw new Error('result lost');
  }
  throw new Error('timeout');
}

async function triggerRefresh(){
  await fetch('/api/hart/refresh',{method:'POST'});
}

async function waitForFreshData(checkFn,maxMs){
  const t0=Date.now();
  while(Date.now()-t0<maxMs){
    const d=await(await fetch('/api/hart')).json();
    if(checkFn(d))return d;
    await sleep(400);
  }
  return await(await fetch('/api/hart')).json();
}

//==================== navigation ====================
function openDrawer(o){$('drawer').classList.toggle('show',o);$('backdrop').classList.toggle('show',o);}
$('ham').onclick=()=>openDrawer(!$('drawer').classList.contains('show'));
$('backdrop').onclick=()=>openDrawer(false);

function showPage(id){
  document.querySelectorAll('.page').forEach(p=>p.classList.remove('active'));
  const el=$('page-'+id);if(el)el.classList.add('active');
  document.querySelectorAll('.navlink').forEach(n=>n.classList.toggle('active',n.dataset.page===id));
  curPage=id;openDrawer(false);window.scrollTo(0,0);
  if(id==='device')loadDevice();
  else if(id==='maint')loadMaint();
  else if(id==='monitor')loadMonitor();
  else if(id==='diag'){loadDiag();loadTrends();}
  else if(id==='config')loadSettings();
  else if(id==='profiles')loadProfiles();
  else if(id.startsWith('dyn'))renderDynPage(parseInt(id.slice(3)));
}
function bindNav(){document.querySelectorAll('.navlink').forEach(n=>{
  n.onclick=()=>showPage(n.dataset.page);});}

//==================== top bar / status ====================
async function loadStatus(){
  try{
    const s=await(await fetch('/api/status')).json();
    $('tUsb').className='dot'+(s.usbActive?' usb':'');
    $('tTcp').className='dot'+(s.tcpClient?' on':'');
    const hp=$('tHart');hp.textContent=s.hartMaster;hp.className='pill '+s.hartMaster;
    const b=s.batteryPercent;$('tBatt').textContent=(b>=0?b:'--')+'%';
    window._status=s;
    if(curPage==='diag')fillDiag(s);
    if(curPage==='monitor')fillMonStatus(s);
    if(curPage==='config')fillHartHardware(s);
  }catch(e){}
}

//==================== HART DEVICE ====================
function rowsHtml(pairs){return pairs.map(p=>'<div class="row"><span class="k">'+p[0]+
  '</span><span class="v">'+(p[1]===''||p[1]==null?'<span class=mut>--</span>':p[1])+'</span></div>').join('');}
function fmt(v,d){return (v==null||isNaN(v))?'--':Number(v).toFixed(d==null?2:d);}

const MFR={0:'(generic)',17:'Endress+Hauser',26:'ABB',38:'Rosemount',
  39:'Yokogawa',56:'Krohne',58:'Vega',74:'Micro Motion',96:'Magnetrol',
  19:'Fisher',6:'Emerson'};

async function loadDevice(){
  try{
    const d=await(await fetch('/api/hart')).json();
    const u=d.pvUnits||'';
    $('pvGrid').innerHTML=[
      ['PV',fmt(d.pv,3),u],['Loop Current',fmt(d.loopCurrent,3),'mA'],
      ['% Range',fmt(d.percentRange,1),'%'],['SV',fmt(d.sv,3),''],
      ['TV',fmt(d.tv,3),''],['QV',fmt(d.qv,3),'']
    ].map(m=>'<div class="metric"><div class="ml">'+m[0]+'</div><div class="mv">'+
      m[1]+'<span class="mu">'+m[2]+'</span></div></div>').join('');
    const mfr=MFR[d.manufacturerId]||('ID '+d.manufacturerId);
    $('identTbl').innerHTML=rowsHtml([
      ['Manufacturer',mfr],['Device Type','0x'+(d.deviceType||0).toString(16)],
      ['Device Revision',d.deviceRevision],['Software Rev',d.softwareRevision],
      ['Hardware Rev',d.hardwareRevision],['Universal Rev',d.universalRev],
      ['Unique Device ID',d.deviceId],['Polling Address',d.pollAddress],
      ['Tag',d.tag],['Long Tag',d.longTag],['Descriptor',d.descriptor],
      ['Message',d.message],['Date',d.date]
    ]);
    const st=d.valid?'<span class="ok">Online</span>':
      (d.enabled?'<span class="bad">Searching</span>':'<span class="mut">Master off</span>');
    $('commTbl').innerHTML=rowsHtml([
      ['HART Carrier',(window._status&&window._status.carrier)?'<span class=ok>Detected</span>':'<span class=mut>None</span>'],
      ['Communication',st],['Last Response',d.lastCommMsAgo+' ms ago'],
      ['Good Responses',d.goodResponses],['Comm Errors',d.commErrors],
      ['TX Frames',d.txFrames],['RX Frames',d.rxFrames]
    ]);
    $('devProfileNote').textContent=(prof&&prof.manufacturer&&prof.manufacturer!=='Generic')?
      ('Profile: '+prof.manufacturer+' '+prof.device):'Generic HART information.';
  }catch(e){}
}

//==================== HART MAINTENANCE ====================
let cfgCache={units:0,urv:NaN,lrv:NaN,damp:NaN};

function unitsLabel(d){
  if(d.configUnitsStr && d.configUnitsStr!=='--') return d.configUnitsStr;
  if(d.pvUnits && typeof d.pvUnits==='string' && !/^\d+$/.test(d.pvUnits)) return d.pvUnits;
  const map={1:'inH2O',2:'inHg',3:'ftH2O',4:'mmH2O',5:'mH2O',6:'psi',7:'bar',8:'mbar',
    9:'g/cm2',10:'kg/cm2',11:'Pa',12:'mmHg',16:'kPa',32:'degC',33:'degF',39:'mA',57:'%'};
  const c=d.configUnits||d.pvUnitsCode;
  return (c!=null && map[c])?map[c]:((c!=null && c!==0)?('unit '+c):'--');
}

function fillMaintFromDevice(d){
  if(d.configValid){
    cfgCache.units=d.configUnits||d.pvUnitsCode;
    cfgCache.urv=d.configUrv;cfgCache.lrv=d.configLrv;cfgCache.damp=d.configDamping;
  }
  const units=unitsLabel(d);
  const wp=d.writeProtect;
  $('cfgTbl').innerHTML=rowsHtml([
    ['Units',units],
    ['URV',d.configValid?fmt(d.configUrv,3)+' '+units:'--'],
    ['LRV',d.configValid?fmt(d.configLrv,3)+' '+units:'--'],
    ['Damping (s)',d.configValid?fmt(d.configDamping,2):'--'],
    ['Write Protect',wp===0?'No':(wp===1?'Yes':'--')],
    ['Poll Address',d.pollAddress],
    ['Config age',d.configValid?(d.configAgeMs+' ms ago'):'not read yet']
  ]);
  if(d.configValid){
    $('inUrv').value=isNaN(d.configUrv)?'':Number(d.configUrv).toFixed(3);
    $('inLrv').value=isNaN(d.configLrv)?'':Number(d.configLrv).toFixed(3);
    $('inDamp').value=isNaN(d.configDamping)?'':Number(d.configDamping).toFixed(2);
  }
  $('inPoll').value=d.pollAddress;
  if(wp===1){
    $('maintWarn').innerHTML='<div class="banner">Write protect is ON. Range and damping writes will fail until disabled at the transmitter.</div>';
  }
}

async function loadMaint(){
  const w=$('maintWarn');
  try{
    const d=await(await fetch('/api/hart')).json();
    if(!d.valid){
      w.innerHTML='<div class="banner">No device online. Connect a transmitter and enable the HART master.</div>';
      $('cfgTbl').innerHTML='<div class="mut">Waiting for device...</div>';
      return;
    }
    w.innerHTML='';
    fillMaintFromDevice(d);
    if(!d.configValid){
      $('cfgTbl').innerHTML+='<div class="mut" style="margin-top:8px">Tap <b>Read Configuration</b> to fetch range and damping from the device.</div>';
    }
  }catch(e){$('cfgTbl').innerHTML='<div class="mut">Could not load: '+e.message+'</div>';}
}

async function readConfig(){
  toast('Reading configuration...');
  try{
    const r=await fetch('/api/hart/config/read',{method:'POST'});
    const j=await r.json();
    if(!j.ok)throw new Error('device did not return configuration');
    fillMaintFromDevice(j.hart?j.hart:await(await fetch('/api/hart')).json());
    toast('Configuration updated');
  }catch(e){toast('Failed: '+e.message);}
}

async function refreshDevice(){
  toast('Refreshing device...');
  try{
    await triggerRefresh();
    await waitForFreshData(x=>x.valid,6000);
    await loadDevice();
    toast('Device data refreshed');
  }catch(e){toast('Refresh failed: '+e.message);}
}
async function writeRange(urv,lrv){
  const body=new URLSearchParams({urv:String(urv)});
  if(!isNaN(lrv))body.set('lrv',String(lrv));
  const r=await fetch('/api/hart/range',{method:'POST',body});
  const j=await r.json();
  if(!j.ok)throw new Error(j.message||'write failed');
  if(j.hart)fillMaintFromDevice(j.hart);
  return j;
}
async function writeUrv(){
  const v=parseFloat($('inUrv').value);if(isNaN(v))return toast('Enter a value');
  const cur=cfgCache.urv;
  if(!await confirmDialog('Change URV',[['Current',fmt(cur,3)],['New',v.toFixed(3)]]))return;
  try{
    const lrv=isNaN(cfgCache.lrv)?parseFloat($('inLrv').value):cfgCache.lrv;
    await writeRange(v,lrv);
    toast('URV updated');
  }catch(e){toast('Failed: '+e.message);}
}
async function writeLrv(){
  const v=parseFloat($('inLrv').value);if(isNaN(v))return toast('Enter a value');
  if(!await confirmDialog('Change LRV',[['Current',fmt(cfgCache.lrv,3)],['New',v.toFixed(3)]]))return;
  try{
    const urv=isNaN(cfgCache.urv)?parseFloat($('inUrv').value):cfgCache.urv;
    await writeRange(urv,v);
    toast('LRV updated');
  }catch(e){toast('Failed: '+e.message);}
}
async function writeDamping(){
  const v=parseFloat($('inDamp').value);if(isNaN(v))return toast('Enter a value');
  if(!await confirmDialog('Change Damping',[['Current',fmt(cfgCache.damp,2)+' s'],['New',v.toFixed(2)+' s']]))return;
  try{
    const r=await fetch('/api/hart/damping',{method:'POST',body:new URLSearchParams({value:String(v)})});
    const j=await r.json();if(!j.ok)throw new Error('write failed');
    if(j.hart)fillMaintFromDevice(j.hart);
    toast('Damping updated');
  }catch(e){toast('Failed: '+e.message);}
}
async function writePollAddr(){
  const v=parseInt($('inPoll').value);if(isNaN(v)||v<0||v>63)return toast('0-63');
  if(!await confirmDialog('Change Poll Address',[['New',v]]))return;
  try{
    const r=await fetch('/api/hart/polladdr',{method:'POST',body:new URLSearchParams({address:String(v)})});
    const j=await r.json();if(!j.ok)throw new Error('write failed');
    toast('Poll address set to '+v);
  }catch(e){toast('Failed: '+e.message);}
}
async function loopTest(ma){
  if(!await confirmDialog('Loop Test',[['Force output',ma+' mA']]))return;
  try{await hartCmd(40,b2hx(wrFloat(ma)));toast('Output forced to '+ma+' mA');}catch(e){toast('Failed: '+e.message);}
}
async function loopTestCustom(){const v=parseFloat($('inMa').value);
  if(isNaN(v))return toast('Enter mA');loopTest(v);}
async function loopTestStop(){
  try{await hartCmd(40,b2hx(wrFloat(0)));toast('Fixed current mode exited');}catch(e){toast('Failed: '+e.message);}
}
async function readDynamic(){
  toast('Reading dynamic variables...');
  try{
    await triggerRefresh();
    const d=await waitForFreshData(x=>x.valid&&!isNaN(x.pv),6000);
    await loadDevice();
    toast('PV '+fmt(d.pv,3)+' | SV '+fmt(d.sv,3)+' | TV '+fmt(d.tv,3)+' | QV '+fmt(d.qv,3));
  }catch(e){toast('Failed: '+e.message);}
}
async function selfTest(){
  if(!await confirmDialog('Self Test (Command 41)',[
    ['What it does','Runs the transmitter built-in self test'],
    ['Note','Many devices acknowledge silently; no visible result is normal']
  ]))return;
  try{
    await hartCmd(41,'');
    toast('Self test command sent');
  }catch(e){
    toast('Self test not supported or no reply (this is common)');
  }
}
async function deviceReset(){
  if(!await confirmDialog('Device Reset',[['Warning','Device will reset'],['Continue?','']]))return;
  try{await hartCmd(42,'');toast('Device reset sent');}catch(e){toast('Failed: '+e.message);}
}

//==================== HART MONITOR ====================
function fillMonStatus(s){
  $('monTbl').innerHTML=rowsHtml([
    ['Carrier',s.carrier?'<span class=ok>Detected</span>':'<span class=mut>None</span>'],
    ['AD5700 Power',s.ad5700Powered?'<span class=ok>ON</span>':'<span class=bad>OFF</span>'],
    ['OCD raw',s.ocdRaw],['Transmitting',s.transmitting?'Yes':'No'],
    ['Modem Owner',s.owner],['TX bytes',s.txBytes],['RX bytes',s.rxBytes]
  ]);
}
async function loadMonitor(){
  try{const s=await(await fetch('/api/status')).json();fillMonStatus(s);}catch(e){}
  try{const m=await(await fetch('/api/hartmon')).json();
    $('monLog').textContent=(Array.isArray(m)&&m.length)?
      m.map(f=>f.dir+' ['+f.n+'] '+f.hex).join('\n'):'No frames captured yet.';
  }catch(e){$('monLog').textContent='Monitor unavailable.';}
}

//==================== DIAGNOSTICS ====================
function fillDiag(s){
  $('diagTbl').innerHTML=rowsHtml([
    ['Battery',s.batteryPercent+'%'],
    ['AD5700',s.ad5700Powered?'Powered':'Off'],
    ['HART Carrier',s.carrier?'Detected':'None'],
    ['USB',s.usbActive?'Connected':'No'],
    ['WiFi',s.wifiConnected?'Connected':'Off'],
    ['Sleep Timer',s.sleepTimerActive?'Active':'Off'],
    ['Firmware',s.fwVersion],['Battery Voltage',fmt(s.batteryVoltage,2)+' V'],
    ['Battery Health',s.batteryHealth],['TCP Client',s.tcpClient?'Connected':'No'],
    ['WiFi Clients',s.wifiClients],['HART Owner',s.owner],
    ['Free Heap',(s.freeHeap/1024).toFixed(1)+' KB'],['CPU Usage',s.cpuUsage+'%'],
    ['Uptime',s.uptime+' s'],['Boot Count',s.bootCount],['Wake Reason',s.wakeReason],
    ['TX Packets',s.txPackets],['RX Packets',s.rxPackets],['UART Errors',s.uartErrors],
    ['Buffer Overruns',s.bufferOverruns],['Last Error',s.lastError||'none']
  ]);
}
async function loadDiag(){
  try{const s=await(await fetch('/api/status')).json();fillDiag(s);}catch(e){}
  try{const l=await(await fetch('/api/logs')).json();
    $('diagLog').textContent=(Array.isArray(l)&&l.length)?l.join('\n'):'No log.';
  }catch(e){}
}
function drawGraph(cid,data,color){
  const c=$(cid);if(!c)return;const dpr=window.devicePixelRatio||1;
  const w=c.clientWidth,h=c.clientHeight;c.width=w*dpr;c.height=h*dpr;
  const x=c.getContext('2d');x.scale(dpr,dpr);x.clearRect(0,0,w,h);
  const pts=(data||[]).filter(v=>v!=null&&!isNaN(v));
  if(pts.length<2){x.fillStyle='#5a6b7a';x.font='12px sans-serif';x.fillText('no data',8,h/2);return;}
  let mn=Math.min(...pts),mx=Math.max(...pts);if(mn===mx){mn-=1;mx+=1;}
  const pad=4,sx=w/(data.length-1),sy=(h-2*pad)/(mx-mn);
  x.strokeStyle=color;x.lineWidth=2;x.beginPath();let started=false;
  data.forEach((v,i)=>{if(v==null||isNaN(v)){started=false;return;}
    const px=i*sx,py=h-pad-(v-mn)*sy;if(!started){x.moveTo(px,py);started=true;}else x.lineTo(px,py);});
  x.stroke();
  x.fillStyle='#5a6b7a';x.font='10px sans-serif';x.fillText(mx.toFixed(1),3,11);x.fillText(mn.toFixed(1),3,h-3);
}
async function loadTrends(){
  try{const t=await(await fetch('/api/trends')).json();
    drawGraph('gPv',t.pv,'#4fc3f7');drawGraph('gLoop',t.loop,'#3fb950');drawGraph('gBatt',t.battery,'#d29922');
  }catch(e){}
}

//==================== CONFIG ====================
async function loadSettings(){
  try{const s=await(await fetch('/api/settings')).json();
    $('cfgName').value=s.deviceName||'';$('cfgSsid').value=s.ssid||'';
    $('cfgPass').value=s.password||'';$('cfgTcp').value=s.tcpPort;
    $('cfgSleep').value=s.autoSleepSec;$('cfgLed').value=s.ledBrightness;
    $('cfgRefresh').value=s.dashRefreshMs;$('cfgPoll').value=s.hartPollAddress;
    $('cfgMaster').checked=!!s.masterEnabled;
  }catch(e){}
  try{fillHartHardware(window._status||await(await fetch('/api/status')).json());}catch(e){}
}
async function toggleMaster(){
  const en=$('cfgMaster').checked;
  await fetch('/api/master',{method:'POST',body:new URLSearchParams({enabled:en?1:0})});
  toast('HART master '+(en?'enabled':'disabled'));
}
function fillHartHardware(s){
  if(!s||!$('hartHwTbl'))return;
  $('hartHwTbl').innerHTML=rowsHtml([
    ['AD5700 Power',s.ad5700Powered?'<span class=ok>ON</span>':'<span class=bad>OFF</span>']
  ]);
}
async function saveConfig(){
  const body=new URLSearchParams({deviceName:$('cfgName').value,ssid:$('cfgSsid').value,
    password:$('cfgPass').value,tcpPort:$('cfgTcp').value,autoSleepSec:$('cfgSleep').value,
    ledBrightness:$('cfgLed').value,dashRefreshMs:$('cfgRefresh').value,
    hartPollAddress:$('cfgPoll').value});
  const r=await(await fetch('/api/settings',{method:'POST',body})).json();
  toast(r.message||'Saved');if(r.dashRefreshMs)refreshMs=r.dashRefreshMs;
}
async function doReboot(){if(!await confirmDialog('Reboot',[['Restart now?','']]))return;
  await fetch('/api/reboot',{method:'POST'});toast('Rebooting...');}
async function doFactory(){if(!await confirmDialog('Factory Reset',[['Erase all settings?','']]))return;
  await fetch('/api/factoryreset',{method:'POST'});toast('Factory reset...');}

//==================== PROFILES ====================
async function loadProfiles(){
  try{
    const st=await(await fetch('/api/profile/status')).json();
    prof=await(await fetch('/api/profile/active')).json();
    let h='';
    if(!st.custom){h+='<div class="banner">No matching device profile. Running Generic HART Mode. Upload a profile below.</div>';}
    h+=rowsHtml([
      ['Current Profile',st.active],['Custom',st.custom?'Yes':'No (generic)'],
      ['Manufacturer',prof.manufacturer||'--'],['Device',prof.device||'--'],
      ['Revision',prof.revision==null?'--':prof.revision],['Version',prof.version||'--'],
      ['Author',prof.author||'--'],['Pages',(prof.pages||[]).length],
      ['Storage',(st.fsUsed/1024).toFixed(0)+' / '+(st.fsTotal/1024).toFixed(0)+' KB']
    ]);
    $('profActive').innerHTML=h;
    const list=await(await fetch('/api/profiles')).json();
    $('profList').innerHTML=list.map(p=>{
      const del=p.file==='generic.json'?'':'<button class="btn danger" style="margin-top:8px" onclick="delProfile(\''+p.file+'\')">Delete</button>';
      return '<div class="card" style="background:var(--panel2)"><div class="row"><span class="k">'+
        (p.manufacturer||'?')+' '+(p.device||'')+'</span><span class="v">'+(p.valid?'':'<span class=bad>invalid</span>')+'</span></div>'+
        rowsHtml([['File',p.file],['Revision',p.revision],['Version',p.version||'--'],['Pages',p.pages||0],['Size',p.size+' B']])+del+'</div>';
    }).join('')||'<div class="mut">None.</div>';
  }catch(e){$('profActive').innerHTML='<div class="mut">Profiles unavailable.</div>';}
}
async function uploadProfile(){
  const f=$('profFile').files[0];if(!f)return toast('Choose a file');
  const fd=new FormData();fd.append('profile',f,f.name);
  try{await fetch('/api/profile/upload',{method:'POST',body:fd});
    toast('Uploaded '+f.name);await buildDynNav();loadProfiles();}catch(e){toast('Upload failed');}
}
async function delProfile(file){
  if(!await confirmDialog('Delete Profile',[['File',file]]))return;
  await fetch('/api/profile/delete',{method:'POST',body:new URLSearchParams({file})});
  toast('Deleted');await buildDynNav();loadProfiles();
}

//==================== DYNAMIC PROFILE PAGES ====================
async function buildDynNav(){
  try{prof=await(await fetch('/api/profile/active')).json();}catch(e){prof=null;}
  const nav=$('dynNav');nav.innerHTML='';
  document.querySelectorAll('[id^=page-dyn]').forEach(e=>e.remove());
  if(!prof||!prof.pages||!prof.pages.length)return;
  prof.pages.forEach((pg,i)=>{
    const a=document.createElement('a');a.className='navlink';a.dataset.page='dyn'+i;
    a.textContent=pg.title||('Page '+(i+1));a.onclick=()=>showPage('dyn'+i);nav.appendChild(a);
    const sec=document.createElement('section');sec.className='page';sec.id='page-dyn'+i;
    sec.innerHTML='<h2 class="title">'+(pg.title||'')+'</h2><div class="sub">'+
      (prof.manufacturer||'')+' '+(prof.device||'')+'</div>'+
      '<button class="btn sec" onclick="renderDynPage('+i+')">Refresh</button><div id="dynBody'+i+'"></div>';
    $('page-device').parentNode.appendChild(sec);
  });
}
async function renderDynPage(i){
  if(!prof||!prof.pages||!prof.pages[i])return;
  const body=$('dynBody'+i);if(!body)return;
  const pg=prof.pages[i];let card=null;let html='';
  for(const w of (pg.widgets||[])){html+=await renderWidget(w);}
  body.innerHTML='<div class="card">'+html+'</div>';
}
function decode(bytes,d){
  const o=d.offset||0;
  switch(d.decode){
    case 'float':return rdFloat(bytes,o);
    case 'u8':return bytes[o];
    case 'u16':return rdU16(bytes,o);
    case 'u32':return rdU32(bytes,o);
    case 'ascii':{let s='';for(let k=0;k<(d.len||4);k++)s+=String.fromCharCode(bytes[o+k]||32);return s.trim();}
    case 'packed':return unpackAscii(bytes,o,d.len||6);
    default:return rdFloat(bytes,o);
  }
}
async function renderWidget(w){
  if(w.type==='section')return '<h3 style="margin-top:6px">'+(w.label||'')+'</h3>';
  const id='w'+Math.random().toString(36).slice(2,8);
  if(w.type==='read'||w.type==='value'){
    let val='--';
    try{if(w.read){const r=await hartCmd(w.read.command,w.read.data||'');let v=decode(r.bytes,w.read);
      val=(typeof v==='number')?(isNaN(v)?'--':v.toFixed(w.read.dec==null?3:w.read.dec)):v;}}catch(e){val='<span class=bad>err</span>';}
    return '<div class="row"><span class="k">'+(w.label||'')+'</span><span class="v">'+val+' '+(w.units||'')+'</span></div>';
  }
  if(w.type==='enum'){
    let cur='';try{if(w.read){const r=await hartCmd(w.read.command,'');cur=r.bytes[w.read.offset||0];}}catch(e){}
    let opts=(w.options||[]).map(o=>'<option value="'+o.v+'"'+(o.v==cur?' selected':'')+'>'+o.t+'</option>').join('');
    return '<label class="fl">'+(w.label||'')+'</label><select id="'+id+'">'+opts+'</select>'+
      (w.write?'<button class="btn" onclick="wEnum(\''+id+'\','+w.write.command+')">Write '+(w.label||'')+'</button>':'');
  }
  if(w.type==='number'||w.type==='float'){
    let cur=NaN;try{if(w.read){const r=await hartCmd(w.read.command,'');cur=decode(r.bytes,w.read);}}catch(e){}
    return '<label class="fl">'+(w.label||'')+(w.units?(' ('+w.units+')'):'')+'</label>'+
      '<input id="'+id+'" type="number" step="any" value="'+(isNaN(cur)?'':cur)+'">'+
      (w.write?'<button class="btn" onclick="wNum(\''+id+'\','+w.write.command+',\''+(w.write.encode||'float')+'\',\''+(w.label||'')+'\')">Write</button>':'');
  }
  if(w.type==='button'){
    return '<button class="btn '+(w.style||'sec')+'" onclick="wBtn('+w.command+',\''+(w.data||'')+'\',\''+(w.label||'')+'\','+(w.confirm?'true':'false')+')">'+(w.label||'Send')+'</button>';
  }
  if(w.type==='graph'){
    const cid=id+'c';
    setTimeout(()=>graphWidget(cid,w),50);
    return '<div class="mut" style="font-size:12px">'+(w.label||'')+'</div><canvas id="'+cid+'"></canvas>';
  }
  return '';
}
async function wEnum(id,cmd){const v=parseInt($(id).value);
  if(!await confirmDialog('Write',[['New value',v]]))return;
  try{await hartCmd(cmd,b2hx([v]));toast('Written');}catch(e){toast('Failed: '+e.message);}}
async function wNum(id,cmd,enc,label){const v=parseFloat($(id).value);if(isNaN(v))return toast('Enter value');
  if(!await confirmDialog('Write '+label,[['New',v]]))return;
  let data=enc==='u8'?b2hx([v&255]):enc==='u16'?b2hx([(v>>8)&255,v&255]):b2hx(wrFloat(v));
  try{await hartCmd(cmd,data);toast('Written');}catch(e){toast('Failed: '+e.message);}}
async function wBtn(cmd,data,label,needConfirm){
  if(needConfirm&&!await confirmDialog(label,[['Execute?','']]))return;
  try{await hartCmd(cmd,data);toast(label+' done');}catch(e){toast('Failed: '+e.message);}}
const graphData={};
async function graphWidget(cid,w){
  try{const r=await hartCmd(w.read.command,'');const v=decode(r.bytes,w.read);
    if(!graphData[cid])graphData[cid]=[];graphData[cid].push(v);if(graphData[cid].length>60)graphData[cid].shift();
    drawGraph(cid,graphData[cid],'#4fc3f7');}catch(e){}
}

//==================== boot ====================
async function init(){
  bindNav();
  try{const s=await(await fetch('/api/settings')).json();if(s.dashRefreshMs)refreshMs=s.dashRefreshMs;}catch(e){}
  await buildDynNav();
  await loadStatus();
  showPage('device');
  statusTimer=setInterval(loadStatus,refreshMs);
  devTimer=setInterval(()=>{
    if(curPage==='device')loadDevice();
    else if(curPage==='diag')loadTrends();
    else if(curPage==='monitor')loadMonitor();
  },refreshMs);
}
init();
</script>
</body>
</html>
)HTML";

#endif  // WEB_PAGES_H
