#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
#define S14_ARMY_BUFF_TEST
#include "army_buff.c"
#include <stdlib.h>
static int call_count,record_count,last_percent,last_attribute,last_actual,change_identity;
static float native_value=2696.43457f,last_native,last_final;
static void *expected_army,*expected_target,*expected_formation;
static int expected_actual,expected_mode;
static void put64(unsigned char *p,size_t at,uintptr_t v){memcpy(p+at,&v,8);}
static void put16(unsigned char *p,size_t at,unsigned short v){memcpy(p+at,&v,2);}
void s14_army_observer_attribute(int i,void *army,int actual,float native,float final,int percent){
    assert(army==expected_army);record_count++;last_percent=percent;last_attribute=i;last_actual=actual;last_native=native;last_final=final;
}
static float native_attack_fixture(float base,void *army,void *target,void *formation,int actual,int mode){
    assert(base==157.25f && army==expected_army && target==expected_target && formation==expected_formation && actual==expected_actual && mode==expected_mode);
    call_count++;if(change_identity)put16(army,0x12,511);SetLastError(0x7788);return native_value;
}
static float native_defense_fixture(float base,void *army,void *target,void *formation,int actual,int mode){
    assert(base==157.25f && army==expected_army && target==expected_target && formation==expected_formation && actual==expected_actual && mode==expected_mode);
    call_count++;SetLastError(0x7788);return native_value;
}
static float attack(int actual,int mode){expected_actual=actual;expected_mode=mode;return buff_attack_hook(157.25f,expected_army,expected_target,expected_formation,actual,mode);}
static float defense(int actual,int mode){expected_actual=actual;expected_mode=mode;return buff_defense_hook(157.25f,expected_army,expected_target,expected_formation,actual,mode);}
static void entry_install_test(const char *attack_bytes,const char *defense_bytes){
    unsigned char *private_image=VirtualAlloc(NULL,0x300000,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE);assert(private_image);
    const char *bytes[]={attack_bytes,defense_bytes};const uintptr_t rvas[]={0x283ad0,0x27bd90};
    for(int i=0;i<2;i++){assert(strlen(bytes[i])==128);for(int j=0;j<64;j++){unsigned value;assert(sscanf(bytes[i]+j*2,"%2x",&value)==1);private_image[rvas[i]+j]=(unsigned char)value;}}
    assert(MH_Initialize()==MH_OK);
    private_image[0x27bd90]^=1;assert(!s14_army_buff_install((uintptr_t)private_image,(uintptr_t)private_image+0x300000));
    assert(!s14_army_buff_ready() && s14_army_buff_install_code()==-2 && s14_army_buff_failed_entry()==0x27bd90);
    private_image[0x27bd90]^=1;void *unused=NULL;
    assert(MH_CreateHook(private_image+0x27bd90,native_defense_fixture,&unused)==MH_OK);
    assert(!s14_army_buff_install((uintptr_t)private_image,(uintptr_t)private_image+0x300000));
    assert(!s14_army_buff_ready() && s14_army_buff_install_code()==MH_ERROR_ALREADY_CREATED && s14_army_buff_failed_entry()==0x27bd90);
    assert(MH_CreateHook(private_image+0x283ad0,native_attack_fixture,&unused)==MH_OK);assert(MH_RemoveHook(private_image+0x283ad0)==MH_OK);
    assert(MH_RemoveHook(private_image+0x27bd90)==MH_OK);
    assert(s14_army_buff_install((uintptr_t)private_image,(uintptr_t)private_image+0x300000));
    assert(s14_army_buff_ready() && !s14_army_buff_install_code() && !s14_army_buff_failed_entry());
    assert(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);assert(MH_DisableHook(MH_ALL_HOOKS)==MH_OK && MH_Uninitialize()==MH_OK);
    InterlockedExchange(&buff_ready,0);VirtualFree(private_image,0,MEM_RELEASE);
}
int main(int argc,char **argv){
    entry_install_test(argc==3?argv[1]:"48895c2408565741564881ec60020000440f298c2420020000488b05c0f469014833c44889842410020000488bca498bd9498bf0488bfa440f28c8e8c0ee0600",
                       argc==3?argv[2]:"4055564154415641574881ec90020000440f299c2420020000488b0500726a014833c44889842410020000488bca498be94d8bf0488bf2440f28d8e8006c0700");
    unsigned char *image=VirtualAlloc(NULL,0x2020000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE),*world=calloc(1,0x85200),*pool=calloc(3,512);assert(image && world && pool);
    buff_image=(uintptr_t)image;put64(image,0x1fc91d0,(uintptr_t)world);put64(world,0x7df60,(uintptr_t)pool);put64(world,0x7df68,(uintptr_t)(pool+512));put64(world,0x7df70,(uintptr_t)(pool+1024));
    put64(pool+512,0,buff_image+0x123e288);pool[512+0x10]=1;put16(pool+512,0x12,518);
    put64(pool+1024,0,buff_image+0x123e288);pool[1024+0x10]=1;put16(pool+1024,0x12,511);
    expected_army=pool+512;expected_target=pool+1024;expected_formation=(void*)0x76543210;buff_original_attack=native_attack_fixture;buff_original_defense=native_defense_fixture;
    InterlockedExchange(&buff_ready,1);s14_army_buff_configure(0);
    assert(attack(1,0)==native_value && GetLastError()==0x7788 && last_percent==0);
    s14_army_buff_configure(1);unsigned epoch=s14_affix_epoch();
    assert(s14_affix_publish((uintptr_t)world,4999,1,1,0,10,epoch,0));assert(attack(1,0)==native_value && !last_percent);
    assert(s14_affix_publish((uintptr_t)world,5000,1,1,0,20,epoch,0));float raised=native_value*1.10f;
    assert(attack(0,0)==native_value && GetLastError()==0x7788 && last_percent==0 && last_actual==0);
    assert(attack(2,0)==native_value && last_percent==0);
    int before=call_count;for(int i=0;i<300;i++)assert(attack(1,i%2)==raised && last_percent==10 && GetLastError()==0x7788);
    assert(call_count==before+300 && last_native==native_value && last_final==raised && (int)raised==2966);
    expected_target=NULL;assert(attack(1,0)==raised);expected_target=pool+1024;
    native_value=1285.01648f;assert(defense(1,0)==native_value*1.10f && last_attribute==4 && last_percent==10 && (int)last_final==1413);
    s14_army_buff_configure(0);assert(defense(1,0)==native_value && last_percent==0);s14_army_buff_configure(1);
    expected_army=pool+1024;assert(attack(1,0)==native_value && last_percent==0);expected_army=pool+512;
    pool[512+0x10]=0;assert(attack(1,0)==native_value);pool[512+0x10]=1;
    unsigned char fake_unit[512];memcpy(fake_unit,pool+512,512);expected_army=fake_unit;assert(attack(1,0)==native_value);expected_army=pool+512;
    put64(world,0x7df68,(uintptr_t)(pool+1024));assert(attack(1,0)==native_value);put64(world,0x7df68,(uintptr_t)(pool+512));
    change_identity=1;assert(attack(1,0)==native_value && last_percent==0);change_identity=0;put16(pool+512,0x12,518);
    native_value=INFINITY;assert(isinf(attack(1,0)) && !last_percent);native_value=NAN;assert(isnan(attack(1,0)) && !last_percent);native_value=-5;assert(attack(1,0)==-5 && !last_percent);
    native_value=10;buff_test_caller=buff_image+0x15cb70;assert(attack(1,0)==11);char line[768];assert(s14_army_buff_next_log(line,sizeof(line)) && strstr(line,"\"combat_path\":true") && strstr(line,"\"caller_rva\":\"0x15cb70\""));assert(!s14_army_buff_next_log(line,64));buff_test_caller=0;
    for(int i=0;i<100;i++){buff_test_caller=buff_image+0x15c99a;assert(defense(1,0)==11);}assert(sample_dropped>0);int drained=0;while(s14_army_buff_next_log(line,sizeof(line)))drained++;assert(drained==64);buff_test_caller=0;
    // Real native ABI through MinHook, against private synthetic functions only.
    assert(MH_Initialize()==MH_OK);assert(MH_CreateHook((void*)native_attack_fixture,buff_attack_hook,(void**)&buff_original_attack)==MH_OK);
    assert(MH_CreateHook((void*)native_defense_fixture,buff_defense_hook,(void**)&buff_original_defense)==MH_OK);assert(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);
    AttributeCall volatile call_attack=native_attack_fixture,call_defense=native_defense_fixture;expected_actual=1;expected_mode=1;
    before=call_count;assert(call_attack(157.25f,expected_army,expected_target,expected_formation,1,1)==11 && GetLastError()==0x7788);assert(call_count==before+1);
    expected_mode=0;before=call_count;assert(call_defense(157.25f,expected_army,expected_target,expected_formation,1,0)==11 && GetLastError()==0x7788);assert(call_count==before+1);
    assert(MH_DisableHook(MH_ALL_HOOKS)==MH_OK && MH_Uninitialize()==MH_OK);assert(record_count==call_count);
    buff_original_attack=native_attack_fixture;buff_original_defense=native_defense_fixture;
    unsigned load_epoch=s14_affix_suspend();assert(attack(1,0)==native_value && !last_percent);
    assert(s14_affix_publish((uintptr_t)world,4999,1,1,0,10,load_epoch,1));assert(attack(1,0)==native_value && !last_percent);
    /* AI affix uses the same actual getter and the same max category, even
       with Cao Ren/battle collection/special troop switches disabled. */
    unsigned char person[512]={0},group[64]={0},settings[128]={0};
    put64(world,0x148+518*8,(uintptr_t)person);put64(person,0,buff_image+0x12a00d0);put16(person,0x10,518);person[0x118]=2;
    put64(world,0xde40+2*8,(uintptr_t)group);put64(group,0,buff_image+0x129fec8);group[0x10]=3;settings[0x3a]=1;put64(world,0x85130,(uintptr_t)settings);
    wchar_t temp[MAX_PATH];assert(GetTempPathW(MAX_PATH,temp));s14_ai_attach(buff_image,temp,1);unsigned char hash[32]={77};s14_ai_load_begin();s14_ai_load_end((uintptr_t)world,1,hash,1);s14_ai_configure(1);
    S14AiDraw candidate={.eligible=1,.hit=1,.world=(uintptr_t)world,.leader=518};s14_ai_creation_begin(&candidate);
    native_value=100;s14_army_buff_configure(0);assert(attack(1,0)==110 && last_percent==10);assert(attack(0,0)==100 && !last_percent);
    s14_army_buff_configure(1);assert(s14_affix_publish((uintptr_t)world,5000,1,1,0,20,load_epoch,1));
    for(int i=0;i<100;i++)assert(attack(1,0)==110 && defense(1,0)==110); // Two +10% bonuses never become +20%/+21%.
    group[0x10]=1;s14_army_buff_configure(0);assert(attack(1,0)==100);group[0x10]=3;
    s14_ai_creation_end();assert(attack(1,0)==100);s14_ai_configure(0);
    free(world);free(pool);VirtualFree(image,0,MEM_RELEASE);
    puts("{\"status\":\"passed\",\"commander_only\":true,\"canonical_live_army_only\":true,\"baseline_unchanged\":true,\"disabled_restores_native\":true,\"independent_of_detail_window\":true,\"no_compounding\":true,\"identity_change_rejected\":true,\"nonfinite_preserved\":true,\"native_float_and_stack_abi\":true,\"original_calls_once\":true,\"last_error_preserved\":true,\"combat_event_queue_bounded\":true,\"rounding_before_integer_conversion\":true,\"production_install_entry_validation\":true,\"failed_creation_rolled_back\":true}");return 0;
}
