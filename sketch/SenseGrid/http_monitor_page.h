#pragma once
// Source: tools/dashboard/index.html (keep in sync)
static const char k_monitor_html[] = R"SG_MONITOR_HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>SenseGrid Live Monitor</title>
  <style>
    :root { font-family: Arial, sans-serif; color: #0b1620; background: #f5f7fb; }
    body { margin: 0; padding: 16px; }
    h1 { margin: 0 0 12px; }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); gap: 12px; }
    .card { background: #fff; border: 1px solid #d8dde4; border-radius: 8px; padding: 12px; box-shadow: 0 2px 6px rgba(0,0,0,0.05); }
    label { display: block; font-size: 12px; margin-bottom: 4px; color: #555; }
    input { width: 100%; padding: 8px; border: 1px solid #cbd3dc; border-radius: 6px; }
    button { padding: 8px 12px; border: none; border-radius: 6px; background: #1b74e4; color: #fff; cursor: pointer; }
    button.secondary { background: #5f6b7a; }
    pre { background: #0b1620; color: #e5f1ff; padding: 10px; border-radius: 6px; overflow: auto; font-size: 12px; }
    .row { display: flex; gap: 8px; flex-wrap: wrap; align-items: center; margin-bottom: 8px; }
    .badge { display: inline-block; padding: 2px 8px; border-radius: 999px; font-size: 12px; color: #fff; }
    .ok { background: #1f9d55; }
    .warn { background: #d97706; }
    .err { background: #d14343; }
  </style>
</head>
<body>
  <h1>SenseGrid Live Monitor</h1>
  <div class="row">
    <div style="flex:1; min-width:200px;">
      <label>Device IP (HTTP)</label>
      <input id="ip" placeholder="device ip" />
    </div>
    <div style="flex:1; min-width:200px;">
      <label>Poll interval (ms)</label>
      <input id="interval" type="number" value="2000" />
    </div>
    <label style="display:flex; align-items:center; gap:6px; font-size:12px; margin-left:8px;">
      <input type="checkbox" id="fmt" checked />
      Formatar dados
    </label>
    <label style="display:flex; align-items:center; gap:6px; font-size:12px; margin-left:8px;">
      <input type="checkbox" id="pipeToggle" />
      Pipeline on/off
    </label>
    <label style="display:flex; align-items:center; gap:6px; font-size:12px; margin-left:8px;">
      <input type="checkbox" id="apMode" />
      AP (192.168.4.1)
    </label>
    <div class="row">
      <button id="start">Start</button>
      <button id="stop" class="secondary">Stop</button>
      <button id="detect" class="secondary">Detect IP</button>
    </div>
    <div id="status" class="badge warn">idle</div>
  </div>

  <div class="grid">
    <div class="card">
      <h3>Occupancy</h3>
      <div id="occ_state">--</div>
      <div id="occ_conf">conf: --</div>
      <div id="occ_ts">ts: --</div>
    </div>
    <div class="card">
      <h3>Health</h3>
      <div id="health_fw">fw: --</div>
      <div id="health_uptime">uptime: -- s</div>
      <div id="health_rssi">rssi: -- dBm</div>
    </div>
    <div class="card">
      <h3>Meas (raw)</h3>
      <div id="meas_state">state: --</div>
      <div id="meas_dist">dist: -- m</div>
      <div id="meas_snr">snr: --</div>
      <div id="meas_sig">signal: --</div>
      <div id="meas_ts">ts: --</div>
    </div>
  </div>

  <div class="card" style="margin-top:12px;">
    <h3>Last payloads</h3>
    <pre id="log"></pre>
  </div>

  <script>
    const ipEl = document.getElementById('ip');
    const intEl = document.getElementById('interval');
    const fmtEl = document.getElementById('fmt');
    const pipeToggle = document.getElementById('pipeToggle');
    const apMode = document.getElementById('apMode');
    const statusEl = document.getElementById('status');
    const logEl = document.getElementById('log');
    let timer = null;
    let lastOcc = null, lastHealth = null, lastMeas = null;
    const host = window.location.hostname;
    if (host) {
      ipEl.value = host;
    }
    let lastStaIp = ipEl.value;

    function setStatus(text, cls) {
      statusEl.textContent = text;
      statusEl.className = 'badge ' + (cls || 'warn');
    }

    function fmt(ts) { return ts ? new Date(ts).toISOString() : '--'; }

    async function fetchJson(path, hostOverride) {
      const ip = (hostOverride || ipEl.value || "").trim();
      const url = `http://${ip}${path}`;
      const res = await fetch(url, { cache: 'no-cache' });
      if (!res.ok) throw new Error(res.statusText);
      return res.json();
    }

    async function pollOnce() {
      try {
        const [occ, health, meas, pipe] = await Promise.all([
          fetchJson('/v1/occupancy'),
          fetchJson('/v1/health'),
          fetchJson('/v1/meas'),
          fetchJson('/v1/pipe'),
        ]);
        lastOcc = occ; lastHealth = health; lastMeas = meas;
        renderOcc(occ);
        renderHealth(health);
        renderMeas(meas);
        if (pipe && typeof pipe.enabled === 'boolean') {
          pipeToggle.checked = pipe.enabled;
        }
        appendLog({ occ, health, meas });
        setStatus('ok', 'ok');
      } catch (e) {
        setStatus('error', 'err');
        appendLog({ error: e.message });
      }
    }

    function renderOcc(o) {
      if (!o) return;
      if (!fmtEl.checked) {
        document.getElementById('occ_state').textContent = JSON.stringify(o);
        document.getElementById('occ_conf').textContent = '';
        document.getElementById('occ_ts').textContent = '';
        return;
      }
      const p = o.payload || {};
      document.getElementById('occ_state').textContent = `state: ${p.count ?? '--'}`;
      document.getElementById('occ_conf').textContent = `conf: ${p.confidence ?? '--'}`;
      document.getElementById('occ_ts').textContent = `ts: ${fmt(o.ts_ms)}`;
    }

    function renderHealth(h) {
      if (!h) return;
      if (!fmtEl.checked) {
        document.getElementById('health_fw').textContent = JSON.stringify(h);
        document.getElementById('health_uptime').textContent = '';
        document.getElementById('health_rssi').textContent = '';
        return;
      }
      const p = h.payload || {};
      document.getElementById('health_fw').textContent = `fw: ${p.fw ?? '--'}`;
      document.getElementById('health_uptime').textContent = `uptime: ${p.uptime_s ?? '--'} s`;
      document.getElementById('health_rssi').textContent = `rssi: ${p.rssi_dbm ?? '--'} dBm`;
    }

    function renderMeas(m) {
      if (!m) return;
      if (!fmtEl.checked) {
        document.getElementById('meas_state').textContent = JSON.stringify(m);
        document.getElementById('meas_dist').textContent = '';
        document.getElementById('meas_snr').textContent = '';
        document.getElementById('meas_sig').textContent = '';
        document.getElementById('meas_ts').textContent = '';
        return;
      }
      const p = m.payload || m; // se endpoint retornar payload enxuto ou bruto
      const rawStatus = p.status ?? '--';
      document.getElementById('meas_state').textContent = `state: ${p.state ?? '--'} (stable ${p.stable ?? '--'}) raw: ${rawStatus}`;
      document.getElementById('meas_dist').textContent = `dist: ${p.dist_m ?? '--'} m`;
      document.getElementById('meas_snr').textContent = `snr: ${p.snr ?? '--'}`;
      document.getElementById('meas_sig').textContent = `signal: ${p.signal ?? '--'}`;
      document.getElementById('meas_ts').textContent = `ts: ${fmt(m.ts_ms ?? p.ts_ms)}`;
    }

    function appendLog(obj) {
      const line = JSON.stringify(obj);
      logEl.textContent = line + "\n" + logEl.textContent;
      if (logEl.textContent.length > 4000) {
        logEl.textContent = logEl.textContent.slice(0, 4000);
      }
    }

    document.getElementById('start').onclick = () => {
      const ms = Math.max(500, Number(intEl.value) || 2000);
      clearInterval(timer);
      pollOnce();
      timer = setInterval(pollOnce, ms);
      setStatus('polling', 'warn');
    };
    document.getElementById('stop').onclick = () => {
      clearInterval(timer);
      timer = null;
      setStatus('stopped', 'warn');
    };
    fmtEl.onchange = () => {
      // re-render com ultimo estado
      renderOcc(lastOcc);
      renderHealth(lastHealth);
      renderMeas(lastMeas);
    };
    apMode.onchange = () => {
      if (apMode.checked) {
        lastStaIp = ipEl.value.trim() || lastStaIp;
        ipEl.value = "192.168.4.1";
      } else {
        ipEl.value = lastStaIp || "192.168.15.11";
      }
    };
    pipeToggle.onchange = async () => {
      try {
        const state = pipeToggle.checked ? 'on' : 'off';
        await fetchJson(`/v1/pipe?state=${state}`);
      } catch (e) {
        setStatus('error', 'err');
        appendLog({ error: e.message });
      }
    };
    document.getElementById('detect').onclick = async () => {
      // tenta current IP, se falhar tenta 192.168.4.1 (AP)
      const candidates = [];
      if (ipEl.value) candidates.push(ipEl.value.trim());
      if (!candidates.includes("192.168.4.1")) candidates.push("192.168.4.1");
      for (const host of candidates) {
        try {
          const info = await fetchJson('/v1/info', host);
          if (info && info.sta_ip) {
            ipEl.value = info.sta_ip;
            setStatus(`detected ${info.sta_ip}`, 'ok');
            return;
          }
          if (info && info.ap_ip) {
            ipEl.value = info.ap_ip;
            setStatus(`detected ${info.ap_ip}`, 'ok');
            return;
          }
        } catch (e) {
          // tenta proximo
        }
      }
      setStatus('detect failed', 'err');
    };
  </script>
</body>
</html>

)SG_MONITOR_HTML";
