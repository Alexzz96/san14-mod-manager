#define WIN32_LEAN_AND_MEAN
#include "troop_store.h"
#include <string.h>
#include <stdlib.h>
uint32_t s14_troop_crc(const void *raw,size_t n){uint32_t x=~0u;const unsigned char *p=raw;while(n--){x^=*p++;for(int k=0;k<8;k++)x=(x>>1)^(0xedb88320u&-(int)(x&1));}return ~x;}
static int valid_id(const char id[64]){int n=0;while(n<64 && id[n]){unsigned char c=id[n++];if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))return 0;}return n>0 && n<64;}
static int valid_state(const S14TroopState *s){
    if(s->world || s->units[0].active)return 0;
    for(int i=1;i<=500;i++){const S14TroopBinding *b=&s->units[i];if(b->active && (b->active!=1 || b->leader<1 || b->leader>6000 || b->carrier<1 || b->carrier>20 || b->serial<0 || b->serial>65535 || b->home<0 || b->home>65535 || !b->revision || !valid_id(b->id)))return 0;}
    for(int i=0;i<64;i++){const S14TroopIntent *b=&s->pending[i];if(b->active && (b->active!=1 || b->leader<1 || b->leader>6000 || b->carrier<1 || b->carrier>20 || b->water<0 || b->water>20 || b->soldiers<1 || b->soldiers>65535 || b->home<0 || b->home>65535 || !b->revision || !valid_id(b->id)))return 0;}
    return 1;
}
int s14_troop_checkpoint_read(const wchar_t *path,S14TroopCheckpoint *c){
    DWORD attr=GetFileAttributesW(path);if(!c || attr==INVALID_FILE_ATTRIBUTES || attr&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))return 0;
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,NULL);if(f==INVALID_HANDLE_VALUE)return 0;DWORD n;LARGE_INTEGER size;BY_HANDLE_FILE_INFORMATION info;
    int ok=GetFileInformationByHandle(f,&info) && info.nNumberOfLinks==1 && !(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) && GetFileSizeEx(f,&size)&&size.QuadPart==sizeof(*c)&&ReadFile(f,c,sizeof(*c),&n,NULL)&&n==sizeof(*c);CloseHandle(f);
    return ok && !memcmp(c->magic,"S14TROOPS.v1",12) && c->version==1 && c->bytes==sizeof(*c) && c->crc==s14_troop_crc(c->hash,sizeof(*c)-offsetof(S14TroopCheckpoint,hash)) && valid_state(&c->state);
}
int s14_troop_checkpoint_owned(const wchar_t *path){S14TroopCheckpoint *c=malloc(sizeof(*c));int ok=c && s14_troop_checkpoint_read(path,c);free(c);return ok;}
