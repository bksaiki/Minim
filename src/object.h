#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include "base.h"
#include "common/buffer.h"

#define MINIM_OBJ_OWNER             0x1
#define MINIM_OBJ_OWNERP(obj)       (obj->flags & MINIM_OBJ_OWNER)
#define MINIM_OBJ_SET_OWNER(obj)    (obj->flags |= MINIM_OBJ_OWNER)

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
    MINIM_OBJ_ITER,
    MINIM_OBJ_SEQ,
    MINIM_OBJ_HASH
} MinimObjectType;

typedef MinimObject *(*MinimBuiltin)(MinimEnv *, int, MinimObject **);

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
Buffer* minim_obj_to_bytes(MinimObject *obj);

#endif