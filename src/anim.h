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
    int   started;
} ActorAnim;

/* Bind an actor to a model and seed its rest pose. */
void anim_init(ActorAnim *a, const ModelDef *model);

/* Kick off a weapon swing, as swing() does in the browser. */
void anim_swing(ActorAnim *a, float amount, float dur);

/* One frame of animateActor. `baseY` is the actor's resting height,
   `moving` whether it is under way, `telegraphing` whether a boss is in
   its wind-up (which freezes the arms and head). */
void anim_update(ActorAnim *a, float dt, int moving, int telegraphing,
                 float baseY);

#endif
