(()=>{'use strict';
const branches=[
 {id:'attack',name:'攻军',icon:'bow',nodes:['校弦','劲弓','破阵'],effect:'focus',bonus:{attack:5,defense:-2.5},description:'强化弓弦与射击训练，用生存能力换取输出。三阶独立增量为 5%、10%、15%，单列合计 30%。'},
 {id:'defense',name:'守备',icon:'shield',nodes:['护具','固位','列盾'],effect:'pin',bonus:{attack:-2.5,defense:5},description:'强化护具与掩护，用火力换取抗压能力。三阶独立增量为 5%、10%、15%，单列合计 30%。'},
 {id:'mobility',name:'机动',icon:'boot',nodes:['轻装','疾退','游射'],effect:'blade',bonus:{mobility:5},description:'训练行军和脱离接触，不增加火力或护甲。三阶独立增量为 5%、10%、15%，单列机动合计 30%。'},
 {id:'capacity',name:'容量',icon:'quiver',nodes:['扩匣','备矢','重箭匣'],effect:'range',bonus:{capacity:10,recoveryRate:-10},benefit:'弹药上限 +10',penalty:'装填速率 −10%（按基础速率计算）',description:'携带更多箭矢，延长持续交战时间，代价是补满弹药更慢。'},
 {id:'loading',name:'装填',icon:'reload',nodes:['轻匣','速装','轮换装填'],effect:'reload',bonus:{capacity:-5,recoveryRate:25},benefit:'装填速率 +25%（按基础速率计算）',penalty:'弹药上限 −5',description:'精简箭匣并优化补给动作，更快补箭，代价是交战储备减少。'}
];
const effects=[
 {id:'focus',branch:'attack',name:'集中射击',icon:'target',benefit:'只对主目标结算，远程伤害 +40%；远射效果仍生效。',penalty:'移除多目标攻击；每日弹药恢复速率 −1。'},
 {id:'pin',branch:'defense',name:'牵制箭',icon:'pin',benefit:'普通远程射击造成实际伤害时，15% 概率令目标止步 1 个游戏日。每个目标每天最多判定一次，不叠加时长。',penalty:'远程伤害 −15%；射击战法与近战不触发。'},
 {id:'blade',branch:'mobility',name:'短刃备战',icon:'blade',benefit:'耗尽后，从次日开始可对一格内最近目标近战。近战伤害为未应用攻军与远程加成的基础伤害的 60%，不消耗箭矢、不阻止补箭。',penalty:'防御 −10%；须补满当前弹药上限才能恢复远程。'},
 {id:'range',branch:'capacity',name:'远射弓臂',icon:'range',benefit:'原有射程增加 1 格；演示从 2 格提高到 3 格。',penalty:'远程伤害 −20%。'},
 {id:'reload',branch:'loading',name:'快速装填',icon:'reload',benefit:'将基础科技修正后的装填速率提高一倍。',penalty:'远程伤害 −15%。'}
];
const finals=[{id:'repeating',name:'诸葛连弩',hero:'诸葛亮',key:'zhuge'},{id:'jiefan',name:'解烦弓',hero:'孙权',key:'sun'}];
const officers=[{id:'guan',name:'关银屏',force:'蜀',gender:'女',portrait:true},{id:'huangzu',name:'黄祖',force:'吴',gender:'男'},{id:'huangzhong',name:'黄忠',force:'蜀',gender:'男'}];
function combat(maximum=40){return {day:0,ammo:maximum,maximum,credit:0,melee:false,seed:20261013,shots:0,meleeHits:0,totalDamage:0,logs:[],stopped:{}};}
function createOfficer(id){return {id,learned:[],parents:{},effects:[],combat:combat()};}
function used(s){return s.learned.length;}
function has(s,id){return s.learned.includes(id);}
function nodes(s){return s.learned.slice();}
function parse(id){const [bid,l]=id.split('.'),rank=Number(l)-1,b=branches.find(b=>b.id===bid);return b&&Number.isInteger(rank)&&rank>=0&&rank<3?{b,rank}:null;}
function nodeBonus(n){const scale=['attack','defense','mobility'].includes(n.b.id)?n.rank+1:1;return Object.fromEntries(Object.entries(n.b.bonus).map(([k,v])=>[k,v*scale]));}
function nodeInfo(id){const n=parse(id);if(!n)return null;const bonus=nodeBonus(n);if(n.b.id==='attack')return {benefit:'攻军 +'+bonus.attack+'%',penalty:'防御 −'+Math.abs(bonus.defense)+'%'};if(n.b.id==='defense')return {benefit:'防御 +'+bonus.defense+'%',penalty:'攻军 −'+Math.abs(bonus.attack)+'%'};if(n.b.id==='mobility')return {benefit:'机动 +'+bonus.mobility+'%',penalty:'无额外数值减益；消耗 1 点基础点数。'};return {benefit:n.b.benefit,penalty:n.b.penalty};}
function status(s,id){if(id==='ready')return 'learned';const n=parse(id);if(!n)return 'locked';if(has(s,id))return 'learned';if(used(s)>=8)return 'budget';if(n.rank>0&&!s.learned.some(id=>parse(id).rank===n.rank-1))return 'locked';return 'available';}
function complete(s,bid){return [1,2,3].every(rank=>has(s,bid+'.'+rank));}
function advancedStatus(s,id){const e=effects.find(e=>e.id===id);if(!e)return 'locked';return s.effects.includes(id)?'learned':complete(s,e.branch)?'available':'locked';}
function stats(s){const totals={attack:0,defense:0,mobility:0,capacity:0,recoveryRate:0};for(const id of nodes(s)){const n=parse(id);for(const [key,value] of Object.entries(nodeBonus(n)))totals[key]+=value;}
 const e=new Set(s.effects);let ranged=1+totals.attack/100,defense=1+totals.defense/100;
 if(e.has('focus'))ranged*=1.4;if(e.has('pin'))ranged*=.85;if(e.has('range'))ranged*=.8;if(e.has('reload'))ranged*=.85;if(e.has('blade'))defense*=.9;
 let recoveryHundredths=200+totals.recoveryRate*2;if(e.has('reload'))recoveryHundredths*=2;if(e.has('focus'))recoveryHundredths-=100;
 return {attack:1+totals.attack/100,ranged,defense,mobility:1+totals.mobility/100,maximum:40+totals.capacity,recovery:recoveryHundredths/100,recoveryHundredths,range:2+(e.has('range')?1:0),focus:e.has('focus'),pin:e.has('pin'),blade:e.has('blade'),totals};}
function resetCombat(s){s.combat=combat(stats(s).maximum);}
function learn(s,id){if(status(s,id)!=='available')return false;const n=parse(id);s.parents[id]=n.rank?s.learned.filter(id=>parse(id).rank===n.rank-1).at(-1):null;s.learned.push(id);resetCombat(s);return true;}
function learnAdvanced(s,id){if(advancedStatus(s,id)!=='available')return false;s.effects.push(id);resetCombat(s);return true;}
function rng(c){c.seed=(Math.imul(1664525,c.seed)+1013904223)>>>0;return c.seed/4294967296;}
function advance(s,input,random){const c=s.combat,p=stats(s),targets=Math.max(0,Math.min(4,Math.floor(Number(input.targets)||0))),distance=Math.max(1,Math.min(4,Math.floor(Number(input.distance)||1))),type=input.type==='tactic'?'tactic':'ordinary';c.day++;let shots=0,meleeHits=0,damage=0;const notes=[],before=c.ammo;
 for(const id of Object.keys(c.stopped))if(c.stopped[id]<c.day)delete c.stopped[id];
 if(c.melee&&c.ammo===p.maximum)c.melee=false;
 if(c.melee){if(targets&&distance===1){meleeHits=1;damage=60;notes.push('一格近战，攻军与远程加成不生效');}else notes.push(targets?'近战目标不在一格内':'没有攻击目标');}
 else if(c.ammo===0){if(p.blade)c.melee=true;notes.push('空弹：普通射击与射击战法均停止');}
 else if(!targets)notes.push('停止射击，整日装填');
 else if(distance>p.range)notes.push('目标超出射程，未射击');
 else{const count=Math.min(targets,p.focus?1:4,c.ammo);for(let id=1;id<=count;id++){shots++;c.ammo--;const hit=Math.round(100*p.ranged);damage+=hit;if(p.pin&&type==='ordinary'&&hit>0&&(random?random():rng(c))<.15){c.stopped[id]=c.day+1;notes.push('目标 '+id+' 止步 1 日');}}if(p.focus&&targets>1)notes.push('集中射击：忽略副目标');if(c.ammo===0){if(p.blade)c.melee=true;notes.push(p.blade?'箭矢耗尽；次日起近战，补满后恢复远程':'箭矢耗尽，停止后续远程结算');}}
 let recovered=0;if(!shots){c.credit+=p.recoveryHundredths;const whole=Math.floor(c.credit/100);c.credit%=100;recovered=Math.min(whole,p.maximum-c.ammo);c.ammo+=recovered;if(recovered)notes.push('远程空闲日恢复 '+recovered);if(c.ammo===p.maximum){c.credit=0;if(c.melee){c.melee=false;notes.push('补满 '+p.maximum+'：下一日恢复远程');}}if(c.credit)notes.push('装填进度 '+c.credit+'%（不足一发跨日累积）');}
 c.shots+=shots;c.meleeHits+=meleeHits;c.totalDamage+=damage;const row={day:c.day,before,after:c.ammo,shots,meleeHits,damage,recovered,type,notes,state:c.melee?'近战装填':c.ammo===0?'空弹停火':'远程就绪'};c.logs.push(row);return row;}
function eligible(roster,id){const f=finals.find(f=>f.id===id);return !!(f&&roster[f.key]);}
const api={branches,effects,finals,officers,combat,createOfficer,used,has,nodes,parse,nodeBonus,nodeInfo,status,complete,advancedStatus,stats,resetCombat,learn,learnAdvanced,advance,eligible};if(typeof window!=='undefined')window.ArcherPrototype=api;else globalThis.ArcherPrototype=api;
})();
