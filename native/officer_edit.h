#ifndef S14_OFFICER_EDIT_H
#define S14_OFFICER_EDIT_H
#include <windows.h>
#include <stdint.h>
/* Versioned edit envelope. Every operation has its own capability; a discovered
   type or readable field does not imply a safe writable operation. */
enum {S14_EDIT_PERSONALITIES=0,S14_EDIT_ATTRIBUTES,S14_EDIT_TACTICS,S14_EDIT_RANK,
      S14_EDIT_LIKES,S14_EDIT_DISLIKES,S14_EDIT_SWORN,S14_EDIT_BOOKS,S14_EDIT_ITEMS,S14_EDIT_OPERATION_COUNT};
enum {S14_EDIT_QUEUED=1,S14_EDIT_SUCCESS,S14_EDIT_EXPIRED,S14_EDIT_INCOMPATIBLE,S14_EDIT_FAILED};
typedef struct {
    unsigned version,epoch;
    uintptr_t world,person;
    int officer;
    unsigned char raw[512];
} S14EditSnapshot;
typedef struct {
    unsigned version;
    int operation;
    S14EditSnapshot before;
    unsigned short personalities[9];
} S14EditRequest;
typedef struct {
    uint64_t id;
    int status,operation,officer,changed,verified;
    wchar_t detail[192];
} S14EditResult;
typedef struct {
    int operation,available;
    wchar_t reason[192];
} S14EditCapability;
int s14_edit_capability(int operation);
int s14_edit_capability_info(int operation,S14EditCapability *out);
int s14_edit_capture(uintptr_t world,int officer,S14EditSnapshot *out);
int s14_edit_submit(const S14EditRequest *request,uint64_t *id);
int s14_edit_result(uint64_t id,S14EditResult *out);
int s14_edit_personality_validate(const unsigned short before[9],const unsigned short after[9]);
#endif
