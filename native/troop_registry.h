#ifndef S14_TROOP_REGISTRY_H
#define S14_TROOP_REGISTRY_H
#include <stddef.h>
#include <stdint.h>

/* Plugin IDs are strings. They must never be written to native formation fields. */
enum { S14_TROOP_SCHEMA=1, S14_TROOP_LIMIT=256, S14_TROOP_COMMANDERS=64,
       S14_TROOP_EFFECTS=8, S14_TROOP_MAX_SOLDIERS=100000 };
/* Attribute array order matches S14_ARMY_*; presentation order is independent. */
enum { S14_TROOP_ATTACK, S14_TROOP_SIEGE_ATTACK, S14_TROOP_SIEGE_BREAK,
       S14_TROOP_MOBILITY, S14_TROOP_DEFENSE, S14_TROOP_ATTRIBUTES };
enum { S14_TROOP_ANY_COMMANDER, S14_TROOP_COMMANDER_WHITELIST };
enum { S14_TROOP_DAMAGE_REDUCTION=1, S14_TROOP_STATUS_IMMUNITY, S14_TROOP_SURROUND_IMMUNITY };
enum { S14_TROOP_CONFUSION=1 };
enum { S14_TROOP_CAP_BINDING=1, S14_TROOP_CAP_SELECTION_UI=2,
       S14_TROOP_CAP_ATTRIBUTE=4, S14_TROOP_CAP_GOLD_COMMIT=8,
       S14_TROOP_CAP_DAMAGE_REDUCTION=16, S14_TROOP_CAP_STATUS_IMMUNITY=32,
       S14_TROOP_CAP_SURROUND_IMMUNITY=64 };
typedef enum {
    S14_TROOP_OK, S14_TROOP_INVALID, S14_TROOP_DUPLICATE,
    S14_TROOP_FULL, S14_TROOP_SEALED, S14_TROOP_NO_MEMORY,
    S14_TROOP_NOT_FOUND, S14_TROOP_COMMANDER_DENIED, S14_TROOP_LOCKED,
    S14_TROOP_NATIVE_DENIED, S14_TROOP_CARRIER_MISMATCH,
    S14_TROOP_SOLDIERS_INVALID, S14_TROOP_GOLD_INSUFFICIENT
} S14TroopResult;
typedef struct {
    int kind, status;
    int value_bp; /* 10000 basis points = 100%; descriptors, not applied effects. */
} S14TroopEffect;
typedef struct {
    char id[64], name[64], description[384], unlock_key[64], icon[128]; /* UTF-8 */
    uint32_t revision;
    uint32_t max_soldiers; /* 0 uses the registry's general 100000 ceiling. */
    int native_carrier; /* Existing legal native ID 1..20, not a new native ID. */
    int commander_scope, commander_count, commanders[S14_TROOP_COMMANDERS];
    int bonus_bp[S14_TROOP_ATTRIBUTES];
    int effect_count;
    S14TroopEffect effects[S14_TROOP_EFFECTS];
    uint64_t fixed_gold, gold_per_1000;
} S14TroopDefinition;
typedef struct S14TroopRegistry S14TroopRegistry;
typedef struct {
    int commander_id, native_carrier, native_allowed, unlocked;
    uint32_t soldiers;
    uint64_t treasury;
} S14TroopRequest;
typedef struct {
    uint64_t fixed_gold, variable_gold, extra_gold;
    uint32_t soldier_groups;
    S14TroopResult selection_result;
} S14TroopQuote;

/* Builder is single-threaded; seal before publishing to concurrent readers.
   Entries are deep-copied. Pointers returned by at/find are stable after seal,
   and valid until destruction. Destroy only after all readers have stopped. */
S14TroopRegistry *s14_troop_registry_create(void);
void s14_troop_registry_destroy(S14TroopRegistry *registry);
S14TroopResult s14_troop_registry_register(S14TroopRegistry *registry,
                                         const S14TroopDefinition *definition);
S14TroopResult s14_troop_registry_seal(S14TroopRegistry *registry);
size_t s14_troop_registry_count(const S14TroopRegistry *registry);
const S14TroopDefinition *s14_troop_registry_at(const S14TroopRegistry *registry, size_t index);
const S14TroopDefinition *s14_troop_registry_find(const S14TroopRegistry *registry, const char *id);
/* Generated from data/troops.json; immutable, never destroy or cast to writable. */
const S14TroopRegistry *s14_troop_builtin_registry(void);
S14TroopResult s14_troop_validate(const S14TroopDefinition *definition);
const char *s14_troop_result_name(S14TroopResult result);
const char *s14_troop_native_carrier_name(int id);
int s14_troop_commander_allowed(const S14TroopDefinition *definition, int commander);

/* Quote and plan are pure. They do not unlock, bind units, deduct funds, call
   the game, or alter native attributes. Caller must explicitly select an ID.
   Quote concerns EXTRA gold only, in addition to any native deployment cost. */
S14TroopResult s14_troop_quote(const S14TroopRegistry *registry, const char *selected_id,
                             const S14TroopRequest *request, S14TroopQuote *quote);
S14TroopResult s14_troop_plan_attributes(const S14TroopRegistry *registry, const char *selected_id,
                                       const S14TroopRequest *request,
                                       const double native_values[S14_TROOP_ATTRIBUTES],
                                       double planned_values[S14_TROOP_ATTRIBUTES]);
unsigned s14_troop_required_capabilities(const S14TroopDefinition *definition);
unsigned s14_troop_missing_capabilities(const S14TroopDefinition *definition, unsigned available);
#endif
