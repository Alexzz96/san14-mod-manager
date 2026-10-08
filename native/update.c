#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "update.h"
#include "features.h"
#include "package.h"
static int fail(wchar_t error[192],const wchar_t *text){wcsncpy(error,text,191);error[191]=0;return 0;}
static int fetch(const wchar_t *url,HANDLE file,char **body,unsigned int limit,unsigned int *length,wchar_t error[192]){
    HINTERNET session=NULL,connect=NULL,request=NULL;int ok=0;char *data=NULL;DWORD status=0,n=sizeof(status);unsigned int total=0;
    wchar_t host[256],path[1024],extra[128];URL_COMPONENTS u={0};u.dwStructSize=sizeof(u);u.lpszHostName=host;u.dwHostNameLength=256;u.lpszUrlPath=path;u.dwUrlPathLength=1024;u.lpszExtraInfo=extra;u.dwExtraInfoLength=128;
    if(!WinHttpCrackUrl(url,0,0,&u) || u.nScheme!=INTERNET_SCHEME_HTTPS || u.dwHostNameLength>=256 || u.dwUrlPathLength>=1024)return fail(error,L"更新地址无效。");
    if(u.dwExtraInfoLength>=128 || u.dwUrlPathLength+u.dwExtraInfoLength>=1024)return fail(error,L"更新地址过长。");
    host[u.dwHostNameLength]=0;path[u.dwUrlPathLength]=0;extra[u.dwExtraInfoLength]=0;wcscat(path,extra);
    if(wcscmp(host,L"api.github.com") && wcscmp(host,L"github.com"))return fail(error,L"更新地址不属于项目发布源。");
    session=WinHttpOpen(L"SAN14ModManager/" S14_MANAGER_VERSION,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)goto done;
    if(!WinHttpSetTimeouts(session,10000,10000,15000,15000))goto done;
    connect=WinHttpConnect(session,host,u.nPort,0);if(!connect)goto done;
    request=WinHttpOpenRequest(connect,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!request)goto done;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    if(!WinHttpSetOption(request,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect)))goto done;
    if(!WinHttpSendRequest(request,L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n",-1L,WINHTTP_NO_REQUEST_DATA,0,0,0) || !WinHttpReceiveResponse(request,NULL))goto done;
    if(!WinHttpQueryHeaders(request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&n,WINHTTP_NO_HEADER_INDEX) || status!=200){
        swprintf(error,192,L"GitHub 返回 HTTP %lu。请稍后重试或打开发布页。",(unsigned long)status);goto done;
    }
    if(body){data=malloc((size_t)limit+1);if(!data){fail(error,L"更新检查内存不足。");goto done;}}
    ULONGLONG start=GetTickCount64();char buffer[32768];
    for(;;){DWORD got=0,written=0;if(GetTickCount64()-start>120000 || !WinHttpReadData(request,buffer,sizeof(buffer),&got))goto done;
        if(!got)break;if(got>limit-total){fail(error,L"下载内容超过允许大小，已停止。");goto done;}
        if(body)memcpy(data+total,buffer,got);else if(!WriteFile(file,buffer,got,&written,NULL)||written!=got)goto done;total+=got;
    }
    if(body){data[total]=0;*body=data;data=NULL;}*length=total;ok=1;
done:
    free(data);if(request)WinHttpCloseHandle(request);if(connect)WinHttpCloseHandle(connect);if(session)WinHttpCloseHandle(session);
    if(!ok && !error[0])fail(error,L"连接 GitHub 或写入下载失败。请检查网络与权限，可稍后重试。");return ok;
}
int s14_update_check(S14Release *release,wchar_t error[192]){
    char *body=NULL;unsigned int size=0;error[0]=0;memset(release,0,sizeof(*release));
    // Include this project's published prereleases; /latest intentionally omits them.
    if(!fetch(L"https://api.github.com/repos/Alexzz96/san14-mod-manager/releases?per_page=10",INVALID_HANDLE_VALUE,&body,1048576,&size,error))return 0;
    char current[32];WideCharToMultiByte(CP_UTF8,0,S14_MANAGER_VERSION,-1,current,sizeof(current),NULL,NULL);
    int ok=s14_release_parse(body,size,current,release);free(body);
    return ok?1:fail(error,L"未找到可校验的新版安装器。请打开 GitHub 发布页查看。");
}
int s14_update_download(const S14Release *release,wchar_t path[MAX_PATH],wchar_t error[192]){
    error[0]=0;path[0]=0;if(!release || !release->size || release->size>33554432 || strlen(release->sha256)!=64)return fail(error,L"缺少已校验的发布信息，请重新检查更新。");
    wchar_t temp[MAX_PATH],unique[MAX_PATH],url[512];DWORD n=GetTempPathW(MAX_PATH,temp);
    if(!n || n>=MAX_PATH || !GetTempFileNameW(temp,L"S14",0,unique))return fail(error,L"无法创建临时下载文件。");
    // Create a unique .exe alongside our own temporary marker; never replace an existing file.
    int fits=wcslen(unique)+4<MAX_PATH;if(fits)swprintf(path,MAX_PATH,L"%ls.exe",unique);DeleteFileW(unique);
    if(!fits)return fail(error,L"临时下载路径过长。");
    HANDLE file=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE){path[0]=0;return fail(error,L"无法创建更新安装器。");}
    unsigned int size=0;int ok=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,release->url,-1,url,512)>0 && fetch(url,file,NULL,release->size,&size,error);
    if(ok)ok=FlushFileBuffers(file)!=0;CloseHandle(file);
    char digest[65];if(ok)ok=size==release->size && s14_hash_file(path,digest) && !strcmp(digest,release->sha256);
    if(!ok){DeleteFileW(path);path[0]=0;if(!error[0])fail(error,L"下载文件大小或 SHA-256 不一致，已删除；请重新检查更新。");}
    return ok;
}
