/* ==========================================================
   actors.c : stats, abilities, AI, combat, progression
   No platform calls - compiles on desktop for the tests.
   ========================================================== */
#include "game.h"

Game G;

/* ---------------- tables ---------------- */

const EnemyDef gEnemyDef[E_COUNT] = {
/*  name             hp   dmg  spd  rng   cd    xp  scale rngd keep  fx        */
  { "Goblin",        14,  3,  2.35f, 1.30f, 1.25f,  22, 1.00f, 0, 0.0f, FX_BLOOD },
  { "Goblin Archer", 12,  4,  2.00f, 8.50f, 2.20f,  28, 1.00f, 1, 5.0f, FX_BLOOD },
  { "Cave Spider",   10,  2,  3.40f, 1.15f, 1.00f,  20, 0.90f, 0, 0.0f, FX_VENOM },
  { "Skeleton",      24,  5,  2.60f, 1.35f, 1.15f,  34, 1.00f, 0, 0.0f, FX_BONE  },
  { "Wraith",        22,  6,  2.20f, 7.50f, 2.50f,  40, 1.00f, 1, 4.5f, FX_FROST },
  { "Bone Hound",    18,  5,  3.80f, 1.25f, 1.10f,  30, 0.95f, 0, 0.0f, FX_BONE  },
  { "Imp",           17,  5,  3.00f, 7.00f, 2.10f,  36, 0.90f, 1, 4.2f, FX_FIRE  },
  { "Magma Golem",   48,  9,  1.70f, 1.90f, 1.90f,  60, 1.25f, 0, 0.0f, FX_DUST  },
  { "Grulk, Warchief",210, 9, 2.50f, 2.10f, 1.30f, 200, 1.45f, 0, 0.0f, FX_BLOOD },
  { "Bone Colossus", 330, 12, 1.90f, 2.40f, 1.60f, 300, 1.80f, 0, 0.0f, FX_BONE  },
  { "Ashen Warden",  460, 14, 2.10f, 2.50f, 1.50f, 500, 1.80f, 0, 0.0f, FX_FIRE  }
};

const ClassDef gClassDef[CLS_COUNT] = {
  { "VANGUARD", "GUARDIAN",
    54, 18, 60, 6, 11.f,  10, 2.6f, 4.25f, 0.46f,
    { "CLEAVE", "SHIELD BASH", "WHIRLWIND", "BULWARK" },
    { 0.50f, 5.f, 8.f, 16.f }, { 0.f, 18.f, 28.f, 34.f }, { 1, 1, 3, 5 } },
  { "PYROMANCER", "CASTER",
    36, 12, 85, 11, 7.f,   10, 3.2f, 4.15f, 0.42f,
    { "EMBER BOLT", "FIREBALL", "FROST NOVA", "METEOR" },
    { 0.45f, 3.5f, 7.f, 14.f }, { 0.f, 20.f, 26.f, 42.f }, { 1, 1, 3, 5 } },
  { "RANGER", "SKIRMISHER",
    42, 14, 70, 8, 10.f,   9, 2.9f, 4.60f, 0.42f,
    { "QUICK SHOT", "VOLLEY", "SHADOW DASH", "VENOM TRAP" },
    { 0.42f, 4.f, 6.f, 11.f }, { 0.f, 16.f, 22.f, 30.f }, { 1, 1, 3, 5 } }
};

/* floor themes: which three enemies spawn, and which boss waits at the end */
static const int  kFloorEnemies[N_FLOORS][3] = {
    { E_GOBLIN, E_ARCHER, E_SPIDER },
    { E_SKELETON, E_WRAITH, E_HOUND },
    { E_IMP, E_GOLEM, E_HOUND }
};
static const int  kFloorBoss[N_FLOORS] = { B_WARCHIEF, B_COLOSSUS, B_WARDEN };
static const char *kFloorName[N_FLOORS] = { "THE UNDERCROFT", "THE OSSUARY", "THE EMBERDEEP" };

const char *ac_floor_name(int depth) {
    return kFloorName[depth < 0 ? 0 : (depth >= N_FLOORS ? N_FLOORS - 1 : depth)];
}

/* ---------------- deferred effects ---------------- */
/* meteors, whirlwind ticks and the boss's staggered volleys all need "do this
   in N seconds"; a tiny fixed table keeps that out of the frame loop.        */
enum { TM_NONE = 0, TM_METEOR, TM_WHIRL, TM_BOSS_METEOR, TM_MELEE_HIT };
typedef struct { int type; float t; float x, z, a; int who; } ATimer;
#define MAX_ATIMER 24
static ATimer sTimers[MAX_ATIMER];

