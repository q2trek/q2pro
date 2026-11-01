/*
Copyright (C) 2003-2012 Andrey Nazarov

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#pragma once

#if USE_CLIENT
extern const char com_env_suf[6][3];
#endif

typedef enum {
    COLOR_BLACK,
    COLOR_RED,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_BLUE,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_WHITE,

    COLOR_ALT,
    COLOR_NONE,

    COLOR_COUNT
} color_index_t;

extern const char *const colorNames[COLOR_COUNT];

bool Com_WildCmpEx(const char *filter, const char *string, int term, bool ignorecase);
#define Com_WildCmp(filter, string)  Com_WildCmpEx(filter, string, 0, false)

#if USE_CLIENT || USE_MVD_CLIENT
bool Com_ParseTimespec(const char *s, int *frames);
#endif

void Com_PlayerToEntityState(const player_state_t *ps, entity_state_t *es);

// only begin attenuating sound volumes when outside the FULLVOLUME range
#define SOUND_FULLVOLUME            80
#define SOUND_LOOPATTENUATE         (ATTN_STATIC * 0.001f)
#define SOUND_LOOPATTENUATE_MULT    0.0006f

float Com_GetEntityLoopDistMult(float attenuation);

bool Com_ParseMapName(char *out, const char *in, size_t size);

unsigned Com_HashString(const char *s, unsigned size);
unsigned Com_HashStringLen(const char *s, size_t len, unsigned size);

size_t Com_FormatLocalTime(char *buffer, size_t size, const char *fmt);

size_t Com_FormatTime(char *buffer, size_t size, time_t t);
size_t Com_FormatTimeLong(char *buffer, size_t size, time_t t);
size_t Com_TimeDiff(char *buffer, size_t size, time_t *p, time_t now);
size_t Com_TimeDiffLong(char *buffer, size_t size, time_t *p, time_t now);

size_t Com_FormatSize(char *dest, size_t destsize, int64_t bytes);
size_t Com_FormatSizeLong(char *dest, size_t destsize, int64_t bytes);

void Com_PageInMemory(void *buffer, size_t size);

color_index_t Com_ParseColor(const char *s);

#if USE_REF
unsigned Com_ParseExtensionString(const char *s, const char *const extnames[]);
#endif

extern const char com_hexchars[16];

size_t Com_EscapeString(char *dst, const char *src, size_t size);

char *Com_MakePrintable(const char *s);

#if USE_CLIENT
uint32_t Com_SlowRand(void);
#define Com_SlowFrand()  ((int32_t)Com_SlowRand() * 0x1p-32f + 0.5f)
#define Com_SlowCrand()  ((int32_t)Com_SlowRand() * 0x1p-31f)
#endif

#if USE_CLIENT
#define UNICODE_UNKNOWN     0xFFFD
#define UNICODE_MAX         0x10FFFF
uint32_t UTF8_ReadCodePoint(const char **src);
size_t UTF8_TranslitBuffer(char *dst, const char *src, size_t size);
char *UTF8_TranslitString(const char *src);
#endif

// Bitmap chunks (for sparse bitmaps)
#define BC_BITS         (sizeof(size_t) * CHAR_BIT)
#define BC_COUNT(n)     (((n) + BC_BITS - 1) / BC_BITS)

// Some mods actually exploit CS_STATUSBAR to take space up to CS_AIRACCEL
static inline size_t Com_ConfigstringSize(const cs_remap_t *csr, int cs)
{
    if (cs >= CS_STATUSBAR && cs < csr->airaccel)
        return MAX_QPATH * (csr->airaccel - cs);

    if (cs >= csr->general && cs < csr->end)
        return MAX_QPATH * (csr->end - cs);

    return MAX_QPATH;
}

/*
World clock contract (rational frame time).

The world runs at an integer rate R Hz with a constant step of 1/R s; the
game sees that step, never a rounded one. Times derived from frame numbers
are whole milliseconds relative to an epoch anchor (map/serverdata/rebase):

  T(n) = floor(n * 1000 / R)   time of frame n (client timeline)
  D(n) = ceil(n * 1000 / R)    deadline of the n-th step (scheduling)
  N(t) = ceil(t * R / 1000)    smallest n with T(n) >= t (inverse of T;
                               floor(t * R / 1000) would map T(1) = 16 at
                               60 Hz back to frame 0)

Each projection is off by less than 1 ms and a whole second is exact, so
nothing drifts; constant 16 ms / 33 ms periods are not used for 60/30 Hz.
The server scheduler keeps a credit of elapsed_ms * R units and spends
FRAME_CREDIT units per step. Products are 64-bit.

Real-time clocks keep their own milliseconds and are advanced once per
external frame, not per world step: svs.realtime, com_eventTime,
com_localTime, ping, timeouts, rate limits, usercmd msec and the command
allowance (1.6 s). Demo/MVD keep the 10 Hz base (BASE_FRAMETIME).
*/
#define FRAME_CREDIT    1000

static inline int64_t Com_FloorDiv(int64_t a, int64_t b)
{
    return a / b - (a % b < 0);
}

static inline int64_t Com_FrameTime(int64_t n, int rate)
{
    return Com_FloorDiv(n * 1000, rate);
}

static inline int64_t Com_FrameForTime(int64_t t, int rate)
{
    return -Com_FloorDiv(-t * rate, 1000);
}

#if USE_FPS
typedef struct {
    int         time;      // variable server frame time
    int         div;       // BASE_FRAMETIME/frametime
} frametime_t;

// Compute frametime based on requested frame rate
static inline frametime_t Com_ComputeFrametime(int rate)
{
    int framediv = Q_clip(rate / BASE_FRAMERATE, 1, MAX_FRAMEDIV);
    return (frametime_t){ .time = BASE_FRAMETIME / framediv, .div = framediv };
}
#endif // USE_FPS
