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
    
    <div class="card">
      <h3>AI Providers</h3>
      <label>Providers</label>
      <select id="providersList" style="margin-bottom:8px"></select>
      <div class="row">
        <button id="newProvider">New</button>
        <button id="deleteProvider">Delete</button>
      </div>
      <label>Id</label>
      <input id="provId" readonly />
      <label>Name</label>
      <input id="provName" />
      <label>Type</label>
      <select id="provType"><option value="openai">OpenAI</option><option value="anthropic">Anthropic</option><option value="openrouter">OpenRouter</option><option value="hermes">Hermes</option><option value="ollama">Ollama</option><option value="offline">Offline</option><option value="custom">Custom</option></select>
      <label>Base URL</label>
      <input id="provBase" />
      <label>Model</label>
      <input id="provModel" />
      <label>Header Name</label>
      <input id="provHeader" placeholder="e.g. Authorization or x-api-key" />
      <label>API Key (leave blank to keep existing)</label>
      <input id="provApiKey" type="password" />
      <label>Params (JSON)</label>
      <input id="provParams" />
      <div class="row" style="margin-top:8px"><button id="saveProvider">Save</button><button id="activateProvider">Activate</button><button id="testProvider">Test</button></div>
      <label>Agent Payload (raw JSON)</label>
      <textarea id="agentPayload" style="width:100%;height:80px;margin-top:4px"></textarea>
      <label>Path (optional)</label>
      <input id="agentPath" placeholder="/run or /v1/agent" />
      <div class="row" style="margin-top:8px"><button id="runAgent">Run Agent</button></div>
      <div id="providerOut" class="small muted" style="margin-top:8px"></div>
    </div>

    <div class="card">
      <h3>Prompts (SD)</h3>
      <div class="small muted">Place a <code>manifest.json</code> under <code>S:/ai-prompts/manifest.json</code></div>
      <pre id="promptsOut">-</pre>
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

// AI providers UI
async function loadAiProviders(){
  try{
    const j = await api('/api/ai/providers');
    const sel = id('providersList'); sel.innerHTML='';
    if (j && j.providers){
      j.providers.forEach(p=>{
        const opt = document.createElement('option'); opt.value = p.id; opt.text = (p.name||p.id) + ' ('+p.type+')'; sel.appendChild(opt);
      });
      if (j.active){ sel.value = j.active; }
      // populate form with selected or first
      const selVal = sel.value || (j.providers[0] && j.providers[0].id) || '';
      const prov = j.providers.find(x=>x.id===selVal) || j.providers[0] || null;
      if (prov) populateProviderForm(prov); else clearProviderForm();
    } else { clearProviderForm(); }
  }catch(e){ id('providerOut').innerText = 'load error'; }
}

function populateProviderForm(p){ id('provId').value = p.id||''; id('provName').value = p.name||''; id('provType').value = p.type||''; id('provBase').value = p.base||''; id('provModel').value = p.model||''; id('provHeader').value = p.header||''; id('provParams').value = p.params||''; id('provApiKey').value=''; id('providerOut').innerText = p.apiKeyMasked ? ('masked: '+p.apiKeyMasked) : ''; }
function clearProviderForm(){ id('provId').value=''; id('provName').value=''; id('provType').value=''; id('provBase').value=''; id('provModel').value=''; id('provHeader').value=''; id('provParams').value=''; id('provApiKey').value=''; id('providerOut').innerText=''; }

async function saveProvider(){
  const payload = {
    id: id('provId').value||'', name: id('provName').value||'', type: id('provType').value||'', base: id('provBase').value||'', model: id('provModel').value||'', header: id('provHeader').value||'', params: id('provParams').value||'', apiKey: id('provApiKey').value||''
  };
  const r = await fetch('/api/ai/provider',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload),credentials:'include'});
  const txt = await r.text(); id('providerOut').innerText = txt; await loadAiProviders();
}

async function deleteProvider(){ const idv = id('providersList').value; if(!idv) return; if(!confirm('Delete provider '+idv+' ?')) return; const r = await fetch('/api/ai/provider/delete',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:idv}),credentials:'include'}); id('providerOut').innerText = await r.text(); await loadAiProviders(); }

async function activateProvider(){ const idv = id('providersList').value; if(!idv) return; const r = await fetch('/api/ai/provider/activate',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:idv}),credentials:'include'}); id('providerOut').innerText = await r.text(); await loadAiProviders(); }

async function testProvider(){ const idv = id('providersList').value; if(!idv) return; id('providerOut').innerText = 'testing...'; const r = await fetch('/api/ai/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:idv}),credentials:'include'}); const j = await r.json(); id('providerOut').innerText = JSON.stringify(j, null, 2); }
async function runAgent(){ const idv = id('providersList').value; if(!idv) return; let payload = id('agentPayload').value||''; try{ JSON.parse(payload); }catch(e){ id('providerOut').innerText = 'invalid JSON payload'; return; } id('providerOut').innerText = 'running...'; const path = id('agentPath').value||''; const url = '/api/ai/agent?id='+encodeURIComponent(idv)+(path?('&path='+encodeURIComponent(path)):('')); const r = await fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:payload,credentials:'include'}); const j = await r.json(); id('providerOut').innerText = JSON.stringify(j, null, 2); }

async function loadAiPrompts(){ try{ const r = await fetch('/api/ai/prompts'); if (r.status===200){ const txt = await r.text(); id('promptsOut').innerText = txt; } else { id('promptsOut').innerText = 'no manifest'; } }catch(e){ id('promptsOut').innerText='error'; } }

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
  id('providersList').onchange = loadAiProviders;
  id('newProvider').onclick = function(){ clearProviderForm(); id('provId').value = 'prov'+Date.now(); };
  id('deleteProvider').onclick = deleteProvider;
  id('saveProvider').onclick = saveProvider;
  id('activateProvider').onclick = activateProvider;
  id('testProvider').onclick = testProvider;
  id('runAgent').onclick = runAgent;
  refreshStatus();
  loadAiProviders();
  loadAiPrompts();
});
</script>
</body>
</html>




)rawliteral";
