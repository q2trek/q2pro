/* Remote player smoothing without engine dependencies. */

#include "remote_lerp.h"

#include <math.h>
#include <string.h>

void RL_Reset(rl_ring_t *r)
{
    memset(r, 0, sizeof(*r));
}

// i = 0 is the newest record
static const rl_rec_t *rec(const rl_ring_t *r, int i)
{
    return &r->rec[(r->head - i + RL_RING) % RL_RING];
}

static bool same3(const float *a, const float *b)
{
    return a[0] == b[0] && a[1] == b[1] && a[2] == b[2];
}

static int clamp_delay(int D, int I)
{
    return D < I ? I : D > I + RL_MAX_EXTRA ? I + RL_MAX_EXTRA : D;
}

// 90th percentile of the change intervals
static int percentile(const rl_ring_t *r)
{
    int v[RL_GAPS], n = r->ngaps;

    for (int i = 0; i < n; i++) {
        int x = r->gap[(r->gap_head - i + RL_GAPS) % RL_GAPS], j = i;
        for (; j > 0 && v[j - 1] > x; j--)
            v[j] = v[j - 1];
        v[j] = x;
    }
    return v[(9 * n + 9) / 10 - 1];
}

static void add_gap(rl_ring_t *r, int gap, int t)
{
    while (r->ngaps && r->gap_t[(r->gap_head - r->ngaps + 1 + RL_GAPS) % RL_GAPS] < t - RL_GAP_WINDOW)
        r->ngaps--;
    if (gap <= RL_WINDOW) {     // longer: a stop, not the sender's interval
        r->gap_head = (r->gap_head + 1) % RL_GAPS;
        r->gap[r->gap_head] = gap;
        r->gap_t[r->gap_head] = t;
        if (r->ngaps < RL_GAPS)
            r->ngaps++;
    }
    if (!r->ngaps)
        return;

    // hysteresis: a new D once the percentile has differed from it for RL_HOLD
    int p = percentile(r);
    if (!r->D || (p != r->D && t - r->since >= RL_HOLD))
        r->D = p;
    if (p == r->D)
        r->since = t;
}

void RL_Push(rl_ring_t *r, const rl_snap_t *s, float jump)
{
    if (r->count) {
        const rl_snap_t *l = &rec(r, 0)->s;
        if (s->model != l->model || s->stat != l->stat || s->teleport || s->t <= l->t || s->t - l->t > RL_ABSENT
            || fabsf(s->org[0] - l->org[0]) > jump
            || fabsf(s->org[1] - l->org[1]) > jump
            || fabsf(s->org[2] - l->org[2]) > jump)
            RL_Reset(r);
    }

    rl_rec_t e = { .s = *s, .moved = true, .turned = true };
    if (r->count) {
        const rl_snap_t *l = &rec(r, 0)->s;
        e.moved = !same3(s->org, l->org);
        e.turned = !same3(s->ang, l->ang);
        if (e.moved || e.turned) {
            int last = r->moved.t > r->turned.t ? r->moved.t : r->turned.t;
            add_gap(r, s->t - last, s->t);
        }
    }
    if (e.moved)
        r->moved = *s, r->has_moved = true;
    if (e.turned)
        r->turned = *s, r->has_turned = true;

    r->head = (r->head + 1) % RL_RING;
    r->rec[r->head] = e;
    if (r->count < RL_RING)
        r->count++;
    while (r->count > 3 && rec(r, r->count - 1)->s.t < s->t - RL_WINDOW)
        r->count--;
}

float RL_Advance(rl_ring_t *r, int dt, int I)
{
    float target = clamp_delay(r->D, I) - I, step = 0.1f * dt, d = target - r->extra;

    // within one step: exactly the target, so a decayed extra is 0 and the path goes off
    r->extra = fabsf(d) <= step ? target : r->extra + (d > 0 ? step : -step);
    return r->extra;
}

// k0: the last change point <= t (the kept one once the window has passed it), k1: the first one > t
static void points(const rl_ring_t *r, float t, bool ang, const rl_snap_t **k0, const rl_snap_t **k1)
{
    *k0 = *k1 = NULL;
    for (int i = 0; i < r->count; i++) {
        const rl_rec_t *e = rec(r, i);
        if (!(ang ? e->turned : e->moved))
            continue;
        if (e->s.t > t) {
            *k1 = &e->s;
        } else {
            *k0 = &e->s;
            return;
        }
    }
    const rl_snap_t *kept = ang ? &r->turned : &r->moved;
    if ((ang ? r->has_turned : r->has_moved) && kept->t <= t)
        *k0 = kept;
}

