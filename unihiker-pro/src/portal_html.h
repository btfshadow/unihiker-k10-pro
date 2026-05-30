// Minimal captive portal HTML for configuration
static const char kPortalHtml[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>UniHiker Admin</title>
<style>
  :root{--gap:10px;--card:#fff;--muted:#666}
  body{font-family:system-ui,Arial,Helvetica,sans-serif;margin:12px;background:#f4f6f8}
  header{display:flex;align-items:center;justify-content:space-between}
  h1{font-size:1.1rem;margin:0}
  .grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:var(--gap);margin-top:12px}
  .card{background:var(--card);padding:12px;border-radius:8px;box-shadow:0 1px 2px rgba(0,0,0,0.06)}
  label{display:block;font-size:0.9rem;margin-bottom:6px}
  input,select,button{padding:8px;margin-top:4px;width:100%;box-sizing:border-box}
  .muted{color:var(--muted);font-size:0.9rem}
  .row{display:flex;gap:8px}
  .row>button{flex:1}
  pre{background:#eef2f6;padding:8px;border-radius:6px}
  .small{font-size:0.85rem;color:var(--muted)}
  .top-actions{display:flex;gap:8px}
  @media(min-width:640px){h1{font-size:1.2rem}}
</style>
</head>
<body>
<header>
  <h1>UniHiker Admin</h1>
  <div class="top-actions">
    <button id="logoutBtn" style="display:none">Logout</button>
  </div>
</header>
<div id="status" class="small muted">loading...</div>

<div id="login" class="card" style="margin-top:12px;display:none">
  <h2>Login</h2>
  <label>Password</label>
  <input id="loginPwd" type="password" placeholder="portal password" />
  <div class="row" style="margin-top:8px">
    <button id="loginBtn">Login</button>
  </div>
  <div class="small muted" style="margin-top:8px">The portal password protects control endpoints.</div>
</div>

<div id="main" style="display:none">
  <div class="grid">
    <div class="card">
      <h3>Camera</h3>
      <div class="row"><button id="camStart">Start Preview</button><button id="camStop">Stop</button></div>
      <div class="small muted" style="margin-top:8px">Starts/stops camera preview on device.</div>
    </div>

    <div class="card">
      <h3>Audio</h3>
      <label>Record seconds</label>
      <input id="recSec" type="number" value="5" />
      <button id="recBtn" style="margin-top:8px">Record</button>
      <div id="recOut" class="small muted" style="margin-top:8px"></div>
    </div>

    <div class="card">
      <h3>Sensors</h3>
      <button id="sensorsBtn">Refresh</button>
      <pre id="sensorsOut">-</pre>
    </div>

    <div class="card">
      <h3>Wi‑Fi</h3>
      <div class="row"><button id="scanBtn">Scan</button><button id="connectBtn">Connect</button></div>
      <label>SSID</label>
      <select id="ssidSelect"></select>
      <label>Password</label>
      <input id="wifiPass" type="password" />
      <div id="wifiOut" class="small muted" style="margin-top:8px"></div>
    </div>

    <div class="card">
      <h3>Settings</h3>
      <label>Portal password (leave blank = no auth)</label>
      <input id="portalPwd" />
      <label>AI Provider</label>
      <input id="aiProvider" />
      <label>AI Model</label>
      <input id="aiModel" />
      <label>API Key</label>
      <input id="apiKey" />
      <label>Descanso tela (segundos)</label>
      <input id="screenSaver" type="number" min="5" />
      <label>Sleep (segundos)</label>
      <input id="sleepTimeout" type="number" min="5" />
      <label>Deep Sleep (segundos)</label>
      <input id="deepSleep" type="number" min="5" />
      <button id="saveSettings" style="margin-top:8px">Save</button>
      <div id="settingsOut" class="small muted" style="margin-top:8px"></div>
    </div>
  </div>

  <div style="margin-top:12px" class="card">
    <h3>Raw</h3>
    <pre id="out">-</pre>
  </div>
</div>

<script>
const opts = {credentials:'include'};
function id(v){return document.getElementById(v)}
function setStatus(t){ id('status').innerText = t }

async function api(path, method='GET', body=null){
  const h = { ...opts };
  const o = { method };
  if (body){ o.body = body; o.headers = {'Content-Type':'application/json'}; }
  Object.assign(o, opts);
  const res = await fetch(path, o);
  if (res.headers.get('content-type') && res.headers.get('content-type').includes('application/json')) return res.json();
  return res.text();
}

async function refreshStatus(){
  try{
    const st = await api('/api/status');
    if(st.connected){ setStatus('Connected: '+st.ssid+' — IP: '+st.ip); }
    else if(st.apSsid){ setStatus('AP: '+st.apSsid); }
    else setStatus('offline');
    // auth handling
    if(st.authRequired && !st.authenticated){ id('login').style.display='block'; id('main').style.display='none'; id('logoutBtn').style.display='none'; }
    else { id('login').style.display='none'; id('main').style.display='block'; id('logoutBtn').style.display='inline-block'; }
    // populate settings fields if available
    if(st.aiProvider) id('aiProvider').value = st.aiProvider;
    if(st.aiModel) id('aiModel').value = st.aiModel;
    if(st.apiKey) id('apiKey').value = st.apiKey;
    if(typeof st.screenSaver !== 'undefined') id('screenSaver').value = st.screenSaver;
    if(typeof st.sleep !== 'undefined') id('sleepTimeout').value = st.sleep;
    if(typeof st.deepSleep !== 'undefined') id('deepSleep').value = st.deepSleep;
  }catch(e){ setStatus('status error'); }
}

async function doLogin(){
  const pw = id('loginPwd').value||'';
  const res = await fetch('/api/login', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({password: pw}), credentials:'include'});
  if (res.status === 200) { await refreshStatus(); id('loginPwd').value=''; }
  else { setStatus('login failed'); }
}

async function doLogout(){ await api('/api/logout','POST'); await refreshStatus(); }

async function doScan(){ id('ssidSelect').innerHTML=''; setStatus('scanning...');
  try{ const resp = await api('/api/wifi/scan'); if(resp.networks){ resp.networks.forEach(n=>{ const opt=document.createElement('option'); opt.value=n.ssid; opt.text=n.ssid+' ('+n.rssi+'dBm)'; id('ssidSelect').appendChild(opt); }); setStatus('scan ok'); } }
  catch(e){ setStatus('scan error'); }
}

async function doConnect(){ const ssid=id('ssidSelect').value; const pass=id('wifiPass').value||''; const body = JSON.stringify({ssid:ssid,password:pass}); const res = await fetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/json'},body,credentials:'include'}); id('out').innerText = await res.text(); setTimeout(refreshStatus,1200); }

async function camStart(){ const r = await api('/api/camera/start','POST',null); id('out').innerText = JSON.stringify(r); }
async function camStop(){ const r = await api('/api/camera/stop','POST',null); id('out').innerText = JSON.stringify(r); }

async function doRecord(){ const s = parseInt(id('recSec').value)||5; const r = await fetch('/api/audio/record',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({seconds:s}),credentials:'include'}); const j = await r.json(); id('recOut').innerText = JSON.stringify(j); }

async function sensorsRefresh(){ const j = await api('/api/sensors'); id('sensorsOut').innerText = JSON.stringify(j, null, 2); }

async function saveSettings(){ 
  const payload = { 
    portalPwd: id('portalPwd').value || '',
    aiProvider: id('aiProvider').value||'',
    aiModel: id('aiModel').value||'',
    apiKey: id('apiKey').value||'',
    screenSaver: id('screenSaver').value || '',
    sleep: id('sleepTimeout').value || '',
    deepSleep: id('deepSleep').value || ''
  };
  const res = await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload),credentials:'include'});
  id('settingsOut').innerText = await res.text(); setTimeout(refreshStatus,600); 
}

document.addEventListener('DOMContentLoaded', ()=>{
  id('loginBtn').onclick = doLogin;
  id('logoutBtn').onclick = doLogout;
  id('scanBtn').onclick = doScan;
  id('connectBtn').onclick = doConnect;
  id('camStart').onclick = camStart;
  id('camStop').onclick = camStop;
  id('recBtn').onclick = doRecord;
  id('sensorsBtn').onclick = sensorsRefresh;
  id('saveSettings').onclick = saveSettings;
  refreshStatus();
});
</script>
</body>
</html>




)rawliteral";
