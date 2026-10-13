(()=>{'use strict';
const A=window.ArcherPrototype,$=id=>document.getElementById(id),states=new Map(A.officers.map(o=>[o.id,A.createOfficer(o.id)]));
let current='guan',dialogAction=null;const roster={zhuge:false,sun:false};
const s=()=>states.get(current),icon=name=>`<svg aria-hidden="true"><use href="#i-${name}"></use></svg>`;
const statusText={learned:'已学习',available:'可学习',locked:'前置未达成',chosen:'当阶已选择',budget:'点数已用完'};
const percent=v=>`${v>=1?'+':'−'}${Math.abs((v-1)*100).toFixed(1).replace(/\.0$/,'')}%`;
document.documentElement.style.setProperty('--reference',`url("${$('referencePortrait').src}")`);
function notice(message){$('notice').textContent=message;}
function changed(){render();notice('培养已更新，作战演示已按当前弹药上限重置。');}
function renderOfficers(){
 $('officerList').innerHTML=A.officers.map(o=>`<button class="officer ${o.id===current?'active':''}" data-officer="${o.id}" aria-pressed="${o.id===current}">${o.name}</button>`).join('');
 const o=A.officers.find(o=>o.id===current);$('personName').innerHTML=`<span class="seal">${o.force}</span>${o.name}`;
 $('portraitArt').parentElement.classList.toggle('reference',!!o.portrait);$('personIdentity').innerHTML=`年龄 —　${o.gender}<span class="healthy">健康</span>`;
 $('referencePortrait').classList.toggle('hidden',!o.portrait);$('portraitPlaceholder').classList.toggle('hidden',!!o.portrait);
 $('portraitPlaceholder').innerHTML=icon('bow')+`<span class="serif" style="font-size:28px">${o.name}</span><small>示例武将 · 立绘占位</small>`;
 $('portraitNote').textContent=o.portrait?'原生角色截图参考 · 不重绘立绘':'仅用于验证武将状态隔离';
}
function renderTree(){
 $('training').innerHTML=`<span class="quiet">跨列学习 · 每节点仅能学习一次 · 进阶免费</span><span class="points">基础点数 <strong>${A.used(s())} / 8</strong></span>`;
 $('rootNode').innerHTML=icon('bow')+'弓兵整备 · 最多 8 点基础';
 $('treeCols').innerHTML=A.branches.map(b=>`<section class="treecol"><div class="colhead">${icon(b.icon)}${b.name}</div>${b.nodes.map((name,i)=>{
  const id=b.id+'.'+(i+1),st=A.status(s(),id);
  return `<div class="skillwrap"><button class="skill ${st}" data-node="${id}" title="点击查看 ${name}" aria-label="${name}，${statusText[st]}"><span class="circle">${icon(b.icon)}</span><span class="label">${name}</span><span class="state">${statusText[st]}</span></button></div>`;
 }).join('')}${(()=>{const e=A.effects.find(e=>e.id===b.effect),st=A.advancedStatus(s(),e.id),count=[1,2,3].filter(rank=>A.has(s(),b.id+'.'+rank)).length;return `<div class="skillwrap advanced"><button class="skill ${st}" data-advanced="${e.id}" title="点击查看 ${e.name}" aria-label="${e.name}，${statusText[st]}"><span class="circle">${icon(e.icon)}</span><span class="label">${e.name}</span><span class="state">${st==='learned'?'进阶已学习':st==='available'?'可免费学习':'本列 '+count+'/3'}</span></button></div>`;})()}</section>`).join('');
 $('pathSummary').innerHTML=`已学习 ${A.used(s())} 个基础节点 · 点满 ${A.branches.filter(b=>A.complete(s(),b.id)).length} 列 · 已学习 ${s().effects.length} 个进阶（免费）`;
 requestAnimationFrame(drawPaths);
}
function drawPaths(){
 const tree=$('tree'),rect=tree.getBoundingClientRect(),w=rect.width,h=rect.height;if(!w||!h)return;
 const point=id=>{const e=tree.querySelector(`[data-node="${id}"],[data-advanced="${id}"] .circle`);const circle=e?.classList.contains('circle')?e:e?.querySelector('.circle');if(!circle)return null;const r=circle.getBoundingClientRect();return {x:r.x-rect.x+r.width/2,y:r.y-rect.y+r.height/2};};
 const line=(a,b,color,width=1,opacity=.7)=>`<path d="M ${a.x} ${a.y} C ${a.x} ${(a.y+b.y)/2}, ${b.x} ${(a.y+b.y)/2}, ${b.x} ${b.y}" stroke="${color}" stroke-width="${width}" opacity="${opacity}" fill="none"/>`;
 let paths='';for(const b of A.branches){const ids=[b.id+'.1',b.id+'.2',b.id+'.3',b.effect],ps=ids.map(point);for(let i=1;i<ps.length;i++)if(ps[i-1]&&ps[i])paths+=line(ps[i-1],ps[i],'#75603b');if(A.complete(s(),b.id)&&ps[2]&&ps[3])paths+=line(ps[2],ps[3],'#c8a763',1.8);}
 for(const id of s().learned){const parent=s().parents[id];if(parent){const a=point(parent),b=point(id);if(a&&b)paths+=line(a,b,'#ebc16a',2,.9);}}
 $('treePaths').setAttribute('viewBox',`0 0 ${w} ${h}`);$('treePaths').innerHTML=paths;
}
function renderSummary(){const p=A.stats(s());
 $('buildSummary').innerHTML=`<span>攻军<b>${percent(p.attack)}</b></span><span>防御<b>${percent(p.defense)}</b></span><span>机动<b>${percent(p.mobility)}</b></span><span>弹药<b>${p.maximum}</b></span><span>装填<b>${Number(p.recovery.toFixed(2))}/日</b></span><span>射程<b>${p.range} 格</b></span><span>战术<b>${s().effects.length? s().effects.map(id=>A.effects.find(e=>e.id===id).name).join(' / '):'尚未学习'}</b></span>`;
 const penalties=[];if(p.totals.attack<0)penalties.push('攻军 '+p.totals.attack+'%');if(p.totals.defense<0)penalties.push('防御 '+p.totals.defense+'%');if(p.totals.capacity<0)penalties.push('弹药上限 '+p.totals.capacity);if(p.totals.recoveryRate<0)penalties.push('装填速率 '+p.totals.recoveryRate+'%');for(const id of s().effects)penalties.push(A.effects.find(e=>e.id===id).penalty);
 $('debuffSummary').textContent=penalties.length?'当前取舍：'+penalties.join('　·　'):'进阶需对应一列三阶全部点亮；跨列培养可以自由组合基础增量。';
}
function renderFinals(){
 $('finals').innerHTML=A.finals.map(f=>{const ok=A.eligible(roster,f.id);return `<article class="finalcard ${ok?'eligible':''}" data-final-info="${f.id}"><div>${icon(ok?'crown':'lock')}</div><div><div class="title">${f.name}</div><div class="condition">${ok?'条件已满足':'需自势力拥有'} · ${f.hero}</div></div><button class="smallbtn" data-final-open="${f.id}" ${ok?'':'disabled'}>${ok?'查看占位':'未解锁'}</button></article>`;}).join('');
}
function showDialog({title,ico,subtitle,description,benefit,penalty,condition,label='确认学习',disabled=true,action=null}){
 $('dialogIcon').innerHTML=icon(ico);$('dialogTitle').textContent=title;$('dialogSubtitle').textContent=subtitle;
 $('dialogDescription').innerHTML=`<p>${description}</p><p class="positive">${benefit}</p><p class="negative">${penalty}</p><p class="condition">${condition}</p>`;
 $('dialogConfirm').textContent=label;$('dialogConfirm').disabled=disabled;dialogAction=action;
 if(!$('skillDialog').open)$('skillDialog').showModal();
}
function openNode(id){
 const n=A.parse(id);if(!n)return;const st=A.status(s(),id),info=A.nodeInfo(id),prev=n.rank?s().learned.filter(id=>A.parse(id).rank===n.rank-1).at(-1):null;
 const condition=st==='learned'?'已经学习；不会重复消耗点数。':st==='budget'?'8 点基础点数已用完。进阶仍可在满足条件时免费学习。':st==='locked'?'请先学习任意列的上一阶节点。':`消耗 1 基础点。${prev?'前置已满足：'+A.parse(prev).b.nodes[A.parse(prev).rank]:'可选择任意列的一阶。'}点数 ${A.used(s())}/8。`;
 showDialog({title:n.b.nodes[n.rank],ico:n.b.icon,subtitle:`${n.b.name}脉络 · ${['一','二','三'][n.rank]}阶`,description:n.b.description,benefit:info.benefit,penalty:info.penalty,condition,label:st==='available'?'确认学习':statusText[st],disabled:st!=='available',action:()=>{if(A.learn(s(),id))changed();}});
}
function openAdvanced(id){
 const e=A.effects.find(e=>e.id===id);if(!e)return;const b=A.branches.find(b=>b.id===e.branch),st=A.advancedStatus(s(),id),requirements=b.nodes.map((name,i)=>`${A.has(s(),b.id+'.'+(i+1))?'✓':'○'} ${name}`).join('　');
 showDialog({title:e.name,ico:e.icon,subtitle:`${b.name}脉络 · 独立进阶战术`,description:'对应列的三个基础节点全部点亮后，独立确认学习并自动生效。',benefit:e.benefit,penalty:e.penalty,condition:`前置：${requirements}。免费学习，不占用 8 点基础点数。`,label:st==='available'?'确认解锁战术':st==='learned'?'进阶已学习':'本列尚未点满',disabled:st!=='available',action:()=>{if(A.learnAdvanced(s(),id))changed();}});
}
function openFinal(id){const f=A.finals.find(f=>f.id===id);if(!f)return;showDialog({title:f.name,ico:'crown',subtitle:'传承兵种 · 预留位置',description:'效果待设计；不填写数值、费用、转换或战斗效果。',benefit:'解锁条件：自势力拥有'+f.hero+'。',penalty:'',condition:A.eligible(roster,id)?'当前模拟势力满足条件。':'当前模拟势力不满足条件。',label:A.eligible(roster,id)?'效果待设计':'资格未满足'});}
function renderCombat(){const c=s().combat,p=A.stats(s());const mode=c.melee?'近战装填':c.ammo===0?'空弹停火':'远程就绪';
 $('simStats').innerHTML=`<div class="simstat"><small>弹药余量 / 上限</small><strong>${c.ammo}<span style="font-size:12px;color:#aa9675"> / ${p.maximum}</span></strong><div class="gauge"><i style="width:${c.ammo/p.maximum*100}%"></i></div></div><div class="simstat"><small>演示日 / 作战状态</small><strong style="font-size:17px">第 ${c.day} 日 · ${mode}</strong></div><div class="simstat"><small>累计远程 / 近战结算</small><strong>${c.shots} <span style="font-size:12px">/ ${c.meleeHits}</span></strong></div><div class="simstat"><small>装填速率 / 累计伤害</small><strong style="font-size:19px">${Number(p.recovery.toFixed(2))}/日 · ${c.totalDamage}</strong></div>`;
 $('combatLog').innerHTML=c.logs.length?c.logs.slice(-100).map(r=>`<tr><td>${r.day}</td><td>${r.shots?`${r.type==='tactic'?'射击战法':'普通射击'} ×${r.shots}`:r.meleeHits?'一格近战 ×1':'未攻击'}</td><td>${r.before} → ${r.after}${r.recovered?` (+${r.recovered})`:''}</td><td>${r.damage}</td><td>${r.notes.join('；')||'正常远程结算'}</td></tr>`).join(''):'<tr><td colspan="5">尚未推进。无科技时：四目标十天消耗 40，两目标消耗 20。</td></tr>';
 const wrap=$('combatLog').closest('.logwrap');wrap.scrollTop=wrap.scrollHeight;
}
function render(){renderOfficers();renderTree();renderSummary();renderFinals();renderCombat();}
document.addEventListener('click',event=>{const el=event.target.closest('button,[data-final-info]');if(!el||el.disabled)return;
 if(el.dataset.officer){current=el.dataset.officer;render();notice('已切换武将，保留其培养与作战演示。');}
 else if(el.dataset.node)openNode(el.dataset.node);
 else if(el.dataset.advanced)openAdvanced(el.dataset.advanced);
 else if(el.dataset.finalOpen){if(A.eligible(roster,el.dataset.finalOpen))openFinal(el.dataset.finalOpen);}
 else if(el.dataset.finalInfo)openFinal(el.dataset.finalInfo);
});
 $('dialogCancel').onclick=()=>{$('skillDialog').close();dialogAction=null;};
 $('dialogConfirm').onclick=()=>{const action=dialogAction;$('skillDialog').close();dialogAction=null;if(action)action();};
 $('skillDialog').addEventListener('click',event=>{if(event.target===$('skillDialog')){const r=$('skillDialog').getBoundingClientRect();if(event.clientX<r.left||event.clientX>r.right||event.clientY<r.top||event.clientY>r.bottom)$('skillDialog').close();}});
 $('skillDialog').addEventListener('close',()=>{dialogAction=null;});
 $('rootNode').onclick=()=>showDialog({title:'弓兵整备',ico:'bow',subtitle:'基础最多 8 点 · 进阶免费',description:'学习任意列的上一阶后，可跨列学习下一阶。每个基础节点消耗 1 点，不能重复学习，不划分培养组。',benefit:'同一列三个节点全亮，免费开放该列一个独立进阶战术。8 点最多点满两列，还可选两个基础节点。',penalty:'攻军、守备、机动三阶为独立的 5%、10%、15% 增量，单列合计 30%。弹药速率不足一发的部分跨日累积。',condition:'示例：校弦 → 固位 → 游射。刷新页面重置演示。',label:'基础已解锁'});
 $('resetBuild').onclick=()=>{states.set(current,A.createOfficer(current));changed();};
 function step(days){const input={targets:Number($('targetCount').value),distance:Number($('targetDistance').value),type:$('attackType').value};for(let i=0;i<days;i++)A.advance(s(),input);renderCombat();notice(`已推进 ${days} 天，作战记录见演示面板。`);}
 $('advanceDay').onclick=()=>step(1);$('advanceTurn').onclick=()=>step(10);$('resetCombat').onclick=()=>{A.resetCombat(s());renderCombat();notice('已重置作战演示，培养保持不变。');};
 $('rosterZhuge').onchange=event=>{roster.zhuge=event.target.checked;renderFinals();};$('rosterSun').onchange=event=>{roster.sun=event.target.checked;renderFinals();};
 new ResizeObserver(drawPaths).observe($('tree'));window.ArcherUI={states,roster,get current(){return current;},render};render();
})();
