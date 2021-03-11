#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include <stdarg.h>
#include "../common/common.h"
#include "../common/buffer.h"

#define MINIM_OBJ_OWNER             0x1

struct MinimEnv;
typedef struct MinimEnv MinimEnv;

typedef struct MinimObject
{
    union
    {
        struct { char *str; void *none; } str;
        struct { void *p1, *p2; } ptrs;
        struct { long int i1, i2; } ints;
        struct { struct MinimObject *car, *cdr; } pair;
        struct { struct MinimObject **arr; size_t len; } vec;
    } u;
    uint8_t type;
    uint8_t flags;
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

// Optimized move:
//  makes 'dest' a owned copy of 'src'
//  moves 'src' to 'dest' if src is an owner
#define OPT_MOVE(dest, src)             \
{                                       \
    if (MINIM_OBJ_OWNERP(src))          \
    { dest = src; src = NULL; }         \
    else                                \
    { dest = fresh_minim_object(src); } \
}

// Optimized move (ref variant)
//  makes 'dest' a copy of 'src'
//  moves 'src' to 'dest' if src is an owner
//  preserves ownership
#define OPT_MOVE_REF(dest, src)         \
{                                       \
    if (MINIM_OBJ_OWNERP(src))          \
    { dest = src; src = NULL; }         \
    else                                \
    { ref_minim_object(&dest, src); }   \
}

// Optimized move (ref2 variant)
//  makes 'dest' a copy of 'src'
//  moves 'src' to 'dest' if src is an owner
//  preserves ownership based on 'obj'
#define OPT_MOVE_REF2(dest, src, obj)   \
{                                       \
    if (MINIM_OBJ_OWNERP(obj))          \
    { dest = src; src = NULL; }         \
    else                                \
    { ref_minim_object(&dest, src); }   \
}

// Deletes all arguments that are owners
#define RELEASE_OWNED_ARGS(args, argc)      \
{                                           \
    for (size_t i = 0; i < argc; ++i)          \
    {                                       \
        if (MINIM_OBJ_OWNERP((args)[i]))      \
            (args)[i] = NULL;                 \
    }                                       \
}

// Deletes if object is a reference
#define RELEASE_IF_REF(obj)     \
{                               \
    if (!MINIM_OBJ_OWNERP(obj)) \
        free_minim_object(obj); \
}


#define MINIM_OBJ_OWNERP(obj)       (obj->flags & MINIM_OBJ_OWNER)
#define MINIM_OBJ_SET_OWNER(obj)    (obj->flags |= MINIM_OBJ_OWNER)

#define MINIM_CAR(obj)      (obj->u.pair.car)
#define MINIM_CDR(obj)      (obj->u.pair.cdr)

//  Initialization / Destruction

void init_minim_object(MinimObject **pobj, MinimObjectType type, ...);
void initv_minim_object(MinimObject **pobj, MinimObjectType type, va_list vargs);

void copy_minim_object(MinimObject **pobj, MinimObject *src);
void ref_minim_object(MinimObject **pobj, MinimObject *src);

void free_minim_object(MinimObject *obj);
void free_minim_objects(MinimObject **objs, size_t count);

//  Specialized constructors

void minim_error(MinimObject **pobj, const char* format, ...);

//  Equivalence

bool minim_equalp(MinimObject *a, MinimObject *b);

//  Miscellaneous

// Returns 'src' or an owned version of 'src'
MinimObject *fresh_minim_object(MinimObject *src);

// Returns an owned version of 'src'
MinimObject *copy2_minim_object(MinimObject *src);

Buffer* minim_obj_to_bytes(MinimObject *obj);

#endif