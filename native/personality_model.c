#include "personality_edit.h"
#include "officer_edit.h"
int s14_edit_personality_validate(const unsigned short before[9],const unsigned short after[9]){
    if(!before || !after)return S14_PE_FAILED;
    for(int i=0;i<9;i++){
        if(before[i]>355 || after[i]>355)return S14_PE_FAILED;
        if(after[i] && before[i]!=after[i])for(int j=0;j<9;j++)if(i!=j && after[i]==after[j])return S14_PE_DUPLICATE;
    }
    return S14_PE_PENDING;
}
int s14_personality_plan(const unsigned short slots[9],int add,int slot,int *target){
    if(!slots || !target || (add!=0 && add!=1))return S14_PE_FAILED;
    for(int i=0;i<9;i++){if(slots[i]>355)return S14_PE_FAILED;if(slots[i]==6)return S14_PE_DUPLICATE;}
    if(add){for(int i=0;i<9;i++)if(!slots[i]){*target=i;return S14_PE_PENDING;}return S14_PE_FULL;}
    if(slot<0 || slot>=9 || !slots[slot])return S14_PE_FAILED;*target=slot;return S14_PE_PENDING;
}
