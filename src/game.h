/* ==========================================================
   Dungeons of the Emberdeep - PS Vita port
   game.h : shared types and module interfaces
   ========================================================== */
#ifndef EMBERDEEP_GAME_H
#define EMBERDEEP_GAME_H

#include <math.h>
#include <string.h>
#include <stdlib.h>

#define SCR_W 960
#define SCR_H 544

#define GRID       42
#define CELL       2.3f
#define WALL_H     3.2f
#define MAX_ROOMS  10
#define MAX_ENEMY  64
#define MAX_PROJ   64
#define MAX_PART   180
#define N_FLOORS   3

#define ROOM_NORMAL 0
#define ROOM_START  1
#define ROOM_BOSS   2

/* enemy type ids */
enum { E_GOBLIN, E_ARCHER, E_SPIDER, E_SKELETON, E_WRAITH, E_HOUND, E_IMP, E_GOLEM,
       B_WARCHIEF, B_COLOSSUS, B_WARDEN, E_COUNT };

/* classes */
enum { CLS_VANGUARD, CLS_PYRO, CLS_RANGER, CLS_COUNT };

/* game states */
enum { ST_CLASS, ST_PLAY, ST_PAUSE, ST_DEAD, ST_WIN };

/* pause menu entries, in the order they are listed */
enum { PAUSE_RESUME, PAUSE_SAVE, PAUSE_QUIT, PAUSE_COUNT };

/* particle / projectile flavours (drive colour only) */
enum { FX_SPARK, FX_FIRE, FX_BLOOD, FX_BONE, FX_DUST, FX_HEAL, FX_GOLD, FX_FROST, FX_VENOM };

typedef struct { float x, y, z; } V3;

typedef struct {
    int x, y, w, h;
    int kind;
    int spawned;
} Room;

typedef struct { float mx, mz; int b[4]; int start; } Input;

typedef struct {
    int   active;
    int   alive;
    int   dying;
    int   awake;                 /* pulled into the fight yet */
    int   isBoss;
    int   type;
    V3    pos;
    float facing;
    float hp, hpMax;
    float dmg, speed, range, cd;
    float atkCd;
    float xp;
    float hitR;
    float baseY;
    float walkT, atkAnim, deathT, deathDur;
    float stun, slow, burnT, burnDps, dotTick;
    float flash;                 /* white hit flash, seconds remaining */
    int   bossState;             /* 0 idle, 1 telegraph */
    float bossTimer;
    int   pending;
    float chargeT;
    V3    chargeDir;
    int   chargeHit;
} Enemy;

typedef struct {
    int   cls;
    V3    pos;
    float facing;
    int   level;
    float hp, hpMax, res, resMax, atk, speed, radius;
    float xp, xpNext;
    float cds[4];
    float invuln, guard, guardAmt;
    float dashT, dashDmg;
    V3    dashDir;
    float walkT, atkAnim;
    float stun, slow, burnT, burnDps, dotTick;
    int   alive;
} Player;

typedef struct {
    int   active;
    V3    pos, vel;
    float life, maxLife, size, grav;
    int   fx;
} Particle;

typedef struct {
    int   active;
    V3    pos, dir;
    float speed, dmg, dist, maxDist, radius;
    int   fromPlayer;
    int   fx;
    float burn;
    float splash, splashDmg;
} Proj;

typedef struct {
    int   state;
    int   depth;
    int   classPick;
    Player pl;
    Enemy  en[MAX_ENEMY];
    Proj   pr[MAX_PROJ];
    Particle pa[MAX_PART];
    int    bossIdx;              /* -1 when no boss present */
    int    portalOn;
    V3     portalPos;
    float  elapsed;
    int    kills;
    float  shake;
    float  hitStop;
    float  bannerT;
    const char *bannerText;
    float  toastT;
    const char *toastText;
    int    pausePick;
    const char *pauseNote;   /* transient menu feedback, never saved */
} Game;

extern Game G;

/* ---- dungeon (dungeon.c) ---- */
extern unsigned char gGrid[GRID][GRID];   /* 0 solid, 1 room, 2 corridor */
extern unsigned char gSeen[GRID][GRID];
extern Room  gRooms[MAX_ROOMS];
extern int   gRoomCount;
extern unsigned int gDungeonGen;   /* changes on every dg_generate() */

void  dg_generate(int depth);
int   dg_walkable(float x, float z, float r);
int   dg_move(V3 *p, float dx, float dz, float r);
Room *dg_room_at(float x, float z);
V3    dg_room_center(const Room *r);
void  dg_mark_seen(float x, float z, int cells);
void  dg_reveal_room(const Room *r);
float dg_world_of(int g);
int   dg_cell_of(float w);

/* ---- actors (actors.c) ---- */
typedef struct {
    const char *name;
    float hp, dmg, speed, range, cd, xp, scale;
    int   ranged;
    float keepAway;
    int   fx;
} EnemyDef;

typedef struct {
    const char *name;
    const char *role;
    float hp, hpPerLv, res, resPerLv, regen;
    float atk, atkPerLv, speed, radius;
    const char *abil[4];
    float cd[4];
    float cost[4];
    int   unlock[4];
} ClassDef;

extern const EnemyDef gEnemyDef[E_COUNT];
extern const ClassDef gClassDef[CLS_COUNT];

void  ac_start_run(int cls);
void  ac_build_floor(int depth);
void  ac_update(float dt);
void  ac_update_player(float dt, const Input *in);
const char *ac_floor_name(int depth);
void  ac_use_ability(int slot);
/* the deferred-effect table, so a save can snapshot it without it
   leaking out of actors.c the rest of the time */
void *ac_timer_state(int *bytes);
void  ac_hurt_player(float amount, int isDot);
void  ac_spawn_particles(V3 at, int count, int fx, float speed, float size, float life);
int   ac_ability_ready(int slot);

/* ---- render (render.c) ---- */
int   rd_init(void);
void  rd_shutdown(void);
void  rd_frame(float dt);

/* ---- small helpers (main.c) ---- */
float rnd01(void);
float rndr(float a, float b);
int   rndi(int a, int b);

static inline float v_dist2(V3 a, V3 b) {
    float dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}
static inline float v_dist(V3 a, V3 b) { return sqrtf(v_dist2(a, b)); }
static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
static inline float approachf(float cur, float tgt, float k) {
    return cur + (tgt - cur) * clampf(k, 0.f, 1.f);
}
static inline float angle_to(float cur, float tgt, float k) {
    float d = tgt - cur;
    while (d > 3.14159265f)  d -= 6.28318531f;
    while (d < -3.14159265f) d += 6.28318531f;
    return cur + d * clampf(k, 0.f, 1.f);
}

#endif /* EMBERDEEP_GAME_H */
