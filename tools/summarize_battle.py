"""Normalize local battle JSONL without turning unverified fields into career totals."""
from pathlib import Path
import argparse
import json
import math
import struct
from collections import Counter

def identity(o):
    if o.get('kind') != 2 or not o.get('active') or o.get('leader_id', -1) < 0:
        return None
    return (o.get('address'),o.get('id'),o.get('leader_id'))

def signed_arg(e,i):
    v=int(e.get('args',[])[i],16)
    return v-(1<<64) if v >= (1<<63) else v

def float32(value):
    return struct.unpack('<f',struct.pack('<f',value))[0]

def native_wounded_formula(e,by_id):
    """Validate the observed army path; this does not verify soldier deaths."""
    result={'wounded_formula_verified':False,'generated_wounded_candidate':None,
            'wounded_attrition_candidate':None,'wounded_formula_reason':'missing_or_unsupported_native_path'}
    if (e.get('schema_version',0)<5 or e.get('caller_rva')!='0x166861'
        or e.get('post_identity_matches')!=1 or not identity(e.get('source',{}))
        or not identity(e.get('target',{})) or identity(e.get('target_after',{}))!=identity(e['target'])
        or e.get('wound_rate_count')!=1 or not e.get('attrition_parameters_read')):
        return result
    children=[r for r in by_id.values() if r.get('kind')=='wounded_generation_ratio' and r.get('parent_id')==e['id']]
    if len(children)!=1 or children[0].get('caller_rva')!='0x16accb': return result
    child=children[0]
    try:
        if signed_arg(e,0)!=27 or signed_arg(child,2)!=27: return result
        for key in ('wound_rate_float_bits','attrition_percent_raw','attrition_divisor_float_bits'):
            if child.get(key)!=e.get(key): return result
        rate=struct.unpack('<f',struct.pack('<I',int(e['wound_rate_float_bits'],16)))[0]
        divisor=struct.unpack('<f',struct.pack('<I',int(e['attrition_divisor_float_bits'],16)))[0]
        percent=e['attrition_percent_raw'];before=e['target']['field18_raw'];after=e['target_after']['field18_raw']
        loss=e['target']['troops']-e['target_after']['troops']
        if not math.isfinite(rate) or rate<0 or rate>1 or not math.isfinite(divisor) or divisor<=0 or percent<0 or percent>100 or loss<=0 or loss>20000 or not (0<=before<=20000):
            result['wounded_formula_reason']='parameters_outside_validated_range';return result
        generated=math.trunc(float32(float32(loss)*rate))
        attrition=math.trunc(float32(float32(float32(percent)/divisor)*float32(loss)))
        intermediate=min(20000,max(0,before+generated));prediction=min(20000,max(0,intermediate-attrition))
        result['predicted_field18_raw']=prediction
        if prediction!=after:
            result['wounded_formula_reason']='native_prediction_does_not_match_snapshot';return result
        result.update(wounded_formula_verified=True,generated_wounded_candidate=generated,
                      wounded_attrition_candidate=attrition,wounded_formula_reason='native_path_and_float32_prediction_match',
                      surviving_field18_raw=after,intermediate_field18_raw=intermediate)
    except (KeyError,IndexError,ValueError,TypeError,OverflowError,struct.error):
        result['wounded_formula_reason']='invalid_or_incomplete_parameters'
    return result

