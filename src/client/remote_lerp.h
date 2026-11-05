/* Remote player smoothing without engine dependencies:
   per-player history of snapshots, delay D from the observed interval between position changes,
   the drawn state at t = cl.time - extra. Times are ms of the client timeline (cl.servertime labels). */

#pragma once

#include <stdbool.h>

#define RL_RING         32      // records per ring
#define RL_WINDOW       250     // ms of history; a longer interval between changes is a stop, not a send gap
#define RL_GAPS         64      // change intervals kept, over at most RL_GAP_WINDOW
#define RL_GAP_WINDOW   1000
#define RL_MAX_EXTRA    100     // D <= I + RL_MAX_EXTRA
#define RL_HOLD         250     // a new D is taken once the percentile has differed this long
#define RL_ABSENT       100     // no snapshot for longer (or a label not after the last): the history restarts
#define RL_ANIM         100     // BASE_FRAMETIME
#define RL_PLAYERS      256     // MAX_CLIENTS

typedef struct {
    int t;                      // snapshot time
    float org[3], ang[3];
    int frame;                  // alias model frame
    int model;                  // reset key: modelindex (entity), clientNum (chase camera)
    int stat;                   // reset key: STAT_CHASE (chase camera), 0 for an entity
    bool teleport;              // teleport event, or the teleport bit flipped
} rl_snap_t;

typedef struct {
    rl_snap_t s;
    bool moved, turned;         // position / angles differ from the previous record (change points)
} rl_rec_t;

typedef struct {
    rl_rec_t rec[RL_RING];
    int head, count;            // rec[head] is the newest
    rl_snap_t moved, turned;    // last change point of position / angles: k0 once the window has passed it
    bool has_moved, has_turned;
    int gap[RL_GAPS], gap_t[RL_GAPS], ngaps, gap_head;
    int D, since;               // accepted delay (0: none yet); since: the percentile differs from D since then
    float extra, shown;         // added delay; t drawn in the last frame of this history
    bool has_shown;
} rl_ring_t;

typedef struct {
    float t;
    float org[3], ang[3];
    int frame, oldframe;
    float backlerp;
} rl_out_t;

void RL_Reset(rl_ring_t *r);
// a snapshot; jump: a move along any axis longer than this restarts the history (512 entity, 256 chase camera)
void RL_Push(rl_ring_t *r, const rl_snap_t *s, float jump);
// once per drawn frame: dt = growth of cl.time since the last frame (0 while paused or clamped),
// I = snapshot interval (RL_Interval); moves extra toward D - I by at most dt / 10, returns extra
float RL_Advance(rl_ring_t *r, int dt, int I);
// position and angles at t with delay D, each between its own change points; animation: frame of the last
// record <= t, oldframe -> frame over RL_ANIM from the record before that frame's run; false: empty ring
bool RL_At(const rl_ring_t *r, float t, float D, rl_out_t *out);
// the drawn state at t = cltime - extra, never before the t of the last call (D clamped to [I, I + RL_MAX_EXTRA])
bool RL_Sample(rl_ring_t *r, int cltime, int I, rl_out_t *out);

// snapshot interval: on the exact timeline the largest Com_FrameTime step, ceil(1000 / rate); else legacy
int RL_Interval(bool exact, int rate, int legacy);

// path conditions: cl_remote_lerp 1, not a demo, frametime.div > 1, cl_nolerp 0, extra > 0
typedef struct {
    bool enabled, demo;
    int div, nolerp;
} rl_cond_t;
// entity entnum (1..maxclients, not the POV clientnum + 1) with its ring's extra
bool RL_EntityOn(const rl_cond_t *c, int entnum, int maxclients, int frame_clientnum, float extra);
// view of the chased player: no prediction and frame.clientNum != own clientNum
bool RL_CameraOn(const rl_cond_t *c, bool predicted, int frame_clientnum, int clientnum, float extra);

// all rings of the client: one per player, one for the chase camera
typedef struct {
    rl_ring_t player[RL_PLAYERS];
    rl_ring_t chase;
    int pov;                    // frame.clientNum of the last frame, -1 before the first
} rl_set_t;
void RL_ResetAll(rl_set_t *set);
// POV clientNum of the new frame: on a change the chase ring and the rings of the old and new POV restart
void RL_SetPov(rl_set_t *set, int clientnum);
