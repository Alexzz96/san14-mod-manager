#define WIN32_LEAN_AND_MEAN
#include <windowsx.h>
#include "army_ui.h"
#include "theme.h"
#include "career_affix.h"
#include <stdio.h>
#include <string.h>
static const wchar_t bar_class[]=L"SAN14ModManager.ArmyAttributeBar.v1",popup_class[]=L"SAN14ModManager.ArmyAttributeDetail.v1";
static int px(S14ArmyUI *ui,int n){return MulDiv(n,ui->scale,96);}
static int memory(void *unused,uintptr_t at,void *out,size_t n){(void)unused;SIZE_T got;return ReadProcessMemory(GetCurrentProcess(),(void*)at,out,n,&got) && got==n;}
static void fill(HDC dc,RECT r,COLORREF c){HBRUSH b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);}
static void text(S14ArmyUI *ui,HDC dc,const wchar_t *s,RECT r,HFONT font,COLORREF color,UINT flags){(void)ui;HGDIOBJ old=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);DrawTextW(dc,s,-1,&r,flags|DT_NOPREFIX);SelectObject(dc,old);}
static RECT box(S14ArmyUI *ui,int x,int y,int w,int h){return (RECT){px(ui,x),px(ui,y),px(ui,x+w),px(ui,y+h)};}
void s14_army_ui_paint(S14ArmyUI *ui,HDC dc,int detail){
    RECT all;GetClientRect(detail?ui->popup:ui->bar,&all);if(all.right<=0)all=(RECT){0,0,ui->width,ui->height};fill(dc,all,S14_PAPER);
    HBRUSH edge=CreateSolidBrush(S14_BORDER);FrameRect(dc,&all,edge);DeleteObject(edge);
    S14ArmyFrame *f=&ui->frame;S14ArmyTrace *t=&f->trace;wchar_t line[256];
    swprintf(line,256,L"%ls%ls · 部队数值解析%ls",f->name,s14_affix_active(f->world,S14_ELITE_OFFICER)?L"":L"队",ui->ready?L"":L" · 采集未接入");
    text(ui,dc,line,detail?box(ui,24,15,1100,34):(RECT){px(ui,14),px(ui,4),all.right-px(ui,240),px(ui,27)},detail?ui->value:ui->small,S14_ACCENT,DT_SINGLELINE|DT_VCENTER);
    if(!detail){
        text(ui,dc,f->connected?L"白字 + 蓝字 · 点击查看来源":L"等待原生计算 · 点击查看说明",(RECT){all.right-px(ui,252),px(ui,5),all.right-px(ui,12),px(ui,27)},ui->small,S14_MUTED,DT_SINGLELINE|DT_VCENTER);
        int cell=(all.right-px(ui,28))/5;const int order[]={0,4,1,2,3};
        for(int col=0;col<5;col++){int i=order[col],x=px(ui,14)+cell*col;
            text(ui,dc,s14_army_attribute_name(i),(RECT){x,px(ui,29),x+cell-px(ui,8),px(ui,47)},ui->small,S14_MUTED,DT_SINGLELINE);
            if(f->connected)swprintf(line,256,L"%d %+d",t->baseline[i],t->actual[i]-t->baseline[i]);else wcscpy(line,L"等待采集");
            text(ui,dc,line,(RECT){x,px(ui,47),x+cell-px(ui,8),px(ui,74)},ui->body,S14_INK,DT_SINGLELINE|DT_VCENTER);
        }return;
    }
    text(ui,dc,L"×",box(ui,1140,12,36,34),ui->value,S14_MUTED,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    swprintf(line,256,L"%ls · %ls · %ls · 士兵 %d · 伤兵 %d · 士气 %d",f->location,f->sea_preview?L"水上预览":L"陆上预览",f->formation[0]?f->formation:L"阵形未识别",f->troops,f->wounded,f->morale);
    text(ui,dc,line,box(ui,24,57,1152,26),ui->body,S14_MUTED,DT_SINGLELINE|DT_VCENTER);
    swprintf(line,256,L"主将基础能力：统率 %d · 武力 %d · 智力 %d · 政治 %d · 魅力 %d",f->abilities[0],f->abilities[1],f->abilities[2],f->abilities[3],f->abilities[4]);
    text(ui,dc,line,box(ui,24,84,1152,24),ui->small,S14_MUTED,DT_SINGLELINE|DT_VCENTER);
    int saved=SaveDC(dc);IntersectClipRect(dc,0,px(ui,112),all.right,all.bottom);SetViewportOrgEx(dc,0,-px(ui,ui->scroll),NULL);
    const wchar_t *headers[]={L"属性",L"白字基础",L"蓝字增减",L"最终数值",L"政策修正",L"个性/状态层",L"临时 buff"};
    const int columns[]={24,120,260,400,550,746,980},widths[]={90,134,134,144,190,228,196};
    const COLORREF blue=RGB(48,109,157);const int order[]={0,4,1,2,3};
    fill(dc,box(ui,24,118,1152,34),S14_CARD);
    for(int j=0;j<7;j++)text(ui,dc,headers[j],box(ui,columns[j]+8,120,widths[j]-8,28),ui->small,S14_MUTED,DT_SINGLELINE|DT_VCENTER);
    for(int row=0;row<5;row++){
        int i=order[row],y=154+row*40,pct=0,bpct=0;fill(dc,box(ui,24,y,1152,38),row%2?S14_PAPER:S14_CARD);
        for(int j=0;j<7;j++){
            if(j==0)wcscpy(line,s14_army_attribute_name(i));else if(!f->connected)wcscpy(line,L"未采集");
            else if(j==1)swprintf(line,256,L"%d",t->baseline[i]);else if(j==2)swprintf(line,256,L"%+d",t->actual[i]-t->baseline[i]);
            else if(j==3)swprintf(line,256,L"%d",t->actual[i]);
            else if(j==4){if(t->policy_seen[0][i])swprintf(line,256,L"%+d%%",t->policy[0][i]-100);else wcscpy(line,L"尚未采集");}
            else if(j==5){if(s14_army_phase_percent(t,0,i,&bpct) && s14_army_percent(t,i,&pct))swprintf(line,256,L"%+d%% → %+d%%",bpct,pct);else wcscpy(line,L"尚未分解");}
            else swprintf(line,256,L"%+d%%",(int)t->temporary[i]*t->steps[i]);
            text(ui,dc,line,box(ui,columns[j]+8,y,widths[j]-8,38),ui->body,j==2?blue:j==3?S14_ACCENT:S14_INK,DT_SINGLELINE|DT_VCENTER);
        }
    }
    if(f->connected && t->buff_seen[0] && t->buff_seen[4])
        swprintf(line,256,L"百战精锐：攻军原生 %.3f × %.2f → %.3f；防御原生 %.3f × %.2f → %.3f",(double)t->native_attribute[0],1.0+(double)t->buff_percent[0]/100.0,(double)t->buffed_attribute[0],(double)t->native_attribute[4],1.0+(double)t->buff_percent[4]/100.0,(double)t->buffed_attribute[4]);
    else wcscpy(line,L"百战精锐：等待原生属性计算采集。");
    text(ui,dc,line,box(ui,24,364,1152,28),ui->small,S14_ACCENT,DT_SINGLELINE);
    text(ui,dc,L"插件增幅在原生实际浮点结果上独立相乘，再由游戏取整；反复打开不会累乘。个性/状态列分别显示白字与实际分支。",box(ui,24,395,1152,32),ui->small,S14_MUTED,DT_WORDBREAK);
    s14_affix_status(f->world,line,256);text(ui,dc,line,box(ui,24,426,1152,24),ui->small,S14_ACCENT,DT_SINGLELINE);
    text(ui,dc,L"白字 · 基础、统率与政策",box(ui,24,452,1152,30),ui->value,S14_INK,DT_SINGLELINE);
    float soldier=0,commander=0,foundation=0;
    if(f->connected && s14_army_foundation(t,f->troops,&soldier,&commander,&foundation))
        swprintf(line,256,L"有效统率 %d（基础 %d，差值 %+d） · 兵力/统率项 %.3f + 统率线性项 %.3f + 常数 %d = %.3f",t->leadership[0],f->abilities[0],t->leadership[0]-f->abilities[0],(double)soldier,(double)commander,t->foundation_flat,(double)foundation);
    else wcscpy(line,L"有效统率与兵力基础项：等待原生计算。");
    text(ui,dc,line,box(ui,24,492,1152,28),ui->small,S14_INK,DT_SINGLELINE);
    if(f->connected && t->foundation_seen)
        swprintf(line,256,L"兵力/统率项 = √[4 × min(兵力, trunc((floor(4×统率/3))^%.1f))] / %d；线性项 = 统率 × %d",t->leadership_exponent,t->sqrt_divisor,t->leadership_scale);
    else wcscpy(line,L"公式参数尚未采集。");
    text(ui,dc,line,box(ui,24,522,1152,28),ui->small,S14_MUTED,DT_SINGLELINE);
    if(f->connected && t->factor_seen[0])swprintf(line,256,L"再经士气、原生全局与地形等基础处理，白字公共计算值 = %.3f；下面按各属性系数换算。",(double)t->factor[0]);else wcscpy(line,L"白字公共计算值尚未采集。");
    text(ui,dc,line,box(ui,24,554,1152,28),ui->small,S14_INK,DT_SINGLELINE);
    for(int row=0;row<5;row++){
        int i=order[row],y=592+row*34;float predicted=0;fill(dc,box(ui,24,y,1152,32),row%2?S14_PAPER:S14_CARD);
        if(!f->connected || !t->formation_seen[i])swprintf(line,256,L"%ls · 阵形系数尚未读取",s14_army_attribute_name(i));
        else if(t->policy_seen[0][i])swprintf(line,256,L"%ls · 阵形 %d · %ls %+d%% · 政策强度 %d · 全局系数 %ls",s14_army_attribute_name(i),t->formation[i],f->policy_names[i][0]?f->policy_names[i]:L"兵种政策",t->policy[0][i]-100,t->policy_strength[0][i],(i==2 || i==3)?L"独立公式":L"见换算");
        else swprintf(line,256,L"%ls · 阵形 %d · 政策修正尚未采集",s14_army_attribute_name(i),t->formation[i]);
        text(ui,dc,line,box(ui,32,y,800,32),ui->small,S14_INK,DT_SINGLELINE|DT_VCENTER);
        if(s14_army_white_formula(t,i,&predicted))swprintf(line,256,L"× %.3f → %.3f · %ls",(double)t->global_percent[i]/100.0,(double)predicted,(int)predicted==t->baseline[i]?L"与白字一致":L"存在未分解差额");
        else wcscpy(line,i==2?L"另含统率、兵力公式":i==3?L"另含地形、移动公式":L"等待公式校验");
        text(ui,dc,line,box(ui,840,y,328,32),ui->small,S14_MUTED,DT_SINGLELINE|DT_VCENTER);
    }
    text(ui,dc,L"攻军/攻城/防御：阵形 × 政策倍率 × 公共计算值 × 全局系数 × 白字效果倍率，再截断；防御另加原生常数 1。",box(ui,24,770,1152,30),ui->small,S14_MUTED,DT_SINGLELINE);
    text(ui,dc,L"蓝字 · 府、连携、个性与临时 buff",box(ui,24,822,1152,30),ui->value,blue,DT_SINGLELINE);
    if(f->connected && t->area_seen)swprintf(line,256,L"府/领地计算返回倍率 × %.3f · 原生累计单位 %d（此入口也可能包含难度修正）",(double)t->area_factor,t->area_units);else wcscpy(line,L"府/领地倍率：尚未采集。");
    text(ui,dc,line,box(ui,24,864,1152,28),ui->body,S14_INK,DT_SINGLELINE);
    text(ui,dc,f->area_sources[0]?f->area_sources:L"所属府与相连府明细尚未取得。",box(ui,24,896,1152,52),ui->small,S14_MUTED,DT_WORDBREAK);
    if(f->connected && t->link_seen && t->link_step_seen)swprintf(line,256,L"连携：原生认定 %d 支 · 每支 %+d%% · 倍率 × %.3f",t->link_count,t->link_step,1.0+(double)t->link_count*t->link_step/100.0);else wcscpy(line,L"连携倍率：尚未采集。");
    text(ui,dc,line,box(ui,24,954,1152,28),ui->body,S14_INK,DT_SINGLELINE);
    if(f->connected && t->factor_seen[0] && t->factor_seen[1] && t->factor[0]>0){
        double ratio=(double)t->factor[1]/t->factor[0],known=t->area_seen?t->area_factor:1.0;
        if(t->link_seen && t->link_step_seen)known*=1.0+(double)t->link_count*t->link_step/100.0;
        if(known>0)swprintf(line,256,L"公共计算值 %.3f → %.3f · 倍率 × %.3f；扣除已解析府与连携后，其余公共状态倍率 × %.3f",(double)t->factor[0],(double)t->factor[1],ratio,ratio/known);else wcscpy(line,L"公共状态存在零倍率，暂不计算余项。");
    }else wcscpy(line,L"公共状态增幅：等待原生计算。");
    text(ui,dc,line,box(ui,24,988,1152,28),ui->small,S14_INK,DT_SINGLELINE);
    text(ui,dc,L"府、连携的公共倍率作用于攻军/攻城/防御；破城与机动走各自公式，不能套用同一个倍率。",box(ui,24,1022,1152,28),ui->small,S14_MUTED,DT_SINGLELINE);
    text(ui,dc,f->sources_verified?L"生效的个性/传播效果 · 已与原生五项汇总核对":L"个性逐项来源 · 尚未与原生汇总闭合",box(ui,24,1064,1152,28),ui->body,S14_ACCENT,DT_SINGLELINE);
    text(ui,dc,f->active_sources[0]?f->active_sources:L"等待生效列表采集。",box(ui,24,1102,1152,220),ui->small,S14_INK,DT_WORDBREAK);
    for(int row=0;row<5;row++){
        int i=order[row],y=1338+row*26;
        if(t->observed[i] && t->extra_observed[i])swprintf(line,256,L"%ls：专项单位 %d + 全能力单位 %d + 附加单位 %d + 条件单位 %d；按 ±%d 单位夹限，每单位 %d%%。",s14_army_attribute_name(i),t->aggregate[i],t->all[i],t->extra[i],t->conditional[i],t->limits[i],t->steps[i]);
        else swprintf(line,256,L"%ls：汇总来源尚未采集。",s14_army_attribute_name(i));
        text(ui,dc,line,box(ui,24,y,1152,26),ui->small,S14_MUTED,DT_SINGLELINE);
    }
    text(ui,dc,L"临时 buff 已按部队字段逐项列出；具体战法施放者仍待对应。格挡、免疫等效果不折算为攻防百分比。",box(ui,24,1484,1152,48),ui->small,S14_MUTED,DT_WORDBREAK);
    text(ui,dc,L"主将持有个性 · 规则供核对，持有不等于当前触发",box(ui,24,1548,1152,28),ui->body,S14_ACCENT,DT_SINGLELINE);
    text(ui,dc,f->personalities[0]?f->personalities:L"个性说明尚未读取",box(ui,24,1586,1152,224),ui->small,S14_INK,DT_WORDBREAK);
    text(ui,dc,L"原生基础与插件增幅分别记录 · 点击右上角或按 Esc 关闭",box(ui,24,1832,1152,26),ui->small,S14_MUTED,DT_SINGLELINE);
    RestoreDC(dc,saved);
}
static void popup_hide(S14ArmyUI *ui){if(ui->popup)ShowWindow(ui->popup,SW_HIDE);ui->popup_shown=0;}
static void hide(S14ArmyUI *ui){if(ui->bar)ShowWindow(ui->bar,SW_HIDE);ui->shown=0;popup_hide(ui);}
static LRESULT CALLBACK procedure(HWND window,UINT msg,WPARAM w,LPARAM l);
static int create(S14ArmyUI *ui,int detail){
    WNDCLASSEXW c={.cbSize=sizeof(c),.lpfnWndProc=procedure,.hInstance=ui->instance,.hCursor=LoadCursorW(NULL,IDC_ARROW),.lpszClassName=detail?popup_class:bar_class};
    if(!RegisterClassExW(&c) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    HWND h=CreateWindowExW(WS_EX_TOOLWINDOW|(detail?0:WS_EX_NOACTIVATE),c.lpszClassName,detail?L"曹仁队 · 部队数值解析":L"部队数值解析",WS_POPUP|(detail?WS_VSCROLL:0),0,0,1,1,ui->owner,NULL,ui->instance,ui);
    if(detail)ui->popup=h;else ui->bar=h;return h!=NULL;
}
static int fonts(S14ArmyUI *ui,int scale){
    if(ui->scale==scale && ui->value)return 1;
    if(ui->small)DeleteObject(ui->small);if(ui->body)DeleteObject(ui->body);if(ui->value)DeleteObject(ui->value);ui->scale=scale;
    ui->small=CreateFontW(-px(ui,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->body=CreateFontW(-px(ui,16),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");
    ui->value=CreateFontW(-px(ui,23),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Microsoft YaHei UI");return ui->small && ui->body && ui->value;
}
static void popup_show(S14ArmyUI *ui){
    if(!ui->popup && !create(ui,1))return;RECT region;GetWindowRect(ui->owner,&region);int w=px(ui,1200),h=px(ui,920);
    ui->scroll=0;SCROLLINFO si={.cbSize=sizeof(si),.fMask=SIF_RANGE|SIF_PAGE|SIF_POS,.nMin=0,.nMax=1867,.nPage=920,.nPos=0};SetScrollInfo(ui->popup,SB_VERT,&si,TRUE);
    ui->popup_shown=SetWindowPos(ui->popup,HWND_TOP,(region.left+region.right-w)/2,(region.top+region.bottom-h)/2,w,h,SWP_SHOWWINDOW)!=0;
    if(ui->popup_shown)SetForegroundWindow(ui->popup);InvalidateRect(ui->popup,NULL,FALSE);
}
static LRESULT CALLBACK procedure(HWND window,UINT msg,WPARAM w,LPARAM l){
    S14ArmyUI *ui=(void*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(msg==WM_NCCREATE){ui=((CREATESTRUCTW*)l)->lpCreateParams;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)ui);}
    if(!ui)return DefWindowProcW(window,msg,w,l);int detail=window==ui->popup;
    if(msg==WM_MOUSEACTIVATE && !detail)return MA_NOACTIVATE;
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_PAINT){PAINTSTRUCT p;HDC dc=BeginPaint(window,&p);RECT r;GetClientRect(window,&r);HDC back=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,r.right,r.bottom);HGDIOBJ old=SelectObject(back,bitmap);s14_army_ui_paint(ui,back,detail);BitBlt(dc,0,0,r.right,r.bottom,back,0,0,SRCCOPY);SelectObject(back,old);DeleteObject(bitmap);DeleteDC(back);EndPaint(window,&p);return 0;}
    if(detail && (msg==WM_MOUSEWHEEL || msg==WM_VSCROLL || (msg==WM_KEYDOWN && (w==VK_NEXT || w==VK_PRIOR || w==VK_HOME || w==VK_END)))){
        int next=ui->scroll;
        if(msg==WM_MOUSEWHEEL)next-=GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*90;
        else if(msg==WM_KEYDOWN)next=w==VK_HOME?0:w==VK_END?948:next+(w==VK_NEXT?640:-640);
        else {SCROLLINFO si={.cbSize=sizeof(si),.fMask=SIF_TRACKPOS};GetScrollInfo(window,SB_VERT,&si);int code=LOWORD(w);next=code==SB_THUMBTRACK?si.nTrackPos:code==SB_TOP?0:code==SB_BOTTOM?948:next+(code==SB_LINEDOWN?40:code==SB_LINEUP?-40:code==SB_PAGEDOWN?640:code==SB_PAGEUP?-640:0);}
        if(next<0)next=0;if(next>948)next=948;ui->scroll=next;SetScrollPos(window,SB_VERT,next,TRUE);InvalidateRect(window,NULL,FALSE);return 0;
    }
    if(msg==WM_LBUTTONUP){if(!detail)popup_show(ui);else if(GET_X_LPARAM(l)>=px(ui,1136) && GET_Y_LPARAM(l)<px(ui,52)){popup_hide(ui);if(ui->owner)SetForegroundWindow(ui->owner);}return 0;}
    if(msg==WM_CLOSE || (msg==WM_KEYDOWN && w==VK_ESCAPE)){popup_hide(ui);if(ui->owner)SetForegroundWindow(ui->owner);return 0;}
    return DefWindowProcW(window,msg,w,l);
}
void s14_army_ui_tick(S14ArmyUI *ui,HINSTANCE instance,HWND owner,uintptr_t base,int enabled,ULONGLONG now){
    ui->enabled=enabled;ui->owner=owner;ui->instance=instance;ui->base=base;ui->ready=s14_army_observer_ready();s14_army_observer_configure(enabled);
    if(!enabled || !owner || !IsWindowVisible(owner) || IsIconic(owner)){hide(ui);return;}
    HWND foreground=GetForegroundWindow();if(foreground!=owner && foreground!=ui->popup){if(ui->bar)ShowWindow(ui->bar,SW_HIDE);ui->shown=0;return;}
    if(now<ui->next_read)return;ui->next_read=now+250;
    S14ArmyFrame frame;if(!s14_army_capture(memory,NULL,base,&frame)){hide(ui);return;}
    S14ArmyTrace trace;if(s14_army_observer_snapshot(&trace) && s14_army_trace_matches(&frame,&trace)){frame.trace=trace;frame.connected=1;s14_army_explain(memory,NULL,base,&frame);}
    RECT client,bar;POINT origin={0};int scale;
    if(!GetClientRect(owner,&client) || !ClientToScreen(owner,&origin) || !s14_detail_rect(&frame.panel,client.right,client.bottom,&bar,&scale) || !fonts(ui,scale)){hide(ui);return;}
    ui->frame=frame;if(!ui->bar && !create(ui,0))return;ui->width=bar.right-bar.left;ui->height=bar.bottom-bar.top;
    ui->shown=SetWindowPos(ui->bar,HWND_TOPMOST,origin.x+bar.left,origin.y+bar.top,ui->width,ui->height,SWP_NOACTIVATE|SWP_SHOWWINDOW)!=0;
    InvalidateRect(ui->bar,NULL,FALSE);if(ui->popup_shown)InvalidateRect(ui->popup,NULL,FALSE);
}
void s14_army_ui_destroy(S14ArmyUI *ui){if(ui->bar)DestroyWindow(ui->bar);if(ui->popup)DestroyWindow(ui->popup);if(ui->small)DeleteObject(ui->small);if(ui->body)DeleteObject(ui->body);if(ui->value)DeleteObject(ui->value);memset(ui,0,sizeof(*ui));}