def summarize(lines):
    # One log is one process session. Queue slots are flushed independently of IDs.
    headers=[e for e in lines if e.get('event')=='battle_probe_startup']
    if len(headers)>1:
        raise ValueError('Analyze each process/session log separately; do not merge campaigns.')
    events=sorted((e for e in lines if e.get('event')=='battle_observation'),key=lambda e:e['id'])
    by_id={e['id']:e for e in events}
    if len(by_id)!=len(events):
        raise ValueError('Duplicate event IDs in one session')
    counts=Counter(e['kind'] for e in events)
    fires={}; rows=[]; actors={}; prior_world=None; prior_day=None
    for e in events:
        kind=e['kind']; target=e.get('target',{}); source=e.get('source',{})
        after=e.get('target_after',{}); key=identity(target)
        row={'id':e['id'],'kind':kind,'planning_day':e.get('planning_day'),
             'source_name':source.get('name',''),'target_name':target.get('name',''),
             'source_unit_id':source.get('id'),'target_unit_id':target.get('id'),
             'action_id':e.get('action_id',0),'effect_id':e.get('effect_id',0),
             'tactic_id':e.get('tactic_id',-1),'tactic_name':e.get('tactic_name',''),
             'text':e.get('text',''),'attribution':'unknown'}
        if prior_world is not None and (e.get('world')!=prior_world or
                (e.get('planning_day',-1)>=0 and prior_day is not None and e['planning_day']<prior_day)):
            fires.clear()
        prior_world=e.get('world')
        if e.get('planning_day',-1)>=0: prior_day=e['planning_day']
        # Any observed movement or slot reuse invalidates that unit's old fire
        # association, including when it appears only as another attack's source.
        objects=[e.get(k,{}) for k in ('source','target','source_after','target_after')]
        for tile,fire in list(fires.items()):
            address,unit_id,_=fire['key']
            if any(o.get('kind')==2 and o.get('address')==address and o.get('id')==unit_id
                   and (identity(o)!=fire['key'] or o.get('tile')!=tile) for o in objects):
                del fires[tile]
        if kind=='hex_fire_transition':
            hex_id=e.get('hex_id',-1); before=e.get('fire_before',-1); final=e.get('fire_after',-1)
            effect=by_id.get(e.get('effect_id'),{})
            action=by_id.get(e.get('action_id'),{})
            primary_chain=(e.get('schema_version')==3 and e.get('caller_rva')=='0x2b17d4'
                           and identity(action.get('source',{}))==identity(source)
                           and identity(action.get('target',{}))==key
                           and effect.get('action_id')==action.get('id')
                           and effect.get('tactic_id',-1)==e.get('tactic_id',-2)
                           and effect.get('tactic_id',-1)>=0)
            verified_effect=(e.get('schema_version',3)>=4 and e.get('effect_category')==17
                             and e.get('source_context_verified')==1
                             and e.get('target_from_hex_occupant')==1)
            legacy_target=(e.get('schema_version',0)<3 and e.get('scope_matches_target')==1
                           and identity(effect.get('target',{}))==key
                           and 17 in effect.get('effect_categories',[]))
            # Schema 3 incorrectly labelled the descriptor's ten parameters as
            # categories, and its subject as the victim. Ignore those assumptions.
            eligible=(before==0 and final>0 and key and identity(source) is not None
                      and identity(effect.get('source',{}))==identity(source)
                      and e.get('post_identity_matches')==1
                      and (primary_chain or verified_effect or legacy_target))
            if eligible:
                fires[hex_id]={'key':key,'event_id':e['id'],'source':source,
                               'tactic_id':effect['tactic_id'],'tactic_name':effect.get('tactic_name','')}
                row['attribution']='native_skill_ignition_chain'
                row['target_basis']='hex_occupant_and_primary_action' if primary_chain else 'native_fire_target'
            elif final==0 or before<0 or final<0 or final>before:
                # Extinction or an extension/re-ignition with competing ownership invalidates credit.
                fires.pop(hex_id,None)
            row.update(hex_id=hex_id,fire_before=before,fire_after=final,
                       ignition_observed=before==0 and final>0)
        elif kind=='troop_change':
            stable=key is not None and e.get('post_identity_matches')==1 and identity(after)==key
            loss=max(0,target.get('troops',0)-after.get('troops',0)) if stable else None
            row.update(troops_before=target.get('troops'),troops_after=after.get('troops'),
                       observed_troop_loss=loss,field18_before_raw=target.get('field18_raw'),
                       field18_after_raw=after.get('field18_raw'),soldiers_killed=None,
                       soldiers_newly_wounded=None)
            row.update(native_wounded_formula(e,by_id))
            credited=None
            if stable and identity(source):
                row['attribution']='native_source';credited=source
            elif stable:
                effect=by_id.get(e.get('effect_id'),{})
                if (effect.get('target_role','affected_entity')!='dispatch_subject'
                    and e.get('schema_version',0)<3
                    and identity(effect.get('target',{}))==key and identity(effect.get('source',{}))):
                    row['attribution']='native_effect_context';credited=effect['source']
                    row.update(tactic_id=effect.get('tactic_id',-1),tactic_name=effect.get('tactic_name',''))
                else:
                    fire=fires.get(e.get('hex_id'))
                    if fire and (fire['key']!=key or e.get('fire_before',-1)<=0 or target.get('tile')!=e.get('hex_id')):
                        fires.pop(e.get('hex_id'),None);fire=None
                    if fire and e.get('caller_rva')=='0x3e916d':
                        # A candidate chain for controlled testing, never a career credit.
                        row.update(attribution='candidate_matching_fire_instance',
                                   candidate_source_name=fire['source'].get('name',''),
                                   ignition_event_id=fire['event_id'],tactic_id=fire['tactic_id'],
                                   tactic_name=fire['tactic_name'])
            if credited:
                row.update(source_name=credited.get('name',''),source_unit_id=credited.get('id'))
                aid=credited['leader_id'];a=actors.setdefault(aid,{'officer_id':aid,'name':credited.get('name',''),
                    'observed_attributed_troop_loss':0,'officer_kills':None,'officer_injuries':None,
                    'soldiers_killed':None,'soldiers_newly_wounded':None,'units_routed':None})
                a['observed_attributed_troop_loss']+=loss or 0
        elif kind=='army_abnormal_application':
            mode=e.get('abnormal_mode_raw',-1);before=e.get('abnormal_before',-1);final=e.get('abnormal_after',-1)
            categories={0:2,1:3,2:4};labels={0:'混乱（待界面核对）',1:'止步',2:'挑衅（待界面核对）'}
            effect=by_id.get(e.get('effect_id'),{});action=by_id.get(e.get('action_id'),{})
            stable=key is not None and identity(after)==key and e.get('post_identity_matches')==1
            chain=(identity(source) is not None and e.get('source_context_verified')==1
                   and identity(effect.get('source',{}))==identity(source)
                   and identity(action.get('source',{}))==identity(source)
                   and effect.get('action_id')==action.get('id')
                   and e.get('effect_category')==categories.get(mode,-2))
            row.update(abnormal_mode_raw=mode,abnormal_label=labels.get(mode,'未知'),
                       abnormal_before=before,abnormal_after=final,
                       abnormal_requested_duration=e.get('abnormal_requested_duration'),
                       abnormal_change_observed=bool(stable and before>=0 and final>=0 and before!=final),
                       abnormal_applied_or_extended=bool(stable and before>=0 and final>before))
            if chain and stable: row['attribution']='native_abnormal_target_and_source_chain'
        elif kind=='officer_injury':
            row.update(health_before_raw=target.get('health_raw'),health_after_raw=after.get('health_raw'),
                       injury_mode_raw=signed_arg(e,1),officer_injury_verified=False)
        elif kind=='person_status':
            row.update(status_before_raw=target.get('raw_status'),status_after_raw=after.get('raw_status'),
                       officer_death_verified=False)
        elif kind=='unit_remove':
            row.update(reason_raw=signed_arg(e,3),unit_rout_verified=False)
            source_after=e.get('source_after',{})
            row.update(source_field18_before_raw=source.get('field18_raw'),
                       source_field18_after_raw=source_after.get('field18_raw'),
                       captured_wounded_candidate=None)
            if identity(source) and identity(source_after)==identity(source):
                row['captured_wounded_candidate']=max(0,source_after.get('field18_raw',0)-source.get('field18_raw',0))
            for tile,fire in list(fires.items()):
                if fire['key']==key: del fires[tile]
        rows.append(row)
    health=[e for e in lines if e.get('event')=='battle_probe_health']
    faults=any(e.get('fault',0) or e.get('dropped',0) for e in health)
    return {'schema_version':5,'session':headers[0] if headers else None,'capture_fault_observed':faults,
            'complete_turn_verified':False,'campaign_save_branch_bound':False,
            'counts':dict(counts),'actors':list(actors.values()),'events':rows,
            'limitations':['Troop loss is not a verified death count.',
                          'field18 is an intermediate raw field, not confirmed new wounded.',
                          'Fire candidates are not credited to career totals.',
                          'Removal, injury and death reasons require controlled validation.',
                          'Logs survive restart but are not yet bound to save/rollback branches.']}

