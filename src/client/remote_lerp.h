/* Remote player smoothing without engine dependencies:
   per-player history of snapshots, delay D from the observed interval between position changes,
   presentation clock P follows the target cl.time - S, with S = min(extra + I, I + RL_MAX_EXTRA): one snapshot
   interval I of reserve plus the adjustable extra. P uses real elapsed time while cl.time is clamped.
   Snapshot labels and P are ms of the client timeline (cl.servertime labels). */

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
#define RL_CLOCK_SNAP   251     // real-time gap: resume at max(P, target) instead of stepping
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
    rl_snap_t moved, turned;    // last change point of position / angles: the change interval (add_gap)
    rl_snap_t anchor_moved, anchor_turned;  // last change point dropped from the ring: k0 while the ring has none <= t
    bool has_moved, has_turned, has_anchor_moved, has_anchor_turned;
    int gap[RL_GAPS], gap_t[RL_GAPS], ngaps, gap_head;
    int D, since;               // accepted delay (0: none yet); since: the percentile differs from D since then
    float extra, shown;         // adjustable delay beyond I; monotone lower bound of the target, not P
    bool has_shown;
    float P;                   // one presentation time for origin, angles and animation
    bool has_P;
    unsigned last_real;        // untrimmed real time of this history's last nonempty sample
} rl_ring_t;

typedef struct {
    float t;
    float org[3], ang[3];
    int frame, oldframe;
    float backlerp;
} rl_out_t;

void RL_Reset(rl_ring_t *r);
// a snapshot; jump: a move along any axis longer than this restarts the history (512 entity, 256 chase camera);
// a change point dropped from the ring (older than RL_WINDOW, or a full ring) becomes anchor_moved / anchor_turned (D1a)
void RL_Push(rl_ring_t *r, const rl_snap_t *s, float jump);
// once per drawn frame: dt = growth of cl.time since the last frame (0 while paused or clamped),
// I = snapshot interval (RL_Interval); moves extra toward D - I by at most dt / 10, returns extra
float RL_Advance(rl_ring_t *r, int dt, int I);
// position and angles at t with delay D, each between its own change points; animation: frame of the last
// record <= t, oldframe -> frame over RL_ANIM from the record before that frame's run; false: empty ring
bool RL_At(const rl_ring_t *r, float t, float D, rl_out_t *out);
// P follows the monotone target cltime - S with a real-time step, capped by the newest snapshot (no lower cap).
// Starts at target; paused clocks stand, gaps >= RL_CLOCK_SNAP use max(P, target). Empty rings preserve state.
// realtime is an unsigned, untrimmed counter; the window stays S + I, at most 2 I + RL_MAX_EXTRA.
bool RL_Sample(rl_ring_t *r, int cltime, unsigned realtime, bool paused, int I, rl_out_t *out);

// snapshot interval: on the exact timeline the largest Com_FrameTime step, ceil(1000 / rate); else legacy
int RL_Interval(bool exact, int rate, int legacy);

// path conditions (design 2): cl_remote_lerp 1, not a demo, frametime.div > 1, cl_nolerp 0; the same for an entity
// and the chase camera, at any extra (the reserve applies to an even sender too)
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
