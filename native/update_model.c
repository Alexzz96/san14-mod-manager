// Bounded JSON reader for the public GitHub Releases API. No substring scraping.
#include "update.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
typedef struct {size_t start,end;int next,type;} Token;
typedef struct {const char *s;size_t size,pos;Token *t;int count;} Parser;
static void space(Parser *p){while(p->pos<p->size && p->s[p->pos] && strchr(" \t\r\n",p->s[p->pos]))p->pos++;}
static int value(Parser *p,int depth){
    space(p);if(depth>48 || p->count>=8192 || p->pos>=p->size)return -1;
    int id=p->count++;Token *t=&p->t[id];t->start=p->pos;t->type=p->s[p->pos++];
    if(t->type=='{' || t->type=='['){
        int close=t->type=='{'?'}':']';space(p);
        if(p->pos<p->size && p->s[p->pos]==close)p->pos++;
        else for(;;){
            if(t->type=='{'){
                space(p);if(p->pos>=p->size || p->s[p->pos]!='"' || value(p,depth+1)<0)return -1;
                space(p);if(p->pos>=p->size || p->s[p->pos++]!=':')return -1;
            }
            if(value(p,depth+1)<0)return -1;space(p);
            if(p->pos>=p->size)return -1;int c=p->s[p->pos++];if(c==close)break;if(c!=',')return -1;
        }
    }else if(t->type=='"'){
        int done=0;while(p->pos<p->size){unsigned char c=p->s[p->pos++];if(c=='"'){done=1;break;}
            if(c<32)return -1;if(c=='\\'){
                if(p->pos>=p->size)return -1;c=p->s[p->pos++];
                if(c=='u'){for(int i=0;i<4;i++)if(p->pos>=p->size || !isxdigit((unsigned char)p->s[p->pos++]))return -1;}
                else if(!strchr("\"\\/bfnrt",c))return -1;
            }
        }if(!done)return -1;
    }else{
        while(p->pos<p->size && !strchr(" \t\r\n,]}",p->s[p->pos]))p->pos++;
        size_t n=p->pos-t->start;const char *s=p->s+t->start;
        int literal=(n==4 && (!memcmp(s,"true",4)||!memcmp(s,"null",4))) || (n==5 && !memcmp(s,"false",5));
        if(!literal){size_t i=0;if(s[i]=='-')i++;if(i>=n)return -1;
            if(s[i]=='0')i++;else{if(s[i]<'1'||s[i]>'9')return -1;while(i<n && isdigit((unsigned char)s[i]))i++;}
            if(i<n && s[i]=='.'){i++;size_t begin=i;while(i<n && isdigit((unsigned char)s[i]))i++;if(i==begin)return -1;}
            if(i<n && (s[i]=='e'||s[i]=='E')){i++;if(i<n && (s[i]=='+'||s[i]=='-'))i++;size_t begin=i;while(i<n && isdigit((unsigned char)s[i]))i++;if(i==begin)return -1;}
            if(i!=n)return -1;
        }
    }
    t->end=p->pos;t->next=p->count;return id;
}
static int equal(Parser *p,int id,const char *s){if(id<0)return 0;Token *t=&p->t[id];return t->type=='"' && t->end-t->start==strlen(s)+2 && !memcmp(p->s+t->start+1,s,strlen(s));}
static int field(Parser *p,int object,const char *key){
    if(object<0 || p->t[object].type!='{')return -1;int found=-1;
    for(int i=object+1;i<p->t[object].next;){int v=i+1;if(equal(p,i,key)){if(found!=-1)return -1;found=v;}i=p->t[v].next;}return found;
}
static int text(Parser *p,int id,char *out,size_t cap){
    if(id<0 || p->t[id].type!='"')return 0;Token *t=&p->t[id];size_t j=0;
    for(size_t i=t->start+1;i<t->end-1;i++){unsigned char c=p->s[i];if(c=='\\'){c=p->s[++i];if(c!='/' && c!='\\' && c!='"')return 0;}
        if(c<32 || c>126 || j+1>=cap)return 0;out[j++]=(char)c;}out[j]=0;return 1;
}
static int false_value(Parser *p,int id){return id>=0 && p->t[id].end-p->t[id].start==5 && !memcmp(p->s+p->t[id].start,"false",5);}
static int version(const char *s,unsigned int a[3]){
    if(*s=='v')s++;for(int i=0;i<3;i++){if(!isdigit((unsigned char)*s))return 0;unsigned int n=0;int digits=0;
        do{if(++digits>5)return 0;n=n*10+(*s++-'0');}while(isdigit((unsigned char)*s));a[i]=n;
        if(i<2 && *s++!='.')return 0;}return !*s;
}
int s14_version_compare(const char *l,const char *r,int *c){unsigned int a[3],b[3];if(!l||!r||!c||!version(l,a)||!version(r,b))return 0;
    *c=0;for(int i=0;i<3;i++)if(a[i]!=b[i]){*c=a[i]>b[i]?1:-1;break;}return 1;}
