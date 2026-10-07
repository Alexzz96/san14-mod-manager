#ifndef S14_PACKAGE_H
#define S14_PACKAGE_H
#include <windows.h>
int s14_join(wchar_t *output,const wchar_t *root,const wchar_t *name);
int s14_hash_file(const wchar_t *path,char output[65]);
int s14_game_available(const wchar_t *root);
enum { S14_FAULT_ENTRY=1, S14_FAULT_HOOK, S14_FAULT_MAP };
const wchar_t *s14_fault_message(int fault);
int s14_game_running(const wchar_t *root);
int s14_owned_install(const wchar_t *root);
int s14_package_install(const wchar_t *root,const wchar_t *manager_source,wchar_t error[192]);
int s14_package_remove(const wchar_t *root,wchar_t error[192]);
void s14_publish_runtime(const wchar_t *root,unsigned int desired,unsigned int effective,int ready,int fault);
int s14_read_runtime(const wchar_t *root,unsigned int *effective,int *fault);
#endif
