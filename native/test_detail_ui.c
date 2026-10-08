#include <assert.h>
#ifdef NDEBUG
#error Assertions required
#endif
#include "detail_ui.c"
static int render(S14DetailUI *ui,const char *path) {
    HDC dc=CreateCompatibleDC(NULL);BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=ui->width;bi.bmiHeader.biHeight=-ui->height;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
    void *bits;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&bits,NULL,0);if(!bitmap) {DeleteDC(dc);return 0;}HGDIOBJ old=SelectObject(dc,bitmap);s14_detail_paint(ui,dc);GdiFlush();
    BITMAPFILEHEADER header={0};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+ui->width*ui->height*4;
    FILE *file=fopen(path,"wb");int ok=file && fwrite(&header,sizeof(header),1,file)==1 && fwrite(&bi.bmiHeader,sizeof(BITMAPINFOHEADER),1,file)==1 && fwrite(bits,ui->width*ui->height*4,1,file)==1;if(file) fclose(file);
    SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return ok;
}
int main(int argc,char **argv) {
    HWND foreground=GetForegroundWindow();S14DetailUI ui={0};assert(fonts(&ui,96));assert(create(&ui,GetModuleHandleW(NULL),NULL));
    LONG_PTR style=GetWindowLongPtrW(ui.window,GWL_EXSTYLE);assert((style&(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_LAYERED))==(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_LAYERED));
    assert(SendMessageW(ui.window,WM_NCHITTEST,0,0)==HTTRANSPARENT);assert(SendMessageW(ui.window,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE);
    wcscpy(ui.frame.name,L"曹仁");ui.width=944;ui.height=76;ui.connected=ui.bound=1;ui.stats=(S14BattleTotals){.valid_mask=31,.enemy_loss=5000,.units_routed=1,.own_loss=242,.officers_injured=1};
    if(argc>1) assert(render(&ui,argv[1]));
    assert(SetWindowPos(ui.window,NULL,-20000,-20000,ui.width,ui.height,SWP_NOACTIVATE|SWP_SHOWWINDOW));assert(GetForegroundWindow()==foreground);
    s14_detail_tick(&ui,GetModuleHandleW(NULL),NULL,0,0,0);assert(!IsWindowVisible(ui.window) && !ui.shown);
    assert(fonts(&ui,192));ui.width*=2;ui.height*=2;
    if(argc>2) assert(render(&ui,argv[2]));s14_detail_destroy(&ui);assert(GetForegroundWindow()==foreground);
    printf("{\"status\":\"passed\",\"click_through\":true,\"does_not_steal_focus\":true,\"disable_hides\":true,\"scales\":[96,192]}\n");return 0;
}
