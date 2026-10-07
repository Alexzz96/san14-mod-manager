#ifndef S14_MANAGER_UI_H
#define S14_MANAGER_UI_H
#include <windows.h>
#include "features.h"
typedef struct S14ManagerUI S14ManagerUI;
typedef void (*S14ManagerAction)(S14ManagerUI*,int,void*);
enum { S14_ACTION_CONFIG=1,S14_ACTION_CHOOSE,S14_ACTION_INSTALL,S14_ACTION_REMOVE,S14_ACTION_LOGS,S14_ACTION_CLEAN };
struct S14ManagerUI {
    HWND window,owner,directory_edit;
    HINSTANCE instance;
    HFONT title_font,body_font,small_font;
    HBRUSH edit_brush;
    wchar_t root[MAX_PATH],ini[MAX_PATH],status[192],notice[192];
    unsigned int requested,effective;
    unsigned int detected;
    int in_game,attached,fault,installed,game_found,running,tab,focus,pressed,scale,scroll,notice_error;
    S14ManagerAction action; void *context;
};
int s14_manager_create(S14ManagerUI *ui,HINSTANCE instance,HWND owner,int in_game);
void s14_manager_toggle(S14ManagerUI *ui);
void s14_manager_refresh(S14ManagerUI *ui);
void s14_manager_paint(S14ManagerUI *ui,HDC dc,int width,int height);
void s14_manager_destroy(S14ManagerUI *ui);
int s14_manager_hit(S14ManagerUI *ui,POINT point);
int s14_manager_activate(S14ManagerUI *ui,int action);
#endif
