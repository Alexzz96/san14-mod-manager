#ifndef S14_BLINK_ANCHORS_H
#define S14_BLINK_ANCHORS_H
/* Current EXE function/call spans; runtime compatibility guards, not a game-file fingerprint. */
typedef struct {uintptr_t rva;size_t size;uint64_t hash;} BlinkAnchor;
static const BlinkAnchor blink_anchors[]={
    {0x2482466,0x1c2,UINT64_C(0xffe5d1683e88f2ff)},
    {0x2482680,0x35,UINT64_C(0xddc2d848eca29e45)},
    {0x280570,0x17c,UINT64_C(0xe6d23f69bdda452d)},
    {0x279eb0,0x164,UINT64_C(0xa758ebd8d2cbd01c)},
    {0x2a97a0,0x240,UINT64_C(0x708f8dfaa1e3a89c)},
    {0x257b2e0,0x90,UINT64_C(0xf3158f530db4a03c)},
    {0x296420,0xc0,UINT64_C(0xdf5767f44fc00ffc)},
    {0x299bc0,0x150,UINT64_C(0xf99511f355f97c4b)},
    {0x208bb0,0x7a,UINT64_C(0x2bb83da096f43088)},
    {0x2bb4d0,0x35,UINT64_C(0xe11efc29d981e6e4)},
    {0x25a3e0,0x70,UINT64_C(0x9e8cf3009356b0e2)},
    {0x2054d0,0x10,UINT64_C(0x4d65d421bcde3489)},
    {0x21d720,0x1e,UINT64_C(0xd58b88a18d0c03d5)},
    {0x2ccd50,0x40,UINT64_C(0xdc3838d6a639ebb9)},
    {0x166842,0x21,UINT64_C(0x211f2a8da9aeca1b)},
    {0x257c370,0x1b,UINT64_C(0x6530447a9b6e3741)},
    {0x2578d5f,0x1c,UINT64_C(0x20c76c7eceb2ebcc)},
    {0x2578ff0,0x1c,UINT64_C(0x2542f08d3f66072b)},
};
#endif
