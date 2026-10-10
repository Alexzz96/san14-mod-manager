#define WIN32_LEAN_AND_MEAN
#include "troop_ui.h"
#include <stdio.h>
#include <string.h>
static const wchar_t klass[]=L"SAN14ModManager.PluginTroops.Hint.v2";
/* The button itself belongs to the game. This window is only a passive hint:
   all pointer input passes through to the native UI, including clicking. */
static void paint(S14TroopUI *ui,HDC dc){
    int s=ui->scale;HBRUSH paper=CreateSolidBrush(RGB(247,240,221));RECT all={0,0,ui->width,ui->height};FillRect(dc,&all,paper);DeleteObject(paper);
    HPEN pen=CreatePen(PS_SOLID,MulDiv(2,s,96),RGB(161,121,65));HGDIOBJ op=SelectObject(dc,pen),ob=SelectObject(dc,GetStockObject(NULL_BRUSH));Rectangle(dc,0,0,ui->width,ui->height);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(pen);
    HGDIOBJ font=SelectObject(dc,ui->font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(73,43,22));
    const S14TroopDefinition *d=s14_troop_registry_find(s14_troop_builtin_registry(),"san14.xianzhen");
    if(!d){SelectObject(dc,font);return;}
    wchar_t name[64],description[384],title[96],price[128],limit[96],attack[128],siege[128],move[128],confusion[128],surround[128];
    MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,d->name,-1,name,64);MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,d->description,-1,description,384);
    swprintf(title,96,L"%ls · 高顺专属%ls",name,ui->frame.selected?L" · 已选择":L"");
    swprintf(limit,96,L"最多 %d 人 · 成功建队后扣费。",ui->frame.max_soldiers);
    swprintf(price,128,L"额外军费 %d · 原生军费 %d",ui->frame.extra_cost,ui->frame.native_cost);
    int reduction=0;for(int i=0;i<d->effect_count;i++)if(d->effects[i].kind==S14_TROOP_DAMAGE_REDUCTION)reduction=d->effects[i].value_bp;
    swprintf(attack,128,L"攻军 %+g%%     防御 %+g%%",d->bonus_bp[0]/100.,d->bonus_bp[4]/100.);
    swprintf(siege,128,L"攻城 %+g%%     破城 %+g%%",d->bonus_bp[1]/100.,d->bonus_bp[2]/100.);
    swprintf(move,128,L"机动 %+g%%     直接交战减伤 %g%%",d->bonus_bp[3]/100.,reduction/100.);
    unsigned caps=s14_troop_capabilities();
    swprintf(confusion,128,L"军纪严整 · 混乱免疫%ls",caps&S14_TROOP_CAP_STATUS_IMMUNITY?L"（实机待验）":L"（未接入）");
    swprintf(surround,128,L"阵列不乱 · 包围无效%ls",caps&S14_TROOP_CAP_SURROUND_IMMUNITY?L"（实机待验）":L"（未接入）");
    const wchar_t *lines[]={title,attack,siege,move,price,limit,confusion,surround};
    for(int i=0;i<8;i++){RECT line={MulDiv(12,s,96),MulDiv(10+i*24,s,96),ui->width-MulDiv(12,s,96),MulDiv(34+i*24,s,96)};DrawTextW(dc,lines[i],-1,&line,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);}
    RECT quote={MulDiv(12,s,96),MulDiv(210,s,96),ui->width-MulDiv(12,s,96),MulDiv(278,s,96)};DrawTextW(dc,description,-1,&quote,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    RECT footer={MulDiv(12,s,96),MulDiv(292,s,96),ui->width-MulDiv(12,s,96),ui->height-MulDiv(10,s,96)};DrawTextW(dc,L"传奇强度设计 · 选择普通兵种可取消。",-1,&footer,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    SelectObject(dc,font);
}
static LRESULT CALLBACK procedure(HWND window,UINT msg,WPARAM w,LPARAM l){
    S14TroopUI *ui=(void*)GetWindowLongPtrW(window,GWLP_USERDATA);if(msg==WM_NCCREATE){ui=((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui);}
    if(msg==WM_MOUSEACTIVATE)return MA_NOACTIVATE;if(msg==WM_NCHITTEST)return HTTRANSPARENT;if(msg==WM_ERASEBKGND)return 1;
    if(ui && msg==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);paint(ui,dc);EndPaint(window,&ps);return 0;}
    return DefWindowProcW(window,msg,w,l);
}
static int hint_rect(const S14TroopFrame *f,int scale,const RECT *client,POINT cursor,RECT *rect){
    RECT button={MulDiv(f->x,scale,96),MulDiv(f->y,scale,96),MulDiv(f->x+f->w,scale,96),MulDiv(f->y+f->h,scale,96)};
    if(!PtInRect(&button,cursor))return 0;
    int width=MulDiv(460,scale,96),height=MulDiv(328,scale,96),margin=MulDiv(10,scale,96);
    if(width>client->right-2*margin || height>client->bottom-2*margin)return 0;
    int x=button.left,y=button.top-height-margin;if(y<margin)y=button.bottom+margin;
    if(x+width>client->right-margin)x=client->right-margin-width;if(x<margin)x=margin;
    if(y+height>client->bottom-margin)y=client->bottom-margin-height;if(y<margin)y=margin;
    *rect=(RECT){x,y,x+width,y+height};return 1;
}
void s14_troop_ui_tick(S14TroopUI *ui,HINSTANCE instance,HWND owner,int blocked){
    S14TroopFrame f;s14_troop_frame(&f);RECT client,rect;POINT origin={0},cursor;HWND front=GetForegroundWindow();
    if(blocked || !f.visible || !owner || front!=owner || !IsWindowVisible(owner) || IsIconic(owner) || !GetClientRect(owner,&client) || !ClientToScreen(owner,&origin) || !GetCursorPos(&cursor) || !ScreenToClient(owner,&cursor)){if(ui->window)ShowWindow(ui->window,SW_HIDE);return;}
    int scale=MulDiv(client.bottom,96,1080);
    if(scale<40 || scale>384 || !hint_rect(&f,scale,&client,cursor,&rect)){if(ui->window)ShowWindow(ui->window,SW_HIDE);return;}
    int width=rect.right-rect.left,height=rect.bottom-rect.top;
    if(!ui->window){WNDCLASSEXW c={.cbSize=sizeof(c),.hInstance=instance,.lpfnWndProc=procedure,.lpszClassName=klass,.hCursor=LoadCursorW(NULL,IDC_ARROW)};if(!RegisterClassExW(&c) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
        ui->window=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TRANSPARENT,klass,L"陷阵营 · 兵种说明",WS_POPUP,0,0,1,1,owner,NULL,instance,ui);}
    if(!ui->window)return;if(ui->scale!=scale || !ui->font){if(ui->font)DeleteObject(ui->font);ui->font=CreateFontW(-MulDiv(16,scale,96),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");}
    int changed=memcmp(&ui->frame,&f,sizeof(f)) || ui->scale!=scale || ui->width!=width || ui->height!=height;ui->frame=f;ui->scale=scale;ui->width=width;ui->height=height;ui->owner=owner;
    SetWindowPos(ui->window,HWND_TOPMOST,origin.x+rect.left,origin.y+rect.top,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);if(changed)InvalidateRect(ui->window,NULL,FALSE);
}
void s14_troop_ui_destroy(S14TroopUI *ui){if(ui->window)DestroyWindow(ui->window);if(ui->font)DeleteObject(ui->font);memset(ui,0,sizeof(*ui));}
