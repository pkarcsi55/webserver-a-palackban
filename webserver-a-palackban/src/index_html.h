// index_html.h – állítható mintavétel + egyszerűsített export + relatív idő

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="hu">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>BMP280 – Mérőrendszer</title>
<style>
  :root { --bg:#0c0f14; --card:#151a22; --fg:#e9eef7; --muted:#9aa7b8; }
  * { box-sizing:border-box; }
  body {
    margin:0; font-family:system-ui,-apple-system,"Segoe UI",Roboto,Arial,sans-serif;
    background:var(--bg); color:var(--fg); display:flex; min-height:100vh; 
    align-items:center; justify-content:center; padding:24px;
  }
  .wrap { width:min(1000px,100%); }
  h1 { font-size:1.4rem; margin:0 0 16px; font-weight:600; letter-spacing:.2px; }
  .grid { display:grid; grid-template-columns:1fr 1fr 1fr; gap:16px; }
  .card {
    background:var(--card); border:1px solid #202734; border-radius:16px; padding:24px;
    box-shadow:0 10px 30px rgba(0,0,0,.2);
  }
  .label { font-size:.9rem; color:var(--muted); margin-bottom:8px; }
  .value { font-size:2.4rem; font-weight:700; line-height:1.1; }
  .unit  { font-size:1.1rem; color:var(--muted); margin-left:.35rem; }
  .row   { display:flex; align-items:flex-end; gap:.3rem; flex-wrap:wrap; }
  .footer { margin-top:16px; color:var(--muted); font-size:.9rem; display:flex; gap:8px; align-items:center; flex-wrap:wrap; }
  button {
    background:#2a3342; color:var(--fg); border:none; padding:10px 14px; border-radius:10px; cursor:pointer;
  }
  button.reset { background:#b71c1c; }
  #history {
    margin-top:20px; background:var(--card); border-radius:12px; padding:16px; 
    max-height:300px; overflow-y:auto; font-size:.9rem;
  }
  #history ul { list-style:none; padding:0; margin:0; }
  #history li { padding:4px 0; border-bottom:1px solid #202734; }
  #history li:last-child { border-bottom:none; }
  .tools { margin-top:8px; display:flex; gap:8px; }

  #toast {
    position:fixed; bottom:20px; right:20px;
    background:#2e7d32; color:#fff; padding:10px 16px; border-radius:8px;
    box-shadow:0 4px 12px rgba(0,0,0,.3);
    font-size:.9rem; opacity:0; pointer-events:none; transition:opacity .3s ease;
  }
  #toast.error { background:#c62828; }
  #toast.show { opacity:1; }
</style>
</head>
<body>
  <div class="wrap">
    <h1>ESP32 + BMP280 – Mérőrendszer</h1>
    <div class="label" style="margin-bottom:12px;">
      IP cím: <b>{{IP}}</b>
      <a href="https://api.qrserver.com/v1/create-qr-code/?size=100x100&data=http://{{IP}}" 
        target="_blank" 
        style="margin-left:12px;color:#9aa7b8;text-decoration:none;">📱 QR</a>
    </div>
    <div class="grid">
      <div class="card"><div class="label">Hőmérséklet</div><div class="row"><div id="tC" class="value">–.–</div><div class="unit">°C</div><div id="tK" class="value" style="font-size:1.6rem;">(–.– K)</div></div></div>
      <div class="card"><div class="label">Nyomás</div><div class="row"><div id="p" class="value">–.–</div><div class="unit">hPa</div></div></div>
      <div class="card"><div class="label">Akkumulátor</div><div class="row"><div id="uPercent" class="value">–%</div><div id="uVolt" class="unit">(–.– V)</div></div></div>
    </div>

    <div class="footer">
      <span id="status">Frissítés…</span>
      <input id="rateInput" type="number" min="200" max="10000" step="200" value="1000" style="width:90px;">
      <button id="setrate">Mintavétel (ms)</button>
      <button id="reset" class="reset">Új mérés indítása</button>
    </div>

    <div id="history">
      <div class="label">Utolsó 100 mérés</div>
      <div class="tools"><button id="copybtn">Másolás vágólapra</button></div>
      <ul id="histlist"></ul>
    </div>
  </div>
  <div id="toast"></div>

<script>
const tCEl=document.getElementById('tC');
const tKEl=document.getElementById('tK');
const pEl=document.getElementById('p');
const uVoltEl=document.getElementById('uVolt');
const uPercentEl=document.getElementById('uPercent');
const statusEl=document.getElementById('status');
const histList=document.getElementById('histlist');
const copyBtn=document.getElementById('copybtn');
const resetBtn=document.getElementById('reset');
const setRateBtn=document.getElementById('setrate');
const rateInput=document.getElementById('rateInput');
const toast=document.getElementById('toast');

