import copy
import unittest
from summarize_battle import summarize,native_wounded_formula

def wound_fixture(loss=2012,before=0,after=212,bits='0x3e828f5c'):
    a=army(1,'曹仁');b=army(2,'张嶷');b['field18_raw']=before
    final=copy.deepcopy(b);final['troops']-=loss;final['field18_raw']=after
    e=dict(id=1,kind='troop_change',schema_version=5,caller_rva='0x166861',source=a,target=b,target_after=final,
           post_identity_matches=1,wound_rate_count=1,attrition_parameters_read=1,wound_rate_float_bits=bits,
           attrition_percent_raw=15,attrition_divisor_float_bits='0x42c80000',args=['0x1b'])
    r=dict(id=2,kind='wounded_generation_ratio',parent_id=1,caller_rva='0x16accb',args=['0x0','0x0','0x1b'],
           wound_rate_float_bits=bits,attrition_percent_raw=15,attrition_divisor_float_bits='0x42c80000')
    return e,{1:e,2:r}

def army(n,name,tile=101,troops=5000):
    return dict(kind=2,id=n,leader_id=10*n,address=hex(n*512),active=1,tile=tile,name=name,troops=troops,field18_raw=0)

def fixture():
    a=army(1,'张嶷',100);b=army(2,'曹仁')
    common=dict(event='battle_observation',world='0x1000',planning_day=100,post_identity_matches=1,clock_raw='010203040506')
    effect=dict(common,id=1,kind='skill_target_effect',source=a,target=b,tactic_id=5,tactic_name='火矢',effect_categories=[17],action_id=1)
    fire=dict(common,id=2,kind='hex_fire_transition',source=a,target=b,hex_id=101,fire_before=0,fire_after=120,effect_id=1,scope_matches_target=1)
    damage=dict(common,id=3,kind='troop_change',source={},target=b,target_after=army(2,'曹仁',troops=4912),hex_id=101,fire_before=119,caller_rva='0x3e916d')
    return [effect,fire,damage]

