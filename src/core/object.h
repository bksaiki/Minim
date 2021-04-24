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
        struct { double f1, f2; } fls;
        struct { struct MinimObject *car, *cdr; } pair;
        struct { struct MinimObject **arr; size_t len; } vec;
    } u;
    uint8_t type;
    uint8_t flags;
} MinimObject;

typedef enum MinimObjectType
{
    MINIM_OBJ_VOID,
    MINIM_OBJ_EXIT,
    MINIM_OBJ_BOOL,
    MINIM_OBJ_EXACT,
    MINIM_OBJ_INEXACT,
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
typedef bool (*MinimPred)(MinimObject *);

// Setters

#define MINIM_OBJ_SET_OWNER(obj)        (obj->flags |= MINIM_OBJ_OWNER)
#define MINIM_OBJ_SET_REF(obj)          (obj->flags &= ~MINIM_OBJ_OWNER)

// Predicates 

#define MINIM_OBJ_OWNERP(obj)           (obj->flags & MINIM_OBJ_OWNER)
#define MINIM_OBJ_SAME_TYPE(obj, t)     (obj->type == t)

#define MINIM_OBJ_VOIDP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_VOID)
#define MINIM_OBJ_EXITP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_EXIT)
#define MINIM_OBJ_BOOLP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_BOOL)
#define MINIM_OBJ_EXACTP(obj)       MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_EXACT)
#define MINIM_OBJ_INEXACTP(obj)     MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_INEXACT)
#define MINIM_OBJ_SYMBOLP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_SYM)
#define MINIM_OBJ_STRINGP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_STRING)
#define MINIM_OBJ_PAIRP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_PAIR)
#define MINIM_OBJ_ERRORP(obj)       MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_ERR)
#define MINIM_OBJ_BUILTINP(obj)     MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_FUNC)
#define MINIM_OBJ_CLOSUREP(obj)     MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_CLOSURE)
#define MINIM_OBJ_SYNTAXP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_SYNTAX)
#define MINIM_OBJ_ASTP(obj)         MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_AST)
#define MINIM_OBJ_SEQP(obj)         MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_SEQ)
#define MINIM_OBJ_HASHP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_HASH)
#define MINIM_OBJ_VECTORP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_VECTOR)

#define MINIM_OBJ_NUMBERP(obj)      (MINIM_OBJ_EXACTP(obj) || MINIM_OBJ_INEXACTP(obj))
#define MINIM_OBJ_FUNCP(obj)        (MINIM_OBJ_BUILTINP(obj) || MINIM_OBJ_CLOSUREP(obj))
#define MINIM_OBJ_THROWNP(obj)      (MINIM_OBJ_ERRORP(obj) || MINIM_OBJ_EXITP(obj))

// Accessors 

#define MINIM_CAR(obj)      (obj->u.pair.car)
#define MINIM_CDR(obj)      (obj->u.pair.cdr)

#define MINIM_CAAR(obj)     MINIM_CAR(MINIM_CAR(obj))
#define MINIM_CADR(obj)     MINIM_CAR(MINIM_CDR(obj))
#define MINIM_CDAR(obj)     MINIM_CDR(MINIM_CAR(obj))
#define MINIM_CDDR(obj)     MINIM_CDR(MINIM_CDR(obj))

#define MINIM_EXACT(obj)    ((mpq_ptr) obj->u.ptrs.p1)
#define MINIM_INEXACT(obj)  (obj->u.fls.f1)


// Optimized move:
//  makes 'dest' a owned copy of 'src'
//  moves 'src' to 'dest' if src is an owner
#define OPT_MOVE(dest, src)             \
{                                       \
    if (MINIM_OBJ_OWNERP(src))          \
    { dest = src; src = NULL; }         \
    else                                \
    { dest = copy2_minim_object(src); } \
}

// Optimized move 
//  makes 'dest' a owned copy of 'src'
//  moves 'src' to 'dest' if 'obj' is an owner
#define OPT_MOVE2(dest, src, obj)       \
{                                       \
    if (MINIM_OBJ_OWNERP(obj))          \
    { dest = src; src = NULL; }         \
    else                                \
    { dest = copy2_minim_object(src); } \
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
//  moves 'src' to 'dest' if obj is an owner
//  preserves ownership based on 'obj'
#define OPT_MOVE_REF2(dest, src, obj)   \
{                                       \
    if (MINIM_OBJ_OWNERP(obj))          \
    { dest = src; src = NULL; }         \
    else                                \
    { ref_minim_object(&dest, src); }   \
}

// Deletes all arguments that are owners
#define RELEASE_OWNED_ARGS(args, argc)                   \
{                                                        \
    for (size_t i = 0; i < argc; ++i)                    \
    {                                                    \
        if ((args)[i] && MINIM_OBJ_OWNERP((args)[i]))    \
            (args)[i] = NULL;                   \
    }                                           \
}

// Deletes if object is a owner
#define RELEASE_IF_OWNER(obj)       \
{                                   \
    if (MINIM_OBJ_OWNERP(obj))      \
        free_minim_object(obj);     \
}

// Deletes if object is a reference
#define RELEASE_IF_REF(obj)         \
{                                   \
    if (!MINIM_OBJ_OWNERP(obj))     \
        free_minim_object(obj);     \
}


//  Initialization / Destruction

void init_minim_object(MinimObject **pobj, MinimObjectType type, ...);
void initv_minim_object(MinimObject **pobj, MinimObjectType type, va_list vargs);

void copy_minim_object(MinimObject **pobj, MinimObject *src);
void ref_minim_object(MinimObject **pobj, MinimObject *src);

void free_minim_object(MinimObject *obj);
void free_minim_objects(MinimObject **objs, size_t count);

//  Equivalence

bool minim_equalp(MinimObject *a, MinimObject *b);

//  Miscellaneous

// Returns 'src' or an owned version of 'src'
MinimObject *fresh_minim_object(MinimObject *src);

// Returns an owned version of 'src'
MinimObject *copy2_minim_object(MinimObject *src);

Buffer* minim_obj_to_bytes(MinimObject *obj);

bool coerce_into_bool(MinimObject *obj);

#endif
