#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include "base.h"
#include "common/buffer.h"

struct MinimEnv;
typedef struct MinimEnv MinimEnv;

typedef struct MinimObject
{
    int type;
    void* data;
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

//
//  Initialization / Destruction
//

// Constructs a single minim object based on the type.
void init_minim_object(MinimObject **pobj, MinimObjectType type, ...);

// Returns a copy of the given object.
void copy_minim_object(MinimObject **pobj, MinimObject *src);

// Deletes a MinimObject.
void free_minim_object(MinimObject *obj);

// Deletes an array of MinimObjects.
void free_minim_objects(int count, MinimObject **objs);

//
//  Specialized constructors
//

void minim_error(MinimObject **pobj, const char* format, ...);

//
//  Equivalence
//

bool minim_equalp(MinimObject *a, MinimObject *b);

//
//  Miscellaneous
//  

Buffer* minim_obj_to_bytes(MinimObject *obj);

#endif