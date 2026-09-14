#ifndef STUB_PSP2_RTC_H
#define STUB_PSP2_RTC_H
typedef struct { unsigned long long tick; } SceRtcTick;
int sceRtcGetCurrentTick(SceRtcTick *t);
#endif
