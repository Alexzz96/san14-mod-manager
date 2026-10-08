#include "update.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static const char *asset="[{\"tag_name\":\"v0.8.0\",\"draft\":false,\"prerelease\":true,\"body\":\"fake \\\"tag_name\\\":v9.9.9\",\"assets\":[{\"name\":\"SAN14ModManager-0.8.0-windows-x64.exe\",\"state\":\"uploaded\",\"size\":900000,\"digest\":\"sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"browser_download_url\":\"https://github.com/Alexzz96/san14-mod-manager/releases/download/v0.8.0/SAN14ModManager-0.8.0-windows-x64.exe\"}]}]";
static void reject(const char *from,const char *to){char b[4096];strcpy(b,asset);char *p=strstr(b,from);assert(p);size_t n=strlen(from),m=strlen(to);memmove(p+m,p+n,strlen(p+n)+1);memcpy(p,to,m);S14Release r;assert(!s14_release_parse(b,strlen(b),"0.7.2",&r));}
int main(void){int cmp;S14Release r;
    assert(s14_version_compare("v0.10.0","0.9.99",&cmp)&&cmp>0);
    assert(s14_version_compare("0.7.2","0.7.2",&cmp)&&!cmp);
    assert(!s14_version_compare("0.7.2-rc1","0.7.2",&cmp));
    assert(!s14_version_compare("0.7","0.7.2",&cmp));
    assert(s14_release_parse(asset,strlen(asset),"0.7.2",&r)&&r.newer&&r.size==900000&&!strcmp(r.version,"0.8.0"));
    assert(s14_release_parse(asset,strlen(asset),"0.8.0",&r)&&!r.newer);
    assert(s14_release_parse(asset,strlen(asset),"1.0.0",&r)&&!r.newer);
    reject("\"draft\":false","\"draft\":true");reject("\"uploaded\"","\"new\"");reject("sha256:","sha512:");
    reject("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","short");reject("900000","33554433");
    reject("900000","-1");reject("900000","1.5");reject("https://github.com/","http://github.com/");
    reject("https://github.com/Alexzz96/","https://github.com/other/");reject("windows-x64.exe\",\"state","source.zip\",\"state");
    reject("\"draft\":false","\"draft\":false,\"draft\":false");
    assert(!s14_release_parse("[]",2,"0.7.2",&r));assert(!s14_release_parse(asset,strlen(asset)-1,"0.7.2",&r));
    char trailing[4096];snprintf(trailing,sizeof(trailing),"%sX",asset);assert(!s14_release_parse(trailing,strlen(trailing),"0.7.2",&r));
    // API ordering is not a version ordering: select the highest usable published release.
    char many[4096];snprintf(many,sizeof(many),"[%.*s,%.*s]",(int)strlen(asset)-2,asset+1,(int)strlen(asset)-2,asset+1);
    char *v=strstr(many,"v0.8.0");v[5]='1'; // Invalid asset/tag agreement must not displace the valid second entry.
    assert(s14_release_parse(many,strlen(many),"0.7.2",&r)&&!strcmp(r.version,"0.8.0"));
    char higher[2048];strcpy(higher,asset);for(char *p=higher;(p=strstr(p,"0.8.0"));p+=5)p[2]='9';
    snprintf(many,sizeof(many),"[%.*s,%.*s]",(int)strlen(asset)-2,asset+1,(int)strlen(higher)-2,higher+1);
    assert(s14_release_parse(many,strlen(many),"0.7.2",&r)&&!strcmp(r.version,"0.9.0"));
    snprintf(many,sizeof(many),"[%.*s,%.*s]",(int)strlen(higher)-2,higher+1,(int)strlen(asset)-2,asset+1);
    assert(s14_release_parse(many,strlen(many),"0.7.2",&r)&&!strcmp(r.version,"0.9.0"));
    puts("{\"update_model\":\"passed\",\"version_order\":true,\"prerelease_supported\":true,\"release_origin_validated\":true,\"digest_required\":true,\"malformed_metadata_rejected\":true}");return 0;
}
