#ifndef S14_ARMY_OBSERVER_H
#define S14_ARMY_OBSERVER_H
#include "army_model.h"
// Passive observations of calls made by the original detail page. Never calls
// native calculation routines independently and never replaces native results.
int s14_army_observer_install(uintptr_t,uintptr_t);
void s14_army_observer_configure(int);
int s14_army_observer_ready(void);
int s14_army_observer_snapshot(S14ArmyTrace*);
// Record results of naturally invoked attribute calls; used by the independent buff.
void s14_army_observer_attribute(int,void*,int,float,float,int);
#endif