let lastHistory=[];
let startTime=null;

function showToast(msg,isError=false){
  toast.textContent=msg;
  toast.className=isError?"error show":"show";
  setTimeout(()=>{toast.className=toast.className.replace("show","");},3000);
}

async function fetchData(){
 try{
  const r=await fetch('/api/sensor',{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const j=await r.json();
  if(j.ok){
    tCEl.textContent=j.T_C.toFixed(2).replace(".",",");
    tKEl.textContent="("+j.T_K.toFixed(2).replace(".",",")+" K)";
    pEl.textContent=j.P.toFixed(1).replace(".",",");
    uPercentEl.textContent=j.Upercent+"%";
    uVoltEl.textContent="("+j.U.toFixed(2).replace(".",",")+" V)";
    if(startTime===null) startTime=j.t;
    const elapsed=Math.floor((j.t-startTime)/1000);
    const h=Math.floor(elapsed/3600);
    const m=Math.floor((elapsed%3600)/60);
    const s=elapsed%60;
    const relTime=`${h}:${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`;
    statusEl.textContent='Idő: '+relTime;
  } else statusEl.textContent='Hiba: '+(j.err||'ismeretlen');
 }catch(e){statusEl.textContent='Nincs kapcsolat: '+e.message;}
}

async function fetchHistory(){
 try{
  const r=await fetch('/api/history',{cache:'no-store'});
  if(!r.ok)throw new Error('HTTP '+r.status);
  const arr=await r.json(); lastHistory=arr; histList.innerHTML='';
  if(arr.length>0 && startTime===null){ startTime = arr[arr.length-1].t; }
  arr.forEach(dp=>{
    const elapsed = Math.floor((dp.t - startTime)/1000);
    const h = Math.floor(elapsed/3600);
    const m = Math.floor((elapsed%3600)/60);
    const s = elapsed%60;
    const relTime = `${h}:${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`;
    const li=document.createElement('li');
    li.textContent=`[${relTime}] P=${dp.P.toFixed(1).replace(".",",")} hPa, T=${dp.T_K.toFixed(2).replace(".",",")} K`;
    histList.appendChild(li);
  });
 }catch(e){histList.innerHTML='<li>Hiba: '+e.message+'</li>';}
}

async function resetHistory(){
 try{
  const r=await fetch('/api/reset');
  if(!r.ok)throw new Error('HTTP '+r.status);
  showToast("Új mérési sorozat indult!");
  histList.innerHTML=''; lastHistory=[]; startTime=null;
 }catch(e){showToast("Reset hiba: "+e.message,true);}
}

setRateBtn.addEventListener('click',async()=>{
 const val=parseInt(rateInput.value);
 if(isNaN(val)||val<200||val>10000){showToast("Érvénytelen érték (200–10000 ms)!",true);return;}
 try{
   const r=await fetch('/api/setrate?interval='+val);
   if(!r.ok)throw new Error('HTTP '+r.status);
   showToast("Mintavételi idő beállítva: "+val+" ms");
 }catch(e){showToast("Beállítás sikertelen: "+e.message,true);}
});

copyBtn.addEventListener('click',()=>{
 if(!lastHistory.length){showToast("Nincs adat a másoláshoz!",true);return;}
 if(startTime===null && lastHistory.length>0){ startTime = lastHistory[lastHistory.length-1].t; }
 let text="time\tP[hPa]\tT[°C]\tT[K]\n";
 lastHistory.slice().reverse().forEach(dp=>{
   const elapsed=Math.floor((dp.t-startTime)/1000);
   const h=Math.floor(elapsed/3600);
   const m=Math.floor((elapsed%3600)/60);
   const s=elapsed%60;
   const relTime=`${h}:${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`;
   const P=dp.P.toFixed(1).replace(".",",");
   const TC=dp.T_C.toFixed(2).replace(".",",");
   const TK=dp.T_K.toFixed(2).replace(".",",");
   text+=`${relTime}\t${P}\t${TC}\t${TK}\n`;
 });
 const ta=document.createElement("textarea");
 ta.value=text; document.body.appendChild(ta); ta.select();
 try{document.execCommand("copy");showToast("Kimásolva a vágólapra (Excel)");}
 catch(e){showToast("Nem sikerült a másolás!",true);}
 document.body.removeChild(ta);
});

resetBtn.addEventListener('click',resetHistory);

setInterval(()=>{fetchData();fetchHistory();},1000);
fetchData();fetchHistory();
</script>
</body>
</html>
)rawliteral";