static void timer_add(int type, float delay, float x, float z, float a, int who) {
    int i;
    for (i = 0; i < MAX_ATIMER; i++) {
        if (sTimers[i].type == TM_NONE) {
            sTimers[i].type = type; sTimers[i].t = delay;
            sTimers[i].x = x; sTimers[i].z = z; sTimers[i].a = a; sTimers[i].who = who;
            return;
        }
    }
}
static void timers_clear(void) { memset(sTimers, 0, sizeof(sTimers)); }

void *ac_timer_state(int *bytes) {
    if (bytes) *bytes = (int)sizeof sTimers;
    return sTimers;
}

/* ---------------- small helpers ---------------- */

static void toast(const char *s) { G.toastText = s; G.toastT = 1.6f; }
static void banner(const char *s) { G.bannerText = s; G.bannerT = 2.4f; }
static void shake(float a) { G.shake += a; if (G.shake > 1.2f) G.shake = 1.2f; }
static void hitstop(float t) { if (t > G.hitStop) G.hitStop = t; }

void ac_spawn_particles(V3 at, int count, int fx, float speed, float size, float life) {
    int i, n = 0;
    for (i = 0; i < MAX_PART && n < count; i++) {
        Particle *p = &G.pa[i];
        if (p->active) continue;
        p->active = 1;
        p->pos = at;
        p->vel.x = rndr(-speed, speed);
        p->vel.y = rndr(speed * 0.3f, speed);
        p->vel.z = rndr(-speed, speed);
        p->grav = -7.f;
        p->size = size;
        p->maxLife = life;
        p->life = life;
        p->fx = fx;
        n++;
    }
}

static Enemy *enemy_alloc(void) {
    int i;
    for (i = 0; i < MAX_ENEMY; i++) if (!G.en[i].active) return &G.en[i];
    return 0;
}
static Proj *proj_alloc(void) {
    int i;
    for (i = 0; i < MAX_PROJ; i++) if (!G.pr[i].active) return &G.pr[i];
    return 0;
}

static float roll_damage(float base, int *crit) {
    if (rnd01() < 0.16f) { *crit = 1; return base * 2.f; }
    *crit = 0; return base;
}

/* ---------------- status ---------------- */

static void st_stun(Enemy *e, float t) { if (t > e->stun) e->stun = t; }
static void st_slow(Enemy *e, float t) { if (t > e->slow) e->slow = t; }
static void st_burn(Enemy *e, float t, float dps) {
    if (t > e->burnT) e->burnT = t;
    if (dps > e->burnDps) e->burnDps = dps;
}

/* ---------------- damage ---------------- */

static void ac_kill_enemy(Enemy *e);

static void hurt_enemy(Enemy *e, float dmg, int crit) {
    if (!e->active || !e->alive || e->dying) return;
    e->hp -= dmg;
    e->flash = 0.09f;
    e->awake = 1;
    ac_spawn_particles(e->pos, crit ? 12 : 6, gEnemyDef[e->type].fx,
                       crit ? 3.2f : 2.2f, 0.07f, 0.5f);
    if (crit) shake(0.22f);
    if (e->hp <= 0.f) ac_kill_enemy(e);
}

void ac_hurt_player(float amount, int isDot) {
    Player *p = &G.pl;
    float dmg;
    if (!p->alive) return;
    if (p->invuln > 0.f) return;
    dmg = amount * (p->guard > 0.f ? (1.f - p->guardAmt) : 1.f);
    if (dmg < 1.f) dmg = 1.f;
    p->hp -= dmg;
    if (!isDot) {
        ac_spawn_particles(p->pos, 8, FX_BLOOD, 2.2f, 0.07f, 0.5f);
        shake(0.3f);
    }
    if (p->hp <= 0.f && p->alive) {
        p->alive = 0;
        ac_spawn_particles(p->pos, 28, FX_BLOOD, 3.4f, 0.09f, 0.8f);
        shake(0.9f);
        G.state = ST_DEAD;
    }
}

static void gain_xp(float amount) {
    Player *p = &G.pl;
    const ClassDef *c = &gClassDef[p->cls];
    if (amount <= 0.f) return;
    p->xp += amount;
    while (p->xp >= p->xpNext) {
        p->xp -= p->xpNext;
        p->level++;
        p->hpMax += c->hpPerLv;   p->hp = p->hpMax;
        p->resMax += c->resPerLv; p->res = p->resMax;
        p->atk += c->atkPerLv;
        p->xpNext *= 1.55f;
        ac_spawn_particles(p->pos, 30, FX_GOLD, 2.2f, 0.08f, 1.f);
        toast("LEVEL UP");
    }
}

