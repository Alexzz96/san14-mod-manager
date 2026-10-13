/* Inlined by the prototype builder; never a browser network dependency. */
(()=>{'use strict';
const A=window.ArcherPrototype,$=id=>document.getElementById(id);
const states=new Map(A.officers.map(o=>[o.id,A.createOfficer(o.id)]));
let current='guan';const roster={zhuge:false,sun:false};
const s=()=>states.get(current),icon=(name)=>`<svg aria-hidden="true"><use href="#i-${name}"></use></svg>`;
document.documentElement.style.setProperty('--reference',`url("${$('referencePortrait').src}")`);
const stateText={learned:'已学习',available:'可学习',locked:'前置未达成',excluded:'路线互斥'};
const pct=(v)=>`${v>=1?'+':'−'}${Math.abs((v-1)*100).toFixed(1).replace(/\.0$/,'')}%`;
function notice(message){$('notice').textContent=message;}
function changed(){notice('配置已更新，作战演示已重置为 40 点满弹。');render();}
function select(id){s().selected=id;render();}
function renderOfficers(){
 $('officerList').innerHTML=A.officers.map(o=>`<button class="officer ${o.id===current?'active':''}" data-officer="${o.id}" aria-pressed="${o.id===current}">${o.name}</button>`).join('');
 const o=A.officers.find(o=>o.id===current);$('personName').innerHTML=`<span class="seal">${o.force}</span>${o.name}`;
 $('portraitArt').parentElement.classList.toggle('reference',!!o.portrait);
 $('personIdentity').innerHTML=`年龄 —　${o.gender}<span class="healthy">健康</span>`;
 $('referencePortrait').classList.toggle('hidden',!o.portrait);$('portraitPlaceholder').classList.toggle('hidden',!!o.portrait);
 $('portraitPlaceholder').innerHTML=icon('bow')+`<span class="serif" style="font-size:28px">${o.name}</span><small>示例武将 · 立绘占位</small>`;
 $('portraitNote').textContent=o.portrait?'原生角色截图参考 · 不重绘立绘':'仅用于验证武将状态隔离';
}
function renderBranches(){
 $('rootNode').innerHTML=icon('bow')+'弓兵整备 · 已解锁';
 $('branches').innerHTML=A.branches.map(b=>`<section><div class="branchhead">${icon(b.icon)}<h3>${b.name}</h3><small>${s().branch===b.id?`专精 ${s().level}/3`:'三选一'}</small></div><p class="branchdesc">${b.role}</p>${b.nodes.map((name,i)=>{
  const id=b.id+'.'+(i+1),st=A.status(s(),id),preview=[10,20,35][i];
  return `${i?`<div class="connector ${s().branch===b.id&&s().level>i?'lit':''}"></div>`:''}<button class="node ${st} ${s().selected===id?'selected':''}" data-node="${id}" aria-pressed="${s().selected===id}" title="${name}：${b.stat}累计 +${preview}%。${i===2?b.penalty:''}">${icon(b.icon)}<span class="label">${name}</span><span class="status">${stateText[st]}</span></button>`;
 }).join('')}<p class="branchfoot">${b.penalty}</p></section>`).join('');
}
function renderEffects(){
 const count=A.unlockedSlots(s());if(s().activeSlot>=count)s().activeSlot=0;
 $('slots').innerHTML=[0,1].map(i=>{const e=A.effects.find(e=>e.id===s().slots[i]);return `<button class="slot ${s().activeSlot===i&&i<count?'active':''}" data-slot="${i}" ${i>=count?'disabled':''} aria-pressed="${s().activeSlot===i&&i<count}" title="${i<count?'选择该槽位后，点击特效并装备':'基础路线达到 '+(i+2)+' 级开放'}">${icon(i<count?(e?.icon||'quiver'):'lock')}<strong>${e?e.name:`战术槽 ${i+1}`}</strong><small>${i>=count?(i+2)+'级开放':e?'已装备':'待装备'}</small></button>`;}).join('');
 $('effects').innerHTML=A.effects.map(e=>`<button class="effect ${s().slots.includes(e.id)?'equipped':''} ${s().selected==='effect.'+e.id?'selected':''}" data-effect="${e.id}" aria-pressed="${s().selected==='effect.'+e.id}" title="${e.name}：${e.benefit} 副作用：${e.penalty}">${icon(e.icon)}<span class="ename">${e.name}</span><small>${s().slots.includes(e.id)?'已装备 · 可卸下':e.brief}</small></button>`).join('');
 const p=A.stats(s());$('buildSummary').innerHTML=`<span>远程伤害<b>${pct(p.ranged)}</b></span><span>防御<b>${pct(p.defense)}</b></span><span>机动<b>${pct(p.mobility)}</b></span><span>射程<b>${p.range} 格</b></span><span>补箭<b>${p.recovery}/日</b></span><span>攻击<b>${p.focus?'单目标':'多目标'}${p.blade?' / 空弹近战':''}</b></span>`;
 const penalties=[];if(s().level===3)penalties.push(A.branches.find(b=>b.id===s().branch).penalty);
 for(const id of s().slots.filter(Boolean))penalties.push(A.effects.find(e=>e.id===id).penalty);
 $('debuffSummary').textContent=penalties.length?'取舍：'+penalties.join('　·　'):'选择路线与进阶战术后，将在这里汇总副作用。';
}
function renderFinals(){
 $('finals').innerHTML=A.finals.map(f=>{const ok=A.eligible(roster,f.id);return `<article class="finalcard ${ok?'eligible':''}" data-final-info="${f.id}" title="仅保留资格；效果待设计"><div>${icon(ok?'crown':'lock')}</div><div><div class="title">${f.name}</div><div class="condition">${ok?'条件已满足':'需自势力拥有'} · ${f.hero}</div></div><button class="smallbtn" data-final-open="${f.id}" ${ok?'':'disabled'}>${ok?'查看占位':'未解锁'}</button></article>`;}).join('');
}
function renderDetail(){
 const id=s().selected;let title='弓兵整备',ico='bow',body='基础弹药 40。每个目标的一次远程结算消耗 1；完整远程空闲日恢复 2。普通弓兵零弹药停止所有远程攻击，补到至少 1 点后可恢复射击。',positive='三条基础路线只能专精一条；二级与三级各开放一个进阶槽位。',negative='本页学习免费、数值仅为预览，不修改真实军功或游戏属性。',label='已解锁',disabled=true,action=null;
 if(id.startsWith('effect.')){const e=A.effects.find(e=>e.id===id.slice(7));if(e){title=e.name;ico=e.icon;body=`进阶战术 · 全路线通用 · ${A.unlockedSlots(s())} / 2 槽位开放`;positive=e.benefit;negative=e.penalty;const at=s().slots.indexOf(e.id);if(at>=0){label='卸下特效';disabled=false;action=()=>{if(A.unequip(s(),at))changed();};}else{label=`装备槽 ${s().activeSlot+1}`;disabled=s().activeSlot>=A.unlockedSlots(s());action=()=>{if(A.equip(s(),e.id,s().activeSlot))changed();};}}}
 else if(id.startsWith('final.')){const f=A.finals.find(f=>f.id===id.slice(6));if(f){title=f.name;ico='crown';body='传承兵种 · 效果待设计';positive=`解锁条件：自势力拥有${f.hero}。${A.eligible(roster,f.id)?'当前模拟势力满足条件。':'当前模拟势力不满足条件。'}`;negative='仅预留名称与资格；未设计数值、费用、转换或战斗效果。';label=A.eligible(roster,f.id)?'效果待设计':'资格未满足';}}
 else if(id!=='ready'){const [bid,levelText]=id.split('.'),level=Number(levelText),b=A.branches.find(b=>b.id===bid);if(b){title=b.nodes[level-1];ico=b.icon;body=b.description;positive=`${b.stat}累计 +${[10,20,35][level-1]}%（替代上一级，不重复相加）。`;negative=level===3?b.penalty:'前置：'+(level===1?'弓兵整备，且未选择其他路线':b.nodes[level-2])+'。满级取舍：'+b.penalty;const st=A.status(s(),id);label=st==='available'?'学习节点':stateText[st];disabled=st!=='available';action=()=>{if(A.learn(s(),id))changed();};}}
 $('detailCopy').innerHTML=`<h3>${icon(ico)}${title}</h3><p>${body}</p><p class="positive">${positive}</p><p class="negative">${negative}</p>`;
 $('detailAction').textContent=label;$('detailAction').disabled=disabled;$('detailAction').onclick=action;
}
function renderCombat(){const c=s().combat;
 const mode=c.melee?'近战补给':c.ammo===0?'空弹停火':'远程就绪';
 $('simStats').innerHTML=`<div class="simstat"><small>弹药余量</small><strong>${c.ammo}<span style="font-size:12px;color:#aa9675"> / 40</span></strong><div class="gauge"><i style="width:${c.ammo/40*100}%"></i></div></div><div class="simstat"><small>演示日 / 作战状态</small><strong style="font-size:17px">第 ${c.day} 日 · ${mode}</strong></div><div class="simstat"><small>累计远程 / 近战结算</small><strong>${c.shots} <span style="font-size:12px">/ ${c.meleeHits}</span></strong></div><div class="simstat"><small>累计预览伤害</small><strong>${c.totalDamage}</strong></div>`;
 $('combatLog').innerHTML=c.logs.length?c.logs.slice(-100).map(r=>`<tr><td>${r.day}</td><td>${r.shots?`${r.type==='tactic'?'射击战法':'普通射击'} ×${r.shots}`:r.meleeHits?'一格近战 ×1':'未攻击'}</td><td>${r.before} → ${r.after}${r.recovered?` (+${r.recovered})`:''}</td><td>${r.damage}</td><td>${r.notes.join('；')||'正常远程结算'}</td></tr>`).join(''):'<tr><td colspan="5">尚未推进。默认演示两个目标，推进十天观察弹药消耗。</td></tr>';
 const wrap=$('combatLog').closest('.logwrap');wrap.scrollTop=wrap.scrollHeight;
}
function render(){renderOfficers();renderBranches();renderEffects();renderFinals();renderDetail();renderCombat();}
document.addEventListener('click',event=>{const el=event.target.closest('button,[data-final-info]');if(!el||el.disabled)return;
 if(el.dataset.officer){current=el.dataset.officer;notice('已切换武将，保留该武将的专精、特效与作战演示。');render();}
 else if(el.dataset.node)select(el.dataset.node);
 else if(el.dataset.effect)select('effect.'+el.dataset.effect);
 else if(el.dataset.slot!==undefined){s().activeSlot=Number(el.dataset.slot);render();notice(`已选择战术槽 ${s().activeSlot+1}，点击特效后装备。`);}
 else if(el.dataset.finalOpen){if(A.eligible(roster,el.dataset.finalOpen)){select('final.'+el.dataset.finalOpen);notice('资格已满足；最终兵种目前仅为占位，效果待设计。');}}
 else if(el.dataset.finalInfo)select('final.'+el.dataset.finalInfo);
});
function step(days){const input={targets:Number($('targetCount').value),distance:Number($('targetDistance').value),type:$('attackType').value};for(let n=0;n<days;n++)A.advance(s(),input);renderCombat();notice(`已推进 ${days} 天，作战记录见演示面板。`);}
 $('rootNode').onclick=()=>select('ready');
 $('resetBuild').onclick=()=>{states.set(current,A.createOfficer(current));changed();};
 $('advanceDay').onclick=()=>step(1);$('advanceTurn').onclick=()=>step(10);
 $('resetCombat').onclick=()=>{A.resetCombat(s());renderCombat();notice('已重置作战演示，专精与特效保持不变。');};
 $('rosterZhuge').onchange=event=>{roster.zhuge=event.target.checked;renderFinals();renderDetail();};
 $('rosterSun').onchange=event=>{roster.sun=event.target.checked;renderFinals();renderDetail();};
 window.ArcherUI={states,roster,get current(){return current;},render};render();
})();
