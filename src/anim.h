/* ==========================================================
   anim.h : the browser build's animateActor, ported.

   The browser keeps its animation state on the entity and eases several
   joints toward targets frame by frame, so the pose is not a pure
   function of the clock - it has to persist. ActorAnim holds that state
   for one actor, alongside the Pose the rig is drawn from.

   The clocks live here rather than in the simulation: the browser
   advances walkT inside animateActor at rates that differ per body plan,
   and keeping them on this side leaves actors.c (and its host tests)
   untouched.

   The walk was reworked on both sides together (the browser's bent its
   knees backwards, skated, and swung the off-hand with its own leg), so
   emberdeep.html in the repository carries the same stride anim.c does
   and animcheck still diffs the two directly. tests/gait_test.c checks
   the walk itself: that knees bend the right way, feet stay planted
   without skating or scraping, and so on.
   ========================================================== */
#ifndef EMBERDEEP_ANIM_H
#define EMBERDEEP_ANIM_H

#include "model.h"

typedef struct {
    const ModelDef *model;
    Pose  pose;
    float walkT;
    float floatT;
    float atkAnim;      /* seconds left in the current swing   */
    float atkAnimDur;   /* its total length                    */
    float atkSwing;     /* how far the follow-through carries  */
    float rootY;        /* the bob animateActor writes to position.y */
    float lastX, lastZ; /* to tell whether the actor is moving */
    float legRestZ[MODEL_MAX_LEGS];   /* arachnid legs splay from a rest angle */
    float stride;       /* 0.05 standing .. 1 walking, eased between */
    float thigh;        /* hip to knee; the foot box's centre below and  */
    float footY, footZ; /* ahead of the knee, its half height, and its   */
    float half;         /* heel and toe either side of centre: enough to */
    float heel, toe;    /* keep whichever foot is planted on the floor   */
    float cycle;        /* ground one full stride cycle covers, at scale 1 */
    float speed;        /* ground speed, set by the caller; 0 if unknown */
    float pace;         /* that speed, eased                              */
    float scale;        /* the size the actor is drawn at; 1 by default */
    int   started;
} ActorAnim;

/* Bind an actor to a model and seed its rest pose. */
void anim_init(ActorAnim *a, const ModelDef *model);

/* Kick off a weapon swing, as swing() does in the browser. */
void anim_swing(ActorAnim *a, float amount, float dur);

/* One frame of animateActor. `baseY` is the actor's resting height,
   `moving` whether it is under way (negative when it is backing away
   while still facing forward, which plays the stride in reverse),
   `telegraphing` whether a boss is in its wind-up (which freezes the arms
   and head). Set `scale` first if the actor is drawn at another size, and
   `speed` (units per second) so the stride keeps pace with the ground
   rather than stepping at a fixed rate. */
void anim_update(ActorAnim *a, float dt, int moving, int telegraphing,
                 float baseY);

#endif