static void ac_kill_enemy(Enemy *e) {
    e->alive = 0;
    e->dying = 1;
    e->deathDur = e->isBoss ? 1.8f : 0.75f;
    e->deathT = e->deathDur;
    G.kills++;
    ac_spawn_particles(e->pos, e->isBoss ? 48 : 22, gEnemyDef[e->type].fx,
                       e->isBoss ? 5.f : 3.4f, 0.09f, 0.9f);
    gain_xp(e->xp);
    if (e->isBoss) {
        shake(1.f);
        hitstop(0.12f);
        G.bossIdx = -1;
        if (G.depth >= N_FLOORS - 1) {
            G.state = ST_WIN;
        } else {
            Room *br = 0;
            int i;
            for (i = 0; i < gRoomCount; i++) if (gRooms[i].kind == ROOM_BOSS) br = &gRooms[i];
            if (br) {
                V3 c = dg_room_center(br);
                G.portalOn = 1;
                G.portalPos = c;
            }
            banner("THE WAY DOWN OPENS");
        }
    } else {
        shake(0.2f);
        hitstop(0.05f);
    }
}

/* ---------------- area / cone helpers ---------------- */

static int damage_area(V3 at, float radius, float dmg,
                       float stun, float slow, float burn, float burnDps) {
    int i, n = 0;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &G.en[i];
        int crit = 0;
        float d, dmgOut;
        if (!e->active || !e->alive || e->dying) continue;
        d = v_dist(e->pos, at);
        if (d > radius + e->hitR) continue;
        /* sequence the roll: C does not fix argument evaluation order */
        dmgOut = roll_damage(dmg, &crit);
        hurt_enemy(e, dmgOut, crit);
        if (stun > 0.f) st_stun(e, stun);
        if (slow > 0.f) st_slow(e, slow);
        if (burn > 0.f) st_burn(e, burn, burnDps);
        n++;
    }
    return n;
}

static int cone_hit(V3 at, float facing, float range, float spread, float dmg, float stun) {
    int i, n = 0;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &G.en[i];
        float dx, dz, d, ang, diff, dmgOut;
        int crit = 0;
        if (!e->active || !e->alive || e->dying) continue;
        dx = e->pos.x - at.x; dz = e->pos.z - at.z;
        d = sqrtf(dx * dx + dz * dz);
        if (d > range + e->hitR) continue;
        ang = atan2f(dx, dz);
        diff = ang - facing;
        while (diff >  3.14159265f) diff -= 6.28318531f;
        while (diff < -3.14159265f) diff += 6.28318531f;
        if (diff < 0.f) diff = -diff;
        if (diff > spread * 0.5f) continue;
        dmgOut = roll_damage(dmg, &crit);
        hurt_enemy(e, dmgOut, crit);
        if (stun > 0.f) st_stun(e, stun);
        n++;
    }
    return n;
}

static void fire_projectile(V3 at, float dx, float dz, float speed, float dmg,
                            int fromPlayer, int fx, float burn,
                            float splash, float splashDmg) {
    Proj *pr = proj_alloc();
    float len = sqrtf(dx * dx + dz * dz);
    if (!pr || len < 0.0001f) return;
    pr->active = 1;
    pr->pos = at;
    pr->dir.x = dx / len; pr->dir.y = 0.f; pr->dir.z = dz / len;
    pr->speed = speed; pr->dmg = dmg;
    pr->dist = 0.f; pr->maxDist = 22.f; pr->radius = 0.55f;
    pr->fromPlayer = fromPlayer; pr->fx = fx;
    pr->burn = burn; pr->splash = splash; pr->splashDmg = splashDmg;
}

/* ---------------- abilities ---------------- */

int ac_ability_ready(int slot) {
    const ClassDef *c = &gClassDef[G.pl.cls];
    if (G.state != ST_PLAY || !G.pl.alive) return 0;
    if (G.pl.level < c->unlock[slot]) return 0;
    if (G.pl.cds[slot] > 0.f) return 0;
    if (c->cost[slot] > G.pl.res) return 0;
    if (G.pl.stun > 0.f) return 0;
    return 1;
}

