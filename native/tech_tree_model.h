#ifndef S14_TECH_TREE_MODEL_H
#define S14_TECH_TREE_MODEL_H
#include "troop_registry.h"
#include <wchar.h>
enum {S14_TECH_NODES=16, S14_TECH_POINTS=8, S14_TECH_CHOICES=20+S14_TROOP_LIMIT};
typedef struct {const char *id;const wchar_t *name,*description;int parent,icon;} S14TechNode;
typedef struct {int attack_bp,defense_bp,mobility_bp,capacity,recovery_bp;} S14TechBonus;
typedef struct {char id[64],name[64];int carrier,is_mod;} S14TechTroop;
typedef struct S14TechRecord {char troop[64];unsigned short learned;int node;struct S14TechRecord *next;} S14TechRecord;
typedef struct S14TechOfficer {int id;char troop[64];S14TechRecord *records;struct S14TechOfficer *next;} S14TechOfficer;
typedef struct {S14TechOfficer *officers,*current;S14TechRecord *record;S14TechTroop choices[S14_TECH_CHOICES];int count,selected;} S14TechSession;
extern const S14TechNode s14_tech_nodes[S14_TECH_NODES];
int s14_tech_choices(int officer,unsigned permission,const S14TroopRegistry*,S14TechTroop*,int capacity);
int s14_tech_officer(S14TechSession*,int officer,unsigned permission,const S14TroopRegistry*);
int s14_tech_troop(S14TechSession*,int index);
int s14_tech_status(const S14TechRecord*,int node); /* 0 locked, 1 available, 2 learned */
int s14_tech_upgrade(S14TechSession*,int node);
int s14_tech_points(unsigned short learned);
int s14_tech_mask_valid(unsigned short learned);
S14TechBonus s14_tech_bonus(unsigned short learned);
void s14_tech_reset(S14TechSession*);
#endif
