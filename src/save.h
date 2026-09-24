/* ==========================================================
   save.h : snapshot of a run in progress.

   Packing is kept apart from the file I/O so the format can be
   round-tripped on the host without a Vita - sv_pack/sv_unpack touch
   nothing but memory, and only sv_save/sv_load reach for ux0:.

   The snapshot is a raw image of the live structs, so it is only valid
   for the build that wrote it: the header carries a version and the
   sizes of everything inside, and a load that disagrees is refused
   rather than trusted. Rebuilding the game invalidates old saves, which
   is the right trade for a single-player roguelike.
   ========================================================== */
#ifndef EMBERDEEP_SAVE_H
#define EMBERDEEP_SAVE_H

#define SV_PATH_DIR  "ux0:data/emberdeep"
#define SV_PATH_FILE "ux0:data/emberdeep/save.bin"

/* generous: the live state is about 30 KB */
#define SV_MAX_BYTES 65536

/* Pack the current run into `buf`. Returns the byte count, or 0 if the
   buffer is too small. The saved state always reads back as ST_PLAY, so
   a run saved from the pause menu resumes unpaused. */
int sv_pack(void *buf, int cap);

/* Restore a run from `buf`. Returns 1 on success, 0 if the header does
   not match this build. On success the world is marked for a rebuild. */
int sv_unpack(const void *buf, int len);

#ifndef SV_HOST_HARNESS
int sv_save(void);     /* 1 on success */
int sv_load(void);     /* 1 on success */
int sv_exists(void);   /* 1 if a save file is present */
void sv_delete(void);
#endif

#endif