void ac_use_ability(int slot) {
    Player *p = &G.pl;
    const ClassDef *c = &gClassDef[p->cls];
    float fx, fz;
    if (!ac_ability_ready(slot)) return;
    p->cds[slot] = c->cd[slot];
    p->res -= c->cost[slot];
    fx = sinf(p->facing); fz = cosf(p->facing);
    p->atkAnim = 0.3f;

    if (p->cls == CLS_VANGUARD) {
        if (slot == 0) {
            if (cone_hit(p->pos, p->facing, 2.7f, 2.0f, p->atk, 0.f)) shake(0.16f);
        } else if (slot == 1) {                     /* shield bash */
            p->dashT = 0.20f; p->dashDir.x = fx; p->dashDir.z = fz;
            p->dashDmg = 0.f;
            cone_hit(p->pos, p->facing, 2.6f, 1.5f, p->atk * 1.3f, 1.4f);
            ac_spawn_particles(p->pos, 12, FX_SPARK, 2.6f, 0.08f, 0.5f);
            shake(0.3f);
        } else if (slot == 2) {                     /* whirlwind: three ticks */
            timer_add(TM_WHIRL, 0.00f, 0, 0, 0, 0);
            timer_add(TM_WHIRL, 0.19f, 0, 0, 0, 0);
            timer_add(TM_WHIRL, 0.38f, 0, 0, 0, 0);
        } else {                                    /* bulwark */
            p->guard = 4.5f; p->guardAmt = 0.65f;
            p->hp += p->hpMax * 0.25f;
            if (p->hp > p->hpMax) p->hp = p->hpMax;
            ac_spawn_particles(p->pos, 22, FX_HEAL, 1.8f, 0.08f, 0.9f);
            toast("BULWARK");
        }
    } else if (p->cls == CLS_PYRO) {
        V3 muzzle = p->pos;
        muzzle.y = 1.1f;
        muzzle.x += fx * 0.7f; muzzle.z += fz * 0.7f;
        if (slot == 0) {
            fire_projectile(muzzle, fx, fz, 17.f, p->atk * 0.95f, 1, FX_FIRE, 0.f, 0.f, 0.f);
        } else if (slot == 1) {                     /* fireball */
            fire_projectile(muzzle, fx, fz, 12.f, p->atk * 1.5f, 1, FX_FIRE,
                            4.f, 2.7f, p->atk * 0.9f);
            shake(0.12f);
        } else if (slot == 2) {                     /* frost nova */
            damage_area(p->pos, 4.4f, p->atk * 0.9f, 0.f, 3.5f, 0.f, 0.f);
            ac_spawn_particles(p->pos, 34, FX_FROST, 4.f, 0.09f, 0.8f);
            shake(0.35f);
        } else {                                    /* meteor */
            timer_add(TM_METEOR, 0.95f, p->pos.x + fx * 5.2f, p->pos.z + fz * 5.2f,
                      p->atk * 3.0f, 0);
            toast("METEOR FALLING");
        }
    } else {                                        /* ranger */
        V3 muzzle = p->pos;
        muzzle.y = 1.05f;
        muzzle.x += fx * 0.7f; muzzle.z += fz * 0.7f;
        if (slot == 0) {
            fire_projectile(muzzle, fx, fz, 19.f, p->atk, 1, FX_SPARK, 0.f, 0.f, 0.f);
        } else if (slot == 1) {                     /* volley */
            int k;
            for (k = -1; k <= 1; k++) {
                float a = p->facing + k * 0.24f;
                fire_projectile(muzzle, sinf(a), cosf(a), 18.f, p->atk * 0.9f,
                                1, FX_VENOM, 0.f, 0.f, 0.f);
            }
            shake(0.1f);
        } else if (slot == 2) {                     /* shadow dash */
            p->dashT = 0.26f; p->dashDir.x = fx; p->dashDir.z = fz;
            p->dashDmg = p->atk * 1.4f;
            if (p->invuln < 0.42f) p->invuln = 0.42f;
            ac_spawn_particles(p->pos, 18, FX_FROST, 2.4f, 0.08f, 0.5f);
        } else {                                    /* venom trap */
            damage_area(p->pos, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);  /* no immediate hit */
            timer_add(TM_METEOR, 0.6f, p->pos.x, p->pos.z, p->atk * 1.5f, 0);
            toast("TRAP ARMED");
        }
    }
}

/* ---------------- spawning ---------------- */

static void spawn_enemy(int type, float x, float z, int isBoss) {
    const EnemyDef *d = &gEnemyDef[type];
    Enemy *e = enemy_alloc();
    if (!e) return;
    memset(e, 0, sizeof(*e));
    e->active = 1; e->alive = 1;
    e->type = type; e->isBoss = isBoss;
    e->pos.x = x; e->pos.z = z; e->pos.y = 0.f;
    e->hp = e->hpMax = d->hp;
    e->dmg = d->dmg; e->speed = d->speed;
    e->range = d->range; e->cd = d->cd; e->xp = d->xp;
    e->hitR = 0.45f * d->scale;
    e->baseY = (type == E_WRAITH || type == E_IMP) ? 0.45f : 0.f;
    e->atkCd = rndr(0.2f, 1.f);
    e->walkT = rndr(0.f, 6.f);
    e->bossTimer = rndr(2.f, 3.5f);
    if (isBoss) {
        e->awake = 1;
        G.bossIdx = (int)(e - G.en);
    }
}