def markdown(report):
    out=['# 战斗采集报告','','这是事件观察报告，尚未计入武将生涯战绩。',
         '兵力减少不等于阵亡；伤兵原始字段、移除原因及火烧候选归属仍需验证。','',
         '| 事件 | 日期码 | 类型 | 施放者／来源 | 目标 | 战法／结果 | 归属依据 |',
         '|---|---|---|---|---|---|---|']
    def esc(v): return str(v if v is not None else '').replace('|','\\|').replace('\n',' ')
    for e in report['events']:
        source=e['source_name'] or e.get('candidate_source_name','') or '未知'
        detail=e.get('tactic_name','') or e.get('text','')
        if 'observed_troop_loss' in e: detail+=f" 兵力 {e['troops_before']}→{e['troops_after']}（减少 {e['observed_troop_loss']}）"
        if 'fire_before' in e: detail+=f" 火状态 {e['fire_before']}→{e['fire_after']}"
        if 'abnormal_before' in e: detail+=f" {e['abnormal_label']} {e['abnormal_before']}→{e['abnormal_after']}"
        if e.get('wounded_formula_verified'): detail+=f" 生成伤兵候选 {e['generated_wounded_candidate']}；扣减伤兵候选 {e['wounded_attrition_candidate']}"
        out.append('| '+' | '.join(esc(x) for x in (e['id'],e['planning_day'],e['kind'],source,e['target_name'],detail,e['attribution']))+' |')
    return '\n'.join(out)+'\n'

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('--output',type=Path,required=True);args=p.parse_args()
    report=summarize([json.loads(l) for l in args.log.read_text(encoding='utf-8-sig').splitlines() if l.strip()])
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    args.output.with_suffix('.md').write_text(markdown(report),encoding='utf-8')
    print(json.dumps({'report':str(args.output),'counts':report['counts'],'capture_fault_observed':report['capture_fault_observed']},ensure_ascii=False))
