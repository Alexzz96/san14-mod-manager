#ifndef S14_MAP_EFFECTS_UI_H
#define S14_MAP_EFFECTS_UI_H
#include "map_effects_model.h"
typedef struct {
    HWND halo,card,owner;HINSTANCE instance;HFONT title,body,small;
    uintptr_t base;ULONGLONG next_read;int ready,scale,halo_width,halo_height,card_width,card_height,halo_shown,card_shown;
    S14MapCache cache;S14MapFrame frame;S14MapBatch batch;
    int feature_flags;unsigned int preferences;wchar_t card_name[24];
} S14MapEffectsUI;
void s14_map_ui_tick(S14MapEffectsUI*,HINSTANCE,HWND,uintptr_t,int,unsigned int,int,ULONGLONG);
void s14_map_ui_destroy(S14MapEffectsUI*);
void s14_map_halo_pixels(unsigned int*,int,int,int);
void s14_map_card_paint(HDC,int,int,int,HFONT,HFONT,HFONT);
#endif