static void populate_room(Room *r) {
    int i, count;
    r->spawned = 1;
    if (r->kind == ROOM_BOSS) {
        V3 c = dg_room_center(r);
        spawn_enemy(kFloorBoss[G.depth], c.x, c.z - 1.5f, 1);
        banner(gEnemyDef[kFloorBoss[G.depth]].name);
        shake(0.4f);
        return;
    }
    count = (r->w * r->h) / 17 + G.depth;
    if (count < 2) count = 2;
    if (count > 5) count = 5;
    for (i = 0; i < count; i++) {
        int gx = rndi(r->x + 1, r->x + r->w - 2);
        int gy = rndi(r->y + 1, r->y + r->h - 2);
        float wx = dg_world_of(gx), wz = dg_world_of(gy);
        float dx = wx - G.pl.pos.x, dz = wz - G.pl.pos.z;
        if (dx * dx + dz * dz < 16.f) continue;      /* never on top of the player */
        spawn_enemy(kFloorEnemies[G.depth][rndi(0, 2)], wx, wz, 0);
    }
}

/* ---------------- floors ---------------- */

void ac_build_floor(int depth) {
    V3 c;
    G.depth = depth;
    memset(G.en, 0, sizeof(G.en));
    memset(G.pr, 0, sizeof(G.pr));
    memset(G.pa, 0, sizeof(G.pa));
    timers_clear();
    G.bossIdx = -1;
    G.portalOn = 0;

    dg_generate(depth);

    c = dg_room_center(&gRooms[0]);
    G.pl.pos = c;
    G.pl.pos.y = 0.f;
    G.pl.facing = 0.f;
    G.pl.dashT = 0.f;
    G.pl.stun = G.pl.slow = G.pl.burnT = 0.f;
    if (G.pl.invuln < 1.2f) G.pl.invuln = 1.2f;   /* grace on arrival */

    dg_reveal_room(&gRooms[0]);
    dg_mark_seen(c.x, c.z, 4);
    banner(ac_floor_name(depth));
}

void ac_start_run(int cls) {
    const ClassDef *c = &gClassDef[cls];
    memset(&G.pl, 0, sizeof(G.pl));
    G.pl.cls = cls;
    G.pl.level = 1;
    G.pl.hp = G.pl.hpMax = c->hp;
    G.pl.res = G.pl.resMax = c->res;
    G.pl.atk = c->atk;
    G.pl.speed = c->speed;
    G.pl.radius = c->radius;
    G.pl.xp = 0.f; G.pl.xpNext = 70.f;
    G.pl.alive = 1;
    G.kills = 0;
    G.shake = 0.f; G.hitStop = 0.f;
    G.state = ST_PLAY;
    ac_build_floor(0);
}

static void next_floor(void) {
    ac_build_floor(G.depth + 1);
    G.pl.hp += G.pl.hpMax * 0.4f;
    if (G.pl.hp > G.pl.hpMax) G.pl.hp = G.pl.hpMax;
}

/* ---------------- per-frame updates ---------------- */

static void update_timers(float dt) {
    int i;
    for (i = 0; i < MAX_ATIMER; i++) {
        ATimer *t = &sTimers[i];
        if (!t->type) continue;
        t->t -= dt;
        if (t->t > 0.f) continue;
        switch (t->type) {
            case TM_WHIRL: {
                V3 at = G.pl.pos;
                damage_area(at, 3.3f, G.pl.atk * 0.8f, 0.f, 0.f, 0.f, 0.f);
                G.pl.atkAnim = 0.2f;
                shake(0.2f);
            } break;
            case TM_METEOR: {
                V3 at; at.x = t->x; at.y = 0.f; at.z = t->z;
                damage_area(at, 3.4f, t->a, 0.f, 0.f, 5.f, 7.f);
                ac_spawn_particles(at, 40, FX_FIRE, 6.f, 0.12f, 1.f);
                shake(0.9f); hitstop(0.07f);
            } break;
            case TM_BOSS_METEOR: {
                V3 at; at.x = t->x; at.y = 0.f; at.z = t->z;
                float d = v_dist(at, G.pl.pos);
                ac_spawn_particles(at, 30, FX_FIRE, 5.f, 0.11f, 0.8f);
                if (d < 2.8f) ac_hurt_player(t->a, 0);
                shake(0.5f);
            } break;
            case TM_MELEE_HIT: {
                Enemy *e = &G.en[t->who];
                if (e->active && e->alive && G.pl.alive &&
                    v_dist(e->pos, G.pl.pos) <= e->range + 0.9f)
                    ac_hurt_player(e->dmg, 0);
            } break;
            default: break;
        }
        t->type = TM_NONE;
    }
}

