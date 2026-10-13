const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const file=path.resolve(process.argv[2]||path.join(__dirname,'../docs/prototypes/archer-tech-tree.html'));
const html=fs.readFileSync(file,'utf8'),core=html.match(/<script id="archer-model">([\s\S]*?)<\/script>/)[1];
const ctx=vm.createContext({});vm.runInContext(core,ctx);const A=ctx.ArcherPrototype,tests=[];
function check(name,fn){fn();tests.push(name);}
function full(first,second){const s=A.createOfficer('test');for(const column of [first,second])if(column)for(let rank=1;rank<=3;rank++)assert(A.learn(s,column+'.'+rank));return s;}
function day(s,targets=4,distance=2,type='ordinary',random){return A.advance(s,{targets,distance,type},random);}
check('cross-column prerequisites, eight base points, no groups or duplicate nodes',()=>{
 const s=A.createOfficer('test');assert(!A.learn(s,'defense.2'));assert(A.learn(s,'attack.1'));
 assert(A.learn(s,'defense.2'));assert.equal(s.parents['defense.2'],'attack.1');assert(A.learn(s,'mobility.3'));
 assert.equal(A.used(s),3);assert(!A.learn(s,'attack.1'));assert(A.learn(s,'defense.1'));
 assert(A.learn(s,'attack.2'));assert(A.learn(s,'attack.3'));assert.equal(A.used(s),6);
 assert.equal(A.complete(s,'attack'),true);assert.equal(A.complete(s,'defense'),false);
 assert(A.learn(s,'loading.1'));assert(A.learn(s,'capacity.3'));assert.equal(A.used(s),8);
 assert(!A.learn(s,'defense.3'));assert.equal(A.status(s,'defense.3'),'budget');
 assert(A.learnAdvanced(s,'focus'));assert.equal(A.used(s),8);assert(!('groups' in s));
});
check('independent node deltas, attack/defense symmetry and pure mobility',()=>{
 const incremental=A.createOfficer('test');for(const [rank,gain] of [[1,5],[2,15],[3,30]]){assert(A.learn(incremental,'attack.'+rank));assert(Math.abs(A.stats(incremental).attack-(1+gain/100))<1e-10);}
 let s=full('attack');assert.equal(A.stats(s).attack,1.3);assert.equal(A.stats(s).defense,.85);
 s=full('defense');assert.equal(A.stats(s).attack,.85);assert.equal(A.stats(s).defense,1.3);
 s=full('mobility');assert.equal(A.stats(s).mobility,1.3);assert.equal(A.stats(s).attack,1);assert.equal(A.stats(s).defense,1);
 s=full('attack','defense');assert.equal(A.stats(s).attack,1.15);assert.equal(A.stats(s).defense,1.15);
});
check('advanced tactics require the complete corresponding column and cost no base points',()=>{
 const s=full('attack','defense');assert(!A.learnAdvanced(s,'reload'));assert(A.learnAdvanced(s,'focus'));assert(!A.learnAdvanced(s,'focus'));assert(A.learnAdvanced(s,'pin'));
 assert.equal(A.used(s),6);assert.equal(s.effects.length,2);assert(Math.abs(A.stats(s).ranged-1.15*1.4*.85)<1e-10);
});
check('capacity/loading tradeoffs, exact fractional reload accumulation and cap',()=>{
 const big=full('capacity'),fast=full('loading');assert.equal(A.stats(big).maximum,70);assert.equal(A.stats(big).recovery,1.4);assert.equal(A.stats(fast).maximum,25);assert.equal(A.stats(fast).recovery,3.5);
 big.combat.ammo=0;for(let i=0;i<5;i++)day(big,0);assert.equal(big.combat.ammo,7);assert.equal(big.combat.credit,0);
 fast.combat.ammo=0;assert.equal(day(fast,0).after,3);assert.equal(day(fast,0).after,7);
 for(let i=0;i<30;i++)day(fast,0);assert.equal(fast.combat.ammo,25);assert.equal(fast.combat.credit,0);
 const combined=full('capacity','loading');assert.equal(A.stats(combined).maximum,55);assert.equal(A.stats(combined).recovery,2.9);assert(A.learnAdvanced(combined,'reload'));assert.equal(A.stats(combined).recovery,5.8);
});
check('untrained four targets ten days empty; two targets half; zero blocks all ranged types',()=>{
 for(const targets of [2,4]){const s=A.createOfficer('test');for(let i=0;i<10;i++)day(s,targets);assert.equal(s.combat.ammo,40-targets*10);assert.equal(s.combat.shots,targets*10);assert(s.combat.logs.every(r=>!r.recovered));}
 for(const type of ['ordinary','tactic']){const s=A.createOfficer('test');s.combat.ammo=1;assert.equal(day(s,4,2,type).shots,1);assert.equal(s.combat.ammo,0);assert.equal(day(s,4,2,type).shots,0);assert.equal(s.combat.ammo,2);}
});
check('blade melee starts next day and must refill adjusted maximum before firing',()=>{
 const s=full('mobility','loading');A.learnAdvanced(s,'blade');A.learnAdvanced(s,'reload');assert.equal(A.stats(s).maximum,25);
 for(let i=0;i<7;i++)day(s,4,1);assert.equal(s.combat.ammo,0);assert.equal(s.combat.meleeHits,0);assert(s.combat.melee);
 for(let i=0;i<3;i++){const r=day(s,4,1);assert.equal(r.shots,0);assert.equal(r.meleeHits,1);assert.equal(r.damage,60);assert(s.combat.melee);}
 const fullRow=day(s,4,1);assert.equal(fullRow.shots,0);assert.equal(s.combat.ammo,25);assert(!s.combat.melee);assert.equal(day(s,4,1).shots,4);
});
check('focus primary only; range works; tactic and melee do not proc pin',()=>{
 const s=full('attack','capacity');A.learnAdvanced(s,'focus');A.learnAdvanced(s,'range');assert.equal(A.stats(s).range,3);assert.equal(day(s,4,3).shots,1);assert.equal(day(s,4,4).shots,0);
 const pin=full('defense');A.learnAdvanced(pin,'pin');assert.equal(day(pin,4,2,'ordinary',()=>0).notes.filter(n=>n.includes('止步')).length,4);
 assert.equal(day(pin,4,2,'tactic',()=>0).notes.filter(n=>n.includes('止步')).length,0);
 const melee=full('defense','mobility');A.learnAdvanced(melee,'pin');A.learnAdvanced(melee,'blade');melee.combat.ammo=0;melee.combat.melee=true;
 assert.equal(day(melee,4,1,'ordinary',()=>0).notes.filter(n=>n.includes('止步')).length,0);
});
check('pin probability threshold and one full following day expiration',()=>{
 const s=full('defense');A.learnAdvanced(s,'pin');day(s,1,2,'ordinary',()=>.14999);assert.equal(s.combat.stopped[1],2);day(s,0);assert.equal(s.combat.stopped[1],2);day(s,0);assert.equal(Object.keys(s.combat.stopped).length,0);assert.equal(day(s,1,2,'ordinary',()=>.15).notes.filter(n=>n.includes('止步')).length,0);
});
check('officer isolation, configuration reset, faction eligibility',()=>{
 const one=full('attack'),two=A.createOfficer('other');day(one);assert.equal(two.combat.ammo,40);assert.equal(A.used(two),0);A.learnAdvanced(one,'focus');assert.equal(one.combat.ammo,40);assert.equal(one.combat.day,0);
 assert(A.eligible({zhuge:true},'repeating'));assert(!A.eligible({zhuge:true},'jiefan'));assert(A.eligible({sun:true},'jiefan'));
});
check('eight-point builds retain bounded ammo, positive loading and at most two complete columns',()=>{
 for(let i=0;i<5;i++)for(let j=i+1;j<5;j++){const s=full(A.branches[i].id,A.branches[j].id);assert(A.learnAdvanced(s,A.branches[i].effect));assert(A.learnAdvanced(s,A.branches[j].effect));assert.equal(s.effects.length,2);assert(A.stats(s).recovery>0);
  const extra=A.branches.find((b,k)=>k!==i&&k!==j);assert(A.learn(s,extra.id+'.1'));assert(A.learn(s,extra.id+'.2'));assert(!A.learn(s,extra.id+'.3'));assert(!A.learnAdvanced(s,extra.effect));assert.equal(A.used(s),8);
  for(let n=0;n<150;n++){day(s,n%11===0?0:4,n%9===0?4:1,n%3===0?'tactic':'ordinary');assert(s.combat.ammo>=0&&s.combat.ammo<=A.stats(s).maximum);}}
});
assert(!html.includes('__REFERENCE_DATA__')&&!html.includes('__MODEL__')&&!html.includes('__UI__'));
console.log(JSON.stringify({status:'passed',tests:tests.length,scenarios:tests},null,2));
