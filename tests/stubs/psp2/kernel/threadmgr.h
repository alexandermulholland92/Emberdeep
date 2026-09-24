/* Minimal threadmgr surface, host type-check only.
   main.c includes this header but calls nothing from it, so there is
   nothing to declare. Anything added here must be reachable from the
   SDK's own psp2/kernel/threadmgr.h - see tools/check_stubs.sh. */
#ifndef STUB_PSP2_THREADMGR_H
#define STUB_PSP2_THREADMGR_H
#endif