// transition k0 -> k1 over [a, b] = [max(k0.t, k1.t - D), k1.t]; none after the last point
static float alpha(const rl_snap_t *k0, const rl_snap_t *k1, float t, float D)
{
    if (!k1)
        return 0;
    float a = fmaxf(k0->t, k1->t - D), b = k1->t;
    if (t <= a)
        return 0;
    float inv = 1.0f / (b - a), f = (t - a) * inv;
    return f > 1 ? 1 : f;
}

bool RL_At(const rl_ring_t *r, float t, float D, rl_out_t *o)
{
    const rl_snap_t *k0, *k1;

    if (!r->count)
        return false;
    o->t = t;

    points(r, t, false, &k0, &k1);
    if (!k0)
        k0 = k1 ? k1 : &rec(r, 0)->s, k1 = NULL;
    float f = alpha(k0, k1, t, D);
    for (int i = 0; i < 3; i++)
        o->org[i] = k1 ? k0->org[i] + f * (k1->org[i] - k0->org[i]) : k0->org[i];

    points(r, t, true, &k0, &k1);
    if (!k0)
        k0 = k1 ? k1 : &rec(r, 0)->s, k1 = NULL;
    f = alpha(k0, k1, t, D);
    for (int i = 0; i < 3; i++) {
        float d = k1 ? k1->ang[i] - k0->ang[i] : 0;
        if (d > 180)
            d -= 360;
        else if (d < -180)
            d += 360;
        o->ang[i] = k0->ang[i] + f * d;
    }

    // animation: the frame of the last record <= t, blended from the record before that frame's run
    int i = 0;
    while (i < r->count - 1 && rec(r, i)->s.t > t)
        i++;
    const rl_snap_t *s = &rec(r, i)->s;
    o->frame = o->oldframe = s->frame;
    o->backlerp = 0;
    while (i + 1 < r->count && rec(r, i + 1)->s.frame == s->frame)
        i++;
    if (i + 1 < r->count) {
        const rl_snap_t *p = &rec(r, i + 1)->s;
        float d = t - p->t;
        if (d <= RL_ANIM) {
            o->oldframe = p->frame;
            o->backlerp = d > 0 ? 1 - d / RL_ANIM : 1;
        }
    }
    return true;
}

bool RL_Sample(rl_ring_t *r, int cltime, int I, rl_out_t *o)
{
    float t = cltime - r->extra;

    if (r->has_shown && t < r->shown)
        t = r->shown;
    if (!RL_At(r, t, clamp_delay(r->D, I), o))
        return false;
    r->shown = t;
    r->has_shown = true;
    return true;
}

int RL_Interval(bool exact, int rate, int legacy)
{
    return exact && rate > 0 ? (1000 + rate - 1) / rate : legacy;
}

static bool path_on(const rl_cond_t *c, float extra)
{
    return c->enabled && !c->demo && c->div > 1 && !c->nolerp && extra > 0;
}

bool RL_EntityOn(const rl_cond_t *c, int entnum, int maxclients, int frame_clientnum, float extra)
{
    return path_on(c, extra) && entnum >= 1 && entnum <= maxclients && entnum != frame_clientnum + 1;
}

bool RL_CameraOn(const rl_cond_t *c, bool predicted, int frame_clientnum, int clientnum, float extra)
{
    return path_on(c, extra) && !predicted && frame_clientnum != clientnum;
}

void RL_ResetAll(rl_set_t *set)
{
    memset(set, 0, sizeof(*set));
    set->pov = -1;
}

void RL_SetPov(rl_set_t *set, int clientnum)
{
    if (clientnum == set->pov)
        return;
    if (set->pov >= 0 && set->pov < RL_PLAYERS)
        RL_Reset(&set->player[set->pov]);
    if (clientnum >= 0 && clientnum < RL_PLAYERS)
        RL_Reset(&set->player[clientnum]);
    RL_Reset(&set->chase);
    set->pov = clientnum;
}