static void update_player(float dt, const Input *in) {
    Player *p = &G.pl;
    const ClassDef *c = &gClassDef[p->cls];
    int i, moving = 0;

    if (!p->alive) return;

    p->invuln = p->invuln > dt ? p->invuln - dt : 0.f;
    p->guard  = p->guard  > dt ? p->guard  - dt : 0.f;
    p->stun   = p->stun   > dt ? p->stun   - dt : 0.f;
    p->slow   = p->slow   > dt ? p->slow   - dt : 0.f;
    p->res += c->regen * dt;
    if (p->res > p->resMax) p->res = p->resMax;
    for (i = 0; i < 4; i++) p->cds[i] = p->cds[i] > dt ? p->cds[i] - dt : 0.f;

    /* burn damage over time */
    if (p->burnT > 0.f) {
        p->burnT -= dt;
        p->dotTick -= dt;
        if (p->dotTick <= 0.f) { p->dotTick = 0.5f; ac_hurt_player(p->burnDps * 0.5f, 1); }
    }

    if (p->dashT > 0.f) {
        float step = (p->dashDmg > 0.f ? 17.f : 12.f) * dt;
        p->dashT -= dt;
        dg_move(&p->pos, p->dashDir.x * step, p->dashDir.z * step, p->radius);
        moving = 1;
        if (p->dashDmg > 0.f) {
            for (i = 0; i < MAX_ENEMY; i++) {
                Enemy *e = &G.en[i];
                int crit = 0;
                float dmgOut;
                if (!e->active || !e->alive || e->dying) continue;
                if (v_dist(e->pos, p->pos) < 1.4f && e->flash <= 0.f) {
                    dmgOut = roll_damage(p->dashDmg, &crit);
                    hurt_enemy(e, dmgOut, crit);
                }
            }
        }
    } else if (p->stun <= 0.f) {
        float mag = sqrtf(in->mx * in->mx + in->mz * in->mz);
        if (mag > 0.15f) {
            float nx = in->mx / mag, nz = in->mz / mag;
            float sp = p->speed * (p->slow > 0.f ? 0.5f : 1.f) * (mag > 1.f ? 1.f : mag) * dt;
            moving = dg_move(&p->pos, nx * sp, nz * sp, p->radius);
            p->facing = atan2f(nx, nz);
        }
    }

    p->walkT += dt * (moving ? 9.f : 2.f);
    if (p->atkAnim > 0.f) p->atkAnim -= dt;

    dg_mark_seen(p->pos.x, p->pos.z, 3);
    {
        Room *r = dg_room_at(p->pos.x, p->pos.z);
        if (r && !r->spawned) { dg_reveal_room(r); populate_room(r); }
        else if (r) dg_reveal_room(r);
    }
    if (G.portalOn && v_dist(p->pos, G.portalPos) < 1.5f) next_floor();
}

static void boss_exec(Enemy *e, int which);

static void update_boss(Enemy *e, float dt, float d) {
    Player *p = &G.pl;

    if (e->chargeT > 0.f) {
        float step = 15.f * dt;
        e->chargeT -= dt;
        dg_move(&e->pos, e->chargeDir.x * step, e->chargeDir.z * step, e->hitR);
        if (!e->chargeHit && v_dist(e->pos, p->pos) < e->hitR + 1.3f) {
            e->chargeHit = 1;
            ac_hurt_player(e->dmg * 1.4f, 0);
            shake(0.5f);
        }
        return;
    }

    if (e->bossState == 1) {                 /* telegraph */
        e->bossTimer -= dt;
        if (e->bossTimer <= 0.f) {
            boss_exec(e, e->pending);
            e->bossState = 0;
            e->bossTimer = rndr(2.6f, 4.2f);
        }
        return;
    }

    if (e->stun <= 0.f && p->alive) {
        if (d > e->range) {
            float dx = p->pos.x - e->pos.x, dz = p->pos.z - e->pos.z;
            float len = sqrtf(dx * dx + dz * dz);
            if (len > 0.001f) {
                float sp = e->speed * (e->slow > 0.f ? 0.5f : 1.f) * dt;
                dg_move(&e->pos, dx / len * sp, dz / len * sp, e->hitR);
            }
            e->facing = angle_to(e->facing, atan2f(dx, dz), dt * 9.f);
        }
        if (e->atkCd <= 0.f && d <= e->range + 0.4f) {
            e->atkCd = e->cd;
            e->atkAnim = 0.32f;
            timer_add(TM_MELEE_HIT, 0.16f, 0, 0, 0, (int)(e - G.en));
        }
        e->bossTimer -= dt;
        if (e->bossTimer <= 0.f && d < 14.f) {
            e->pending = rndi(0, 2);
            e->bossState = 1;
            e->bossTimer = 0.85f;
            toast("IT READIES AN ATTACK");
        }
    }
}

