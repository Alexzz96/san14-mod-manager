#ifndef S14_TROOP_STORE_H
#define S14_TROOP_STORE_H
#include <windows.h>
#include <stdint.h>
#include <stddef.h>
typedef struct {int active,leader,carrier,serial,home;char id[64];uint32_t revision;} S14TroopBinding;
typedef struct {int active,leader,carrier,soldiers,water,number,home;char id[64];uint32_t revision;} S14TroopIntent;
typedef struct {uint64_t world;S14TroopBinding units[501];S14TroopIntent pending[64];} S14TroopState;
typedef struct {char magic[16];uint32_t version,bytes,crc;unsigned char hash[32];S14TroopState state;} S14TroopCheckpoint;
uint32_t s14_troop_crc(const void *raw,size_t n);
int s14_troop_checkpoint_read(const wchar_t *path,S14TroopCheckpoint *out);
int s14_troop_checkpoint_owned(const wchar_t *path);
#endif