int s14_release_parse(const char *json,size_t size,const char *current,S14Release *out){
    if(!json||!out||!current||!size||size>1048576)return 0;
    Token *tokens=calloc(8192,sizeof(*tokens));if(!tokens)return 0;Parser p={json,size,0,tokens,0};int ok=0;
    if(value(&p,0)!=0 || tokens[0].type!='[')goto done;space(&p);if(p.pos!=size)goto done;
    memset(out,0,sizeof(*out));
    for(int r=1;r<tokens[0].next;r=tokens[r].next){S14Release candidate={0};char tag[32],name[96],expected[96],digest[80],url[512];int cmp;
        if(!false_value(&p,field(&p,r,"draft")) || !text(&p,field(&p,r,"tag_name"),tag,sizeof(tag)) || tag[0]!='v' || !s14_version_compare(tag,current,&cmp))continue;
        strcpy(candidate.version,tag+1);snprintf(expected,sizeof(expected),"SAN14ModManager-%s-windows-x64.exe",candidate.version);
        int assets=field(&p,r,"assets");if(assets<0 || tokens[assets].type!='[')continue;
        for(int a=assets+1;a<tokens[assets].next;a=tokens[a].next){
            if(!text(&p,field(&p,a,"name"),name,sizeof(name)) || strcmp(name,expected) || !equal(&p,field(&p,a,"state"),"uploaded"))continue;
            if(!text(&p,field(&p,a,"digest"),digest,sizeof(digest)) || strlen(digest)!=71 || strncmp(digest,"sha256:",7))continue;
            int valid=1;for(int i=7;i<71;i++)if(!(digest[i]>='0'&&digest[i]<='9') && !(digest[i]>='a'&&digest[i]<='f'))valid=0;if(!valid)continue;
            if(!text(&p,field(&p,a,"browser_download_url"),candidate.url,sizeof(candidate.url)))continue;
            snprintf(url,sizeof(url),"https://github.com/Alexzz96/san14-mod-manager/releases/download/%s/%s",tag,expected);
            if(strcmp(url,candidate.url))continue;
            int sz=field(&p,a,"size");if(sz<0)continue;Token *t=&tokens[sz];size_t n=t->end-t->start;if(!n||n>8)continue;
            unsigned int bytes=0;for(size_t i=t->start;i<t->end;i++){if(!isdigit((unsigned char)json[i])){valid=0;break;}bytes=bytes*10+(json[i]-'0');}
            if(!valid||bytes<64||bytes>33554432)continue;
            candidate.size=bytes;strcpy(candidate.sha256,digest+7);candidate.newer=cmp>0;
            int best=1;if(ok && (!s14_version_compare(candidate.version,out->version,&best)||best<=0))continue;
            *out=candidate;ok=1;
        }
    }
done:free(tokens);return ok;
}