static void boss_exec(Enemy *e, int which) {
    Player *p = &G.pl;
    V3 here = e->pos;
    int tier = e->type - B_WARCHIEF;      /* 0,1,2 */

    if (tier == 0) {
        if (which == 0) {                              /* charge */
            float dx = p->pos.x - here.x, dz = p->pos.z - here.z;
            float len = sqrtf(dx * dx + dz * dz);
            if (len > 0.001f) { e->chargeDir.x = dx / len; e->chargeDir.z = dz / len; }
            e->chargeT = 0.75f; e->chargeHit = 0;
            shake(0.35f);
        } else if (which == 1) {                       /* cleave */
            if (v_dist(here, p->pos) < 4.f) ac_hurt_player(e->dmg * 1.3f, 0);
            ac_spawn_particles(here, 18, FX_DUST, 3.f, 0.1f, 0.6f);
            shake(0.5f);
        } else {                                       /* summon */
            int i;
            for (i = 0; i < 3; i++) {
                float a = (float)i / 3.f * 6.28318531f;
                float sx = here.x + cosf(a) * 2.6f, sz = here.z + sinf(a) * 2.6f;
                if (!dg_walkable(sx, sz, 0.5f)) continue;
                spawn_enemy(kFloorEnemies[G.depth][rndi(0, 2)], sx, sz, 0);
            }
            toast("REINFORCEMENTS");
        }
    } else if (tier == 1) {
        if (which == 0) {                              /* slam */
            if (v_dist(here, p->pos) < 5.5f) ac_hurt_player(e->dmg * 1.5f, 0);
            ac_spawn_particles(here, 36, FX_DUST, 5.f, 0.11f, 0.8f);
            shake(0.9f); hitstop(0.06f);
        } else if (which == 1) {                       /* bone shards */
            int s;
            for (s = 0; s < 8; s++) {
                float a = (float)s / 8.f * 6.28318531f;
                V3 at = here; at.y = 1.2f;
                fire_projectile(at, sinf(a), cosf(a), 10.f, e->dmg * 0.8f, 0, FX_BONE,
                                0.f, 0.f, 0.f);
            }
            shake(0.3f);
        } else {                                       /* stomp */
            if (v_dist(here, p->pos) < 6.f) ac_hurt_player(e->dmg * 0.7f, 0);
            ac_spawn_particles(here, 24, FX_DUST, 4.f, 0.1f, 0.7f);
            shake(0.5f);
        }
    } else {
        if (which == 0) {                              /* fire nova */
            if (v_dist(here, p->pos) < 6.5f) {
                ac_hurt_player(e->dmg * 1.4f, 0);
                G.pl.burnT = 4.f; G.pl.burnDps = 5.f;
            }
            ac_spawn_particles(here, 48, FX_FIRE, 7.f, 0.11f, 0.9f);
            shake(0.8f); hitstop(0.05f);
        } else if (which == 1) {                       /* meteor rain */
            int m;
            for (m = 0; m < 4; m++)
                timer_add(TM_BOSS_METEOR, 0.55f * m + 0.75f,
                          p->pos.x, p->pos.z, e->dmg * 1.2f, 0);
            toast("THE CEILING CRACKS");
        } else {                                       /* charge */
            float dx = p->pos.x - here.x, dz = p->pos.z - here.z;
            float len = sqrtf(dx * dx + dz * dz);
            if (len > 0.001f) { e->chargeDir.x = dx / len; e->chargeDir.z = dz / len; }
            e->chargeT = 0.75f; e->chargeHit = 0;
            shake(0.35f);
        }
    }
}

static void update_enemies(float dt) {
    Player *p = &G.pl;
    int i;
    for (i = 0; i < MAX_ENEMY; i++) {
        Enemy *e = &G.en[i];
        const EnemyDef *d;
        float dist;
        int moving = 0;

        if (!e->active) continue;
        if (e->flash > 0.f) e->flash -= dt;

        if (e->dying) {
            e->deathT -= dt;
            if (e->deathT <= 0.f) e->active = 0;
            continue;
        }
        if (!e->alive) continue;

        d = &gEnemyDef[e->type];

        e->stun = e->stun > dt ? e->stun - dt : 0.f;
        e->slow = e->slow > dt ? e->slow - dt : 0.f;
        if (e->burnT > 0.f) {
            e->burnT -= dt;
            e->dotTick -= dt;
            if (e->dotTick <= 0.f) {
                e->dotTick = 0.5f;
                hurt_enemy(e, e->burnDps * 0.5f, 0);
                if (!e->alive) continue;
            }
        }
        e->atkCd -= dt;
        if (e->atkAnim > 0.f) e->atkAnim -= dt;

        dist = v_dist(e->pos, p->pos);

        /* wake by proximity so a room is pulled apart in pieces, not all at once */
        if (!e->awake) {
            if (dist < 11.f) e->awake = 1;
            else { e->walkT += dt * 2.f; continue; }
        }

        if (e->isBoss) { update_boss(e, dt, dist); e->walkT += dt * 6.f; continue; }

        if (e->stun <= 0.f && p->alive) {
            float want = d->ranged ? d->keepAway : e->range;
            float dx = p->pos.x - e->pos.x, dz = p->pos.z - e->pos.z;
            float len = sqrtf(dx * dx + dz * dz);
            float sp = e->speed * (e->slow > 0.f ? 0.5f : 1.f) * dt;
            if (len > 0.001f) {
                if (dist > want + 0.4f) {
                    moving = dg_move(&e->pos, dx / len * sp, dz / len * sp, e->hitR);
                } else if (d->ranged && dist < want - 1.2f) {
                    moving = dg_move(&e->pos, -dx / len * sp * 0.7f, -dz / len * sp * 0.7f, e->hitR);
                }
                e->facing = angle_to(e->facing, atan2f(dx, dz), dt * 9.f);
            }
            if (e->atkCd <= 0.f && dist <= (d->ranged ? d->range : e->range + 0.25f)) {
                e->atkCd = e->cd;
                e->atkAnim = 0.3f;
                if (d->ranged) {
                    V3 at = e->pos; at.y = e->baseY + 1.f;
                    fire_projectile(at, dx, dz, 12.f, e->dmg, 0, d->fx, 0.f, 0.f, 0.f);
                } else {
                    timer_add(TM_MELEE_HIT, 0.14f, 0, 0, 0, i);
                }
            }
        }
        e->walkT += dt * (moving ? 9.f : 2.f);
    }
}

