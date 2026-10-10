#include "personality_edit.h"
int s14_personality_plan(const unsigned short slots[9],int add,int slot,int *target){
    if(!slots || !target || (add!=0 && add!=1))return S14_PE_FAILED;
    for(int i=0;i<9;i++){if(slots[i]>355)return S14_PE_FAILED;if(slots[i]==6)return S14_PE_DUPLICATE;}
    if(add){for(int i=0;i<9;i++)if(!slots[i]){*target=i;return S14_PE_PENDING;}return S14_PE_FULL;}
    if(slot<0 || slot>=9 || !slots[slot])return S14_PE_FAILED;*target=slot;return S14_PE_PENDING;
}