class Tests(unittest.TestCase):
    def test_float32_native_generation_and_attrition(self):
        e,by_id=wound_fixture();r=native_wounded_formula(e,by_id)
        self.assertTrue(r['wounded_formula_verified']);self.assertEqual(r['generated_wounded_candidate'],513)
        self.assertEqual(r['wounded_attrition_candidate'],301);self.assertEqual(r['surviving_field18_raw'],212)
    def test_negative_net_wounded_still_generated(self):
        e,by_id=wound_fixture(81,164,159,'0x3db851ec');r=native_wounded_formula(e,by_id)
        self.assertTrue(r['wounded_formula_verified']);self.assertEqual(r['generated_wounded_candidate'],7)
        self.assertEqual(r['wounded_attrition_candidate'],12)
    def test_wound_formula_rejects_fire_and_unmatched_path(self):
        for field,value in [('caller_rva','0x3e916d'),('source',{}),('wound_rate_count',2),('attrition_parameters_read',0),('wound_rate_float_bits','0x7fc12345')]:
            e,by_id=wound_fixture();e[field]=value;self.assertFalse(native_wounded_formula(e,by_id)['wounded_formula_verified'])
        e,by_id=wound_fixture();by_id[2]['caller_rva']='0x16b05e';self.assertFalse(native_wounded_formula(e,by_id)['wounded_formula_verified'])
    def test_wound_formula_requires_matching_outcome(self):
        e,by_id=wound_fixture(after=213);r=native_wounded_formula(e,by_id)
        self.assertFalse(r['wounded_formula_verified']);self.assertIsNone(r['generated_wounded_candidate'])
    def test_actual_abnormal_target_success_and_failure(self):
        a=army(1,'王威');b=army(2,'罗宪');e=dict(event='battle_observation',kind='army_abnormal_application',id=3,source=a,target=b,target_after=b,
            effect_id=2,action_id=1,source_context_verified=1,post_identity_matches=1,effect_category=3,abnormal_mode_raw=1,abnormal_before=0,abnormal_after=15)
        action=dict(event='battle_observation',id=1,kind='skill_action_attempt',source=a)
        effect=dict(event='battle_observation',id=2,kind='skill_effect_dispatch',source=a,target=a,action_id=1)
        r=summarize([e,effect,action])['events'][-1];self.assertEqual(r['target_name'],'罗宪')
        self.assertEqual(r['attribution'],'native_abnormal_target_and_source_chain');self.assertTrue(r['abnormal_applied_or_extended'])
        e['abnormal_after']=0;self.assertFalse(summarize([e,effect,action])['events'][-1]['abnormal_applied_or_extended'])
        action['source']=b;self.assertEqual(summarize([e,effect,action])['events'][-1]['attribution'],'unknown')
    def test_fire_candidate_no_career_credit(self):
        r=summarize(fixture());e=r['events'][-1]
        self.assertEqual(e['attribution'],'candidate_matching_fire_instance');self.assertEqual(e['candidate_source_name'],'张嶷')
        self.assertEqual(e['observed_troop_loss'],88);self.assertIsNone(e['soldiers_killed']);self.assertEqual(r['actors'],[])
    def test_real_schema3_dispatch_subject_is_caster(self):
        f=fixture();f[0].update(schema_version=3,action_id=10,target=copy.deepcopy(f[0]['source']),effect_categories=[3,40,10,130,130,163,160,40,40,8])
        f[1].update(schema_version=3,action_id=10,tactic_id=5,scope_matches_target=0,caller_rva='0x2b17d4')
        action=copy.deepcopy(f[0]);action.update(id=10,kind='skill_action_attempt',target=copy.deepcopy(f[1]['target']))
        f.append(action);r=summarize(f)
        damage=next(e for e in r['events'] if e['kind']=='troop_change')
        self.assertEqual(damage['attribution'],'candidate_matching_fire_instance')
        # Exact primary victim/source must still match; descriptor alone is insufficient.
        f[-1]['target']=army(3,'别的部队');r=summarize(f)
        self.assertEqual(next(e for e in r['events'] if e['kind']=='troop_change')['attribution'],'unknown')
    def test_native_schema4_fire_targets_occupant_not_subject(self):
        f=fixture();f[0].update(schema_version=4,kind='skill_effect_dispatch',target_role='dispatch_subject',target=copy.deepcopy(f[0]['source']))
        f[1].update(schema_version=4,scope_matches_target=0,effect_category=17,source_context_verified=1,target_from_hex_occupant=1)
        self.assertEqual(summarize(f)['events'][-1]['attribution'],'candidate_matching_fire_instance')
        for key in ('source_context_verified','target_from_hex_occupant'):
            broken=copy.deepcopy(f);broken[1][key]=0
            self.assertEqual(summarize(broken)['events'][-1]['attribution'],'unknown')
    def test_dispatch_subject_cannot_receive_damage_credit(self):
        f=fixture();f[0].update(schema_version=4,target_role='dispatch_subject',target=copy.deepcopy(f[-1]['target']))
        f[-1].update(schema_version=4,effect_id=1)
        f[1]['scope_matches_target']=0
        self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_captured_wounded_is_separate_from_new_wounds(self):
        a=army(1,'胜方');b=army(2,'败方',troops=0);after=copy.deepcopy(a);after['field18_raw']=263
        r=summarize([dict(event='battle_observation',id=1,kind='unit_remove',source=a,source_after=after,target=b,args=['0x0']*3+['0x1'])])
        self.assertEqual(r['events'][0]['captured_wounded_candidate'],263)
        self.assertFalse(r['events'][0]['unit_rout_verified'])
    def test_nested_native_effect(self):
        f=fixture();f[-1]['effect_id']=1;r=summarize(f)
        self.assertEqual(r['events'][-1]['attribution'],'native_effect_context');self.assertEqual(r['actors'][0]['observed_attributed_troop_loss'],88)
        self.assertIsNone(r['actors'][0]['soldiers_newly_wounded'])
    def test_other_tile_not_attributed(self):
        f=fixture();f[1]['scope_matches_target']=0;self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_other_skill_not_fire(self):
        f=fixture();f[0]['effect_categories']=[4];self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_failed_fire(self):
        f=fixture();f[1]['fire_after']=0;self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_extinction_invalidates(self):
        f=fixture();e=copy.deepcopy(f[1]);e.update(id=3,fire_before=120,fire_after=0);f[-1]['id']=4;f.insert(2,e)
        self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_competing_extension_invalidates(self):
        f=fixture();e=copy.deepcopy(f[1]);e.update(id=3,fire_before=120,fire_after=180,scope_matches_target=0);f[-1]['id']=4;f.insert(2,e)
        self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_move_or_leader_reuse_invalidates(self):
        for field,value in [('tile',102),('leader_id',33)]:
            f=fixture();f[-1]['target']=copy.deepcopy(f[-1]['target']);f[-1]['target'][field]=value;f[-1]['target_after'][field]=value
            self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_leave_and_return_invalidates_even_when_seen_only_as_source(self):
        f=fixture();f[-1]['id']=4
        moved=copy.deepcopy(f[0]);moved.update(id=3,kind='native_log',source=army(2,'曹仁',tile=102),target={})
        f.insert(2,moved)
        self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_damage_branch_must_match(self):
        f=fixture();f[-1]['caller_rva']='0x123';self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_status_and_remove_never_auto_credit(self):
        f=fixture();f.extend([dict(event='battle_observation',kind='person_status',id=4,target={'raw_status':1},target_after={'raw_status':9}),
                            dict(event='battle_observation',kind='unit_remove',id=5,args=['0x0']*3+['0xffffffffffffffff'])])
        r=summarize(f);self.assertFalse(r['events'][-2]['officer_death_verified']);self.assertFalse(r['events'][-1]['unit_rout_verified'])
        self.assertEqual(r['events'][-1]['reason_raw'],-1)
    def test_rollback_invalidates(self):
        f=fixture();f[-1]['planning_day']=99;self.assertEqual(summarize(f)['events'][-1]['attribution'],'unknown')
    def test_order_and_duplicates(self):
        self.assertEqual(summarize(fixture())['events'],summarize(fixture()[::-1])['events'])
        with self.assertRaises(ValueError):summarize(fixture()+[fixture()[0]])
    def test_faults_are_reported(self):
        f=fixture()+[dict(event='battle_probe_health',fault=1,dropped=1)];self.assertTrue(summarize(f)['capture_fault_observed'])

if __name__=='__main__':unittest.main()