static void update_projectiles(float dt) {
    int i, j;
    for (i = 0; i < MAX_PROJ; i++) {
        Proj *pr = &G.pr[i];
        float step;
        int done = 0;
        if (!pr->active) continue;
        step = pr->speed * dt;
        pr->pos.x += pr->dir.x * step;
        pr->pos.z += pr->dir.z * step;
        pr->dist += step;

        if (!dg_walkable(pr->pos.x, pr->pos.z, 0.12f) || pr->dist > pr->maxDist) done = 1;

        if (!done && pr->fromPlayer) {
            for (j = 0; j < MAX_ENEMY; j++) {
                Enemy *e = &G.en[j];
                int crit = 0;
                float dmgOut;
                if (!e->active || !e->alive || e->dying) continue;
                if (v_dist(pr->pos, e->pos) > pr->radius + e->hitR) continue;
                dmgOut = roll_damage(pr->dmg, &crit);
                hurt_enemy(e, dmgOut, crit);
                if (pr->burn > 0.f) st_burn(e, pr->burn, 5.f);
                if (pr->splash > 0.f) {
                    damage_area(pr->pos, pr->splash, pr->splashDmg, 0.f, 0.f, pr->burn, 5.f);
                    shake(0.25f);
                }
                done = 1;
                break;
            }
        } else if (!done && !pr->fromPlayer && G.pl.alive) {
            if (v_dist(pr->pos, G.pl.pos) < pr->radius + G.pl.radius) {
                ac_hurt_player(pr->dmg, 0);
                if (pr->burn > 0.f) { G.pl.burnT = pr->burn; G.pl.burnDps = 4.f; }
                done = 1;
            }
        }
        if (done) {
            ac_spawn_particles(pr->pos, 8, pr->fx, 2.6f, 0.08f, 0.4f);
            pr->active = 0;
        }
    }
}

static void update_particles(float dt) {
    int i;
    for (i = 0; i < MAX_PART; i++) {
        Particle *p = &G.pa[i];
        if (!p->active) continue;
        p->life -= dt;
        if (p->life <= 0.f) { p->active = 0; continue; }
        p->vel.y += p->grav * dt;
        p->pos.x += p->vel.x * dt;
        p->pos.y += p->vel.y * dt;
        p->pos.z += p->vel.z * dt;
        if (p->pos.y < 0.05f) {
            p->pos.y = 0.05f;
            p->vel.y *= -0.35f;
            p->vel.x *= 0.6f;
            p->vel.z *= 0.6f;
        }
    }
}

void ac_update(float dt) {
    float sim = dt;
    if (G.hitStop > 0.f) { G.hitStop -= dt; sim = dt * 0.08f; }

    G.elapsed += dt;
    if (G.toastT  > 0.f) G.toastT  -= dt;
    if (G.bannerT > 0.f) G.bannerT -= dt;
    if (G.shake   > 0.f) { G.shake -= dt * 2.4f; if (G.shake < 0.f) G.shake = 0.f; }

    if (G.state != ST_PLAY) return;

    update_timers(sim);
    update_enemies(sim);
    update_projectiles(sim);
    update_particles(sim);
}

/* player update is separate so input can be applied before the world moves */
void ac_update_player(float dt, const Input *in) {
    float sim = (G.hitStop > 0.f) ? dt * 0.08f : dt;
    if (G.state != ST_PLAY) return;
    update_player(sim, in);
}
