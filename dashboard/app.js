const $ = (id) => document.getElementById(id);
const palette = { grid: '#263449', text: '#8ea0b8', cyan: '#55d6ff', green: '#53e0b0', red: '#ff7b8a' };
let currentData = null;
const money = (v) => Number(v).toLocaleString(undefined, {minimumFractionDigits:2, maximumFractionDigits:4});
const pct = (v) => (v * 100).toFixed(3) + '%';

function drawLineChart(canvas, points, xKey, yKey, opts={}) {
  const rect = canvas.getBoundingClientRect();
  if (!rect.width || !rect.height || !points?.length) return;
  const dpr = window.devicePixelRatio || 1;
  canvas.width = Math.round(rect.width * dpr); canvas.height = Math.round(rect.height * dpr);
  const ctx = canvas.getContext('2d'); ctx.scale(dpr,dpr);
  const w=rect.width,h=rect.height,p={l:48,r:12,t:12,b:31};
  const xs=points.map(d=>d[xKey]), ys=points.map(d=>d[yKey]);
  let xmin=Math.min(...xs), xmax=Math.max(...xs), ymin=Math.min(...ys), ymax=Math.max(...ys);
  if (ymin===ymax) {ymin-=1;ymax+=1;} const pad=(ymax-ymin)*.12; ymin-=pad;ymax+=pad;
  const X=x=>p.l+(x-xmin)/(xmax-xmin||1)*(w-p.l-p.r);
  const Y=y=>h-p.b-(y-ymin)/(ymax-ymin)*(h-p.t-p.b);
  ctx.font='10px system-ui';ctx.lineWidth=1;ctx.strokeStyle=palette.grid;ctx.fillStyle=palette.text;
  for(let i=0;i<5;i++){const y=p.t+i*(h-p.t-p.b)/4;ctx.beginPath();ctx.moveTo(p.l,y);ctx.lineTo(w-p.r,y);ctx.stroke();const val=ymax-i*(ymax-ymin)/4;ctx.fillText(opts.yFormat?opts.yFormat(val):val.toFixed(1),2,y+3);}
  for(let i=0;i<5;i++){const x=p.l+i*(w-p.l-p.r)/4;ctx.beginPath();ctx.moveTo(x,p.t);ctx.lineTo(x,h-p.b);ctx.stroke();const val=xmin+i*(xmax-xmin)/4;ctx.fillText(opts.xFormat?opts.xFormat(val):val.toFixed(2),Math.max(0,x-17),h-8);}
  if(opts.zeroLine && ymin<0 && ymax>0){ctx.strokeStyle='#5b6a80';ctx.setLineDash([4,4]);ctx.beginPath();ctx.moveTo(p.l,Y(0));ctx.lineTo(w-p.r,Y(0));ctx.stroke();ctx.setLineDash([]);}
  ctx.strokeStyle=opts.color||palette.cyan;ctx.lineWidth=2.5;ctx.beginPath();points.forEach((d,i)=>{const x=X(d[xKey]),y=Y(d[yKey]);if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);});ctx.stroke();
  points.forEach(d=>{ctx.beginPath();ctx.arc(X(d[xKey]),Y(d[yKey]),2.2,0,Math.PI*2);ctx.fillStyle=opts.pointColor?opts.pointColor(d):opts.color||palette.cyan;ctx.fill();});
}
function drawStress(canvas, points) {
  const rect=canvas.getBoundingClientRect();if(!rect.width||!rect.height||!points?.length)return;
  const dpr=window.devicePixelRatio||1;canvas.width=Math.round(rect.width*dpr);canvas.height=Math.round(rect.height*dpr);
  const ctx=canvas.getContext('2d');ctx.scale(dpr,dpr);const w=rect.width,h=rect.height,p={l:42,r:10,t:12,b:42};
  const vals=points.map(p=>p.pnl);let min=Math.min(0,...vals),max=Math.max(0,...vals);if(min===max){min=-1;max=1;}const pad=(max-min)*.14;min-=pad;max+=pad;
  const Y=v=>h-p.b-(v-min)/(max-min)*(h-p.t-p.b);const band=(w-p.l-p.r)/points.length;ctx.font='10px system-ui';ctx.fillStyle=palette.text;ctx.strokeStyle=palette.grid;
  for(let i=0;i<5;i++){const y=p.t+i*(h-p.t-p.b)/4;ctx.beginPath();ctx.moveTo(p.l,y);ctx.lineTo(w-p.r,y);ctx.stroke();ctx.fillText(money(max-i*(max-min)/4),2,y+3);}
  const y0=Y(0);ctx.strokeStyle='#64748b';ctx.setLineDash([4,4]);ctx.beginPath();ctx.moveTo(p.l,y0);ctx.lineTo(w-p.r,y0);ctx.stroke();ctx.setLineDash([]);
  points.forEach((d,i)=>{const bh=Math.abs(Y(d.pnl)-y0);ctx.fillStyle=d.pnl>=0?palette.green:palette.red;const x=p.l+i*band+band*.22;ctx.fillRect(x,Math.min(y0,Y(d.pnl)),band*.56,Math.max(1,bh));ctx.save();ctx.translate(x+band*.28,h-p.b+10);ctx.rotate(-.42);ctx.fillStyle=palette.text;ctx.fillText(d.scenario.replace('Rates ',''),0,0);ctx.restore();});
}
function renderAnalytics(data) {
  currentData=data;
  $('price').textContent=money(data.analytics.price);$('duration').textContent=Number(data.analytics.modifiedDuration).toFixed(4)+' yr';$('convexity').textContent=Number(data.analytics.convexity).toFixed(4);$('dv01').textContent=money(data.analytics.dv01);
  $('calcState').textContent='Calculated by C++';$('calcState').classList.remove('loading');$('calcError').style.display='none';
  drawLineChart($('priceChart'),data.priceYield,'yield','price',{xFormat:v=>(v*100).toFixed(1)+'%',yFormat:money});
  drawStress($('stressChart'),data.stress);
  $('scenarioRows').innerHTML=data.stress.map(s=>`<tr><td>${s.scenario}</td><td class="num">${s.yieldShift===0?'0 bp':(s.yieldShift*10000>0?'+':'')+(s.yieldShift*10000).toFixed(0)+' bp'}</td><td class="num">${money(s.price)}</td><td class="num ${s.pnl>=0?'positive':'negative'}">${s.pnl>0?'+':''}${money(s.pnl)}</td></tr>`).join('');
}
async function recalculate(){const button=$('recalculate');button.disabled=true;$('calcState').textContent='Calculating…';$('calcState').classList.add('loading');$('calcError').style.display='none';
  try{const n=Number($('notional').value),c=Number($('coupon').value)/100,y=Number($('ytm').value)/100;if(!Number.isFinite(n)||!Number.isFinite(c)||!Number.isFinite(y))throw Error('Enter valid numeric inputs.');
    const query=new URLSearchParams({notional:String(n),coupon:String(c),ytm:String(y)});const response=await fetch('/api/analytics?'+query);const data=await response.json();if(!response.ok)throw Error(data.error||'Analytics request failed.');renderAnalytics(data);
  }catch(e){$('calcState').textContent='Calculation unavailable';$('calcError').textContent=e.message;$('calcError').style.display='block';}finally{button.disabled=false;}}
