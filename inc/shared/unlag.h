#ifndef SHARED_UNLAG_H
#define SHARED_UNLAG_H

#define UNLAG_API_V1 "UNLAG_API_V1"

typedef enum {
    UNLAG_OUTSIDE,
    UNLAG_REPEAT,
    UNLAG_NO_ESTIMATE,
    UNLAG_ESTIMATE,
} unlag_kind_t;

typedef struct {
    unlag_kind_t kind;
    int frame;                  // world frame, valid only with UNLAG_ESTIMATE
} unlag_command_t;

typedef struct {
    int (*WorldFrame)(void);
    int (*FrameRate)(void);
    unlag_command_t (*Command)(const edict_t *ent);
} unlag_api_v1_t;

#endif
