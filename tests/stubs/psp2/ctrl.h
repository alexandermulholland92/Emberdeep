#ifndef STUB_PSP2_CTRL_H
#define STUB_PSP2_CTRL_H
typedef struct { unsigned int timeStamp; unsigned int buttons;
                 unsigned char lx, ly, rx, ry; unsigned char reserved[16]; } SceCtrlData;
#define SCE_CTRL_MODE_ANALOG 1
#define SCE_CTRL_UP       0x00000010
#define SCE_CTRL_RIGHT    0x00000020
#define SCE_CTRL_DOWN     0x00000040
#define SCE_CTRL_LEFT     0x00000080
#define SCE_CTRL_TRIANGLE 0x00001000
#define SCE_CTRL_CIRCLE   0x00002000
#define SCE_CTRL_CROSS    0x00004000
#define SCE_CTRL_SQUARE   0x00008000
#define SCE_CTRL_START    0x00000008
int sceCtrlSetSamplingMode(int mode);
int sceCtrlPeekBufferPositive(int port, SceCtrlData *pad, int count);
#endif