function drawSonarBars(canvas, entries, opts={}) {
  const rect=canvas.getBoundingClientRect(); if(!rect.width||!rect.height)return;
  const dpr=window.devicePixelRatio||1;canvas.width=Math.round(rect.width*dpr);canvas.height=Math.round(rect.height*dpr);
  const ctx=canvas.getContext('2d');ctx.scale(dpr,dpr);const w=rect.width,h=rect.height;
  ctx.clearRect(0,0,w,h);const left=opts.percent?112:118,right=opts.percent?38:24,top=12,row=31;
  const max=Math.max(1,...entries.map(e=>e.value));ctx.font='11px system-ui';
  entries.forEach((e,i)=>{const y=top+i*row;ctx.fillStyle=palette.text;ctx.fillText(e.label,0,y+12);const x=left,available=Math.max(10,w-left-right),barW=Math.max(0,available*(e.value/max));ctx.fillStyle=palette.grid;ctx.fillRect(x,y,available,14);ctx.fillStyle=e.color||palette.cyan;ctx.fillRect(x,y,barW,14);ctx.fillStyle='#e8eef8';const label=opts.percent?`${Number(e.value).toFixed(2)}%`:String(e.value);ctx.fillText(label,Math.min(w-right+4,x+barW+5),y+12);});
}
function renderSonarGraphs(measures){
  const issueDefs=[['bugs','Bugs',palette.red],['vulnerabilities','Vulnerabilities','#ff9b68'],['code_smells','Code smells',palette.cyan],['security_hotspots','Security hotspots',palette.green]];
  const issueEntries=issueDefs.filter(([key])=>measures[key]!==undefined&&measures[key]!==null&&Number.isFinite(Number(measures[key]))).map(([key,label,color])=>({label,value:Number(measures[key]),color}));
  const percentDefs=[['coverage','Coverage',palette.green],['duplicated_lines_density','Duplicated lines',palette.amber]];
  const percentEntries=percentDefs.filter(([key])=>measures[key]!==undefined&&measures[key]!==null&&Number.isFinite(Number(measures[key]))).map(([key,label,color])=>({label,value:Number(measures[key]),color}));
  $('sonarIssuesChart').hidden=!issueEntries.length;$('sonarIssuesEmpty').hidden=!!issueEntries.length;
  $('sonarPercentChart').hidden=!percentEntries.length;$('sonarPercentEmpty').hidden=!!percentEntries.length;
  if(issueEntries.length)drawSonarBars($('sonarIssuesChart'),issueEntries);
  if(percentEntries.length)drawSonarBars($('sonarPercentChart'),percentEntries,{percent:true});
}
async function loadSonar(){try{const response=await fetch('/api/sonar');const data=await response.json();const labels=[['bugs','Bugs'],['vulnerabilities','Vulnerabilities'],['code_smells','Code smells'],['coverage','Coverage'],['duplicated_lines_density','Duplicated lines'],['security_hotspots','Security hotspots']];$('qualityMetrics').innerHTML=labels.map(([key,label])=>{const value=data.measures?.[key];const shown=value===undefined||value===null?'—':(key==='coverage'||key==='duplicated_lines_density'?value+'%':value);return `<div class="quality-item"><span>${label}</span><strong>${shown}</strong></div>`;}).join('');renderSonarGraphs(data.measures||{});
  $('sonarBadge').textContent=data.configured?data.status:'Not configured';$('sonarStatus').textContent=!data.configured?'Not analyzed: configure SONAR_HOST_URL, SONAR_PROJECT_KEY and SONAR_TOKEN on the server.':data.status==='Connected'?'Measures loaded from configured SonarQube server.':data.status==='No analysis results'?'SonarQube is reachable, but this project has no available analysis measures yet.':'Could not retrieve measures from SonarQube.';$('qualityGate').textContent='Quality gate: '+(data.qualityGate||'Not available');if(data.dashboardUrl){$('sonarLink').href=data.dashboardUrl;$('sonarLink').hidden=false;}}
  catch(e){$('sonarBadge').textContent='Unavailable';$('sonarStatus').textContent='SonarQube data could not be loaded. No metrics are assumed.';renderSonarGraphs({});}}
$('recalculate').addEventListener('click',recalculate);window.addEventListener('resize',()=>{if(currentData){drawLineChart($('priceChart'),currentData.priceYield,'yield','price',{xFormat:v=>(v*100).toFixed(1)+'%',yFormat:money});drawStress($('stressChart'),currentData.stress);}if(!$('sonarIssuesChart').hidden||!$('sonarPercentChart').hidden)loadSonar();});recalculate();loadSonar();
