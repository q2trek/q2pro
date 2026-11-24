#ifndef SHARED_FRAMEDIV_H
#define SHARED_FRAMEDIV_H

#define OPENTDM60_FRAMEDIV_V1 "OPENTDM60_FRAMEDIV_V1"

typedef struct {
    // World frames per snapshot, or zero if the edict is not a client in game.
    int (*ClientFrameDiv)(edict_t *ent);
} opentdm60_framediv_v1_t;

#endif
