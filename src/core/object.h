#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include "../common/common.h"
#include "../common/buffer.h"

#define MINIM_OBJ_OWNER             0x1
#define MINIM_OBJ_OWNERP(obj)       (obj->flags & MINIM_OBJ_OWNER)
#define MINIM_OBJ_SET_OWNER(obj)    (obj->flags |= MINIM_OBJ_OWNER)

#define OPT_MOVE(dest, src)             \
{                                       \
    if (MINIM_OBJ_OWNERP(src))          \
    { dest = src; src = NULL; }         \
    else                                \
    { dest = fresh_minim_object(src); } \
}

#define OPT_MOVE_REF(dest, src)         \
{                                       \
    if (MINIM_OBJ_OWNERP(src))          \
    { dest = src; src = NULL; }         \
    else                                \
    { ref_minim_object(&dest, src); }   \
}

#define OPT_MOVE_REF2(dest, src, obj)   \
{                                       \
    if (MINIM_OBJ_OWNERP(obj))          \
    { dest = src; src = NULL; }         \
    else                                \
    { ref_minim_object(&dest, src); }   \
}

#define RELEASE_OWNED_ARGS(args, argc)      \
{                                           \
    for (size_t i = 0; i < argc; ++i)          \
    {                                       \
        if (MINIM_OBJ_OWNERP((args)[i]))      \
            (args)[i] = NULL;                 \
    }                                       \
}

#define RELEASE_IF_REF(obj)     \
{                               \
    if (!MINIM_OBJ_OWNERP(obj)) \
        free_minim_object(obj); \
}

struct MinimEnv;
typedef struct MinimEnv MinimEnv;

typedef struct MinimObject
{
    union { void* data; intmax_t si; };
    uint16_t type;
    uint16_t flags;
} MinimObject;

typedef enum MinimObjectType
{
    MINIM_OBJ_VOID,
    MINIM_OBJ_BOOL,
    MINIM_OBJ_NUM,
    MINIM_OBJ_SYM,
    MINIM_OBJ_STRING,
    MINIM_OBJ_PAIR,
    MINIM_OBJ_ERR,
    MINIM_OBJ_FUNC,
    MINIM_OBJ_CLOSURE,
    MINIM_OBJ_SYNTAX,
    MINIM_OBJ_AST,
    MINIM_OBJ_SEQ,
    MINIM_OBJ_HASH,
    MINIM_OBJ_VECTOR
} MinimObjectType;

typedef MinimObject *(*MinimBuiltin)(MinimEnv *, MinimObject **, size_t);

//  Initialization / Destruction

void init_minim_object(MinimObject **pobj, MinimObjectType type, ...);
void copy_minim_object(MinimObject **pobj, MinimObject *src);
void ref_minim_object(MinimObject **pobj, MinimObject *src);
void free_minim_object(MinimObject *obj);
void free_minim_objects(MinimObject **objs, size_t count);

//  Specialized constructors

void minim_error(MinimObject **pobj, const char* format, ...);

//  Equivalence

bool minim_equalp(MinimObject *a, MinimObject *b);

//  Miscellaneous

MinimObject *fresh_minim_object(MinimObject *src);
MinimObject *copy2_minim_object(MinimObject *src);
Buffer* minim_obj_to_bytes(MinimObject *obj);

#endif