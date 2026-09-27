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

   One deliberate departure: walking. The browser's biped and quadruped
   stride bent the knees the wrong way and swung each leg forward
   straight, so a walk read as walking backwards, and its off-hand swung
   with its own leg; anim.c replaces both (see the comments there).
   tests/model_ref.js applies the same change to the browser's function
   before diffing, so everything else is still checked against the
   original, and tests/gait_test.c checks the walk itself.
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
    float thigh;        /* hip to knee, then the sole's depth below the */
    float sole;         /* knee and its heel and toe along z: enough to */
    float heel, toe;    /* keep whichever foot is planted on the floor  */
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
   and head). Set `scale` first if the actor is drawn at another size. */
void anim_update(ActorAnim *a, float dt, int moving, int telegraphing,
                 float baseY);

#endif
