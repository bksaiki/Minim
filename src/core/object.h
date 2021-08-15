#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include <stdarg.h>
#include "../common/common.h"
#include "../common/buffer.h"

#define PTR(x, offset)  (((intptr_t) (x)) + offset)
#define VOID_PTR(x)     ((void*)((intptr_t) (x)))

// Forward declaration
typedef struct MinimEnv MinimEnv;

typedef struct MinimObject
{
    uint8_t type;           // base object has only one member
} MinimObject;

typedef MinimObject *(*MinimBuiltin)(MinimEnv *, size_t, MinimObject **);
typedef bool (*MinimPred)(MinimObject *);

typedef enum MinimObjectType
{
    MINIM_OBJ_EXACT,
    MINIM_OBJ_INEXACT,
    MINIM_OBJ_SYM,
    MINIM_OBJ_STRING,
    MINIM_OBJ_PAIR,
    MINIM_OBJ_ERR,
    MINIM_OBJ_FUNC,
    MINIM_OBJ_CLOSURE,
    MINIM_OBJ_SYNTAX,
    MINIM_OBJ_TAIL_CALL,
    MINIM_OBJ_TRANSFORM,
    MINIM_OBJ_AST,
    MINIM_OBJ_SEQ,
    MINIM_OBJ_HASH,
    MINIM_OBJ_VECTOR,
    MINIM_OBJ_PROMISE,
    MINIM_OBJ_VALUES,
    MINIM_OBJ_CHAR,
    MINIM_OBJ_EXIT
} MinimObjectType;

#define minim_exactnum_size         (2 * sizeof(void*))
#define minim_inexactnum_size       (2 * sizeof(void*))
#define minim_symbol_size           (2 * sizeof(void*))
#define minim_string_size           (2 * sizeof(void*))
#define minim_cons_size             (3 * sizeof(void*))
#define minim_vector_size           (3 * sizeof(void*))
#define minim_hash_table_size       (2 * sizeof(void*))
#define minim_promise_size          (3 * sizeof(void*))
#define minim_builtin_size          (2 * sizeof(void*))
#define minim_syntax_size           (2 * sizeof(void*))
#define minim_closure_size          (2 * sizeof(void*))
#define minim_tail_call_size        (2 * sizeof(void*))
#define minim_transform_size        (3 * sizeof(void*))
#define minim_ast_size              (2 * sizeof(void*))
#define minim_sequence_size         (2 * sizeof(void*))
#define minim_values_size           (3 * sizeof(void*))
#define minim_error_size            (2 * sizeof(void*))
#define minim_char_size             8
#define minim_exit_size             1

// Special objects

#define minim_void      ((MinimObject*) 0x8)
#define minim_true      ((MinimObject*) 0x10)
#define minim_false     ((MinimObject*) 0x18)
#define minim_null      ((MinimObject*) 0x20)

// Predicates 

#define MINIM_OBJ_SAME_TYPE(obj, t)     ((((uintptr_t) obj) & ~0xFF) && ((obj)->type == t))
#define MINIM_OBJ_TYPE_EQP(a, b)        ((((uintptr_t) a) & ~0xFF) && (((uintptr_t) b) & ~0xFF)&& ((a)->type == (b)->type))

#define minim_voidp(x)              ((x) == minim_void)
#define minim_truep(x)              ((x) == minim_true)
#define minim_falsep(x)             ((x) == minim_false)
#define minim_booleanp(x)           (minim_truep(x) || minim_falsep(x))
#define minim_nullp(x)              ((x) == minim_null)

#define MINIM_OBJ_EXACTP(obj)       MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_EXACT)
#define MINIM_OBJ_INEXACTP(obj)     MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_INEXACT)
#define MINIM_OBJ_SYMBOLP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_SYM)
#define MINIM_OBJ_STRINGP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_STRING)
#define MINIM_OBJ_PAIRP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_PAIR)
#define MINIM_OBJ_ERRORP(obj)       MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_ERR)
#define MINIM_OBJ_BUILTINP(obj)     MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_FUNC)
#define MINIM_OBJ_CLOSUREP(obj)     MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_CLOSURE)
#define MINIM_OBJ_SYNTAXP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_SYNTAX)
#define MINIM_OBJ_TAIL_CALLP(obj)   MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_TAIL_CALL)
#define MINIM_OBJ_TRANSFORMP(obj)   MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_TRANSFORM)
#define MINIM_OBJ_ASTP(obj)         MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_AST)
#define MINIM_OBJ_SEQP(obj)         MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_SEQ)
#define MINIM_OBJ_HASHP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_HASH)
#define MINIM_OBJ_VECTORP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_VECTOR)
#define MINIM_OBJ_PROMISEP(obj)     MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_PROMISE)
#define MINIM_OBJ_VALUESP(obj)      MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_VALUES)
#define MINIM_OBJ_CHARP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_CHAR)
#define MINIM_OBJ_EXITP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_EXIT)

#define MINIM_OBJ_NUMBERP(obj)      (MINIM_OBJ_EXACTP(obj) || MINIM_OBJ_INEXACTP(obj))
#define MINIM_OBJ_FUNCP(obj)        (MINIM_OBJ_BUILTINP(obj) || MINIM_OBJ_CLOSUREP(obj))
#define MINIM_OBJ_THROWNP(obj)      (MINIM_OBJ_ERRORP(obj) || MINIM_OBJ_EXITP(obj))

// Accessors 

#define MINIM_EXACTNUM(obj)         (*((mpq_ptr*) VOID_PTR(PTR(obj, 8))))
#define MINIM_INEXACTNUM(obj)       (*((double*) VOID_PTR(PTR(obj, 8))))
#define MINIM_SYMBOL(obj)           (*((char**) VOID_PTR(PTR(obj, 8))))
#define MINIM_STRING(obj)           (*((char**) VOID_PTR(PTR(obj, 8))))
#define MINIM_CAR(obj)              (*((MinimObject**) VOID_PTR(PTR(obj, 8))))
#define MINIM_CDR(obj)              (*((MinimObject**) VOID_PTR(PTR(obj, 16))))
#define MINIM_VECTOR(obj)           (*((MinimObject***) VOID_PTR(PTR(obj, 8))))
#define MINIM_VECTOR_REF(obj, i)    ((*((MinimObject***) VOID_PTR(PTR(obj, 8))))[i])
#define MINIM_VECTOR_LEN(obj)       (*((size_t*) VOID_PTR(PTR(obj, 16))))
#define MINIM_HASH_TABLE(obj)       (*((MinimHash**) VOID_PTR(PTR(obj, 8))))
#define MINIM_PROMISE_VAL(obj)      (*((MinimObject**) VOID_PTR(PTR(obj, 8))))
#define MINIM_PROMISE_ENV(obj)      (*((MinimEnv**) VOID_PTR(PTR(obj, 16))))
#define MINIM_PROMISE_STATE(obj)    (*((uint8_t*) VOID_PTR(PTR(obj, 1))))
#define MINIM_BUILTIN(obj)          (*((MinimBuiltin*) VOID_PTR(PTR(obj, 8))))
#define MINIM_SYNTAX(obj)           (*((MinimBuiltin*) VOID_PTR(PTR(obj, 8))))
#define MINIM_CLOSURE(obj)          (*((MinimLambda**) VOID_PTR(PTR(obj, 8))))
#define MINIM_TAIL_CALL(obj)        (*((MinimTailCall**)  VOID_PTR(PTR(obj, 8))))
#define MINIM_TRANSFORM_NAME(obj)   (*((char**) VOID_PTR(PTR(obj, 8))))
#define MINIM_TRANSFORM_PROC(obj)   (*((MinimLambda**) VOID_PTR(PTR(obj, 16))))
#define MINIM_AST(obj)              (*((SyntaxNode**) VOID_PTR(PTR(obj, 8))))
#define MINIM_SEQUENCE(obj)         (*((MinimSeq**) VOID_PTR(PTR(obj, 8))))
#define MINIM_VALUES(obj)           (*((MinimObject***) VOID_PTR(PTR(obj, 8))))
#define MINIM_VALUES_REF(obj, i)    ((*((MinimObject***) VOID_PTR(PTR(obj, 8))))[i])
#define MINIM_VALUES_SIZE(obj)      (*((size_t*) VOID_PTR(PTR(obj, 16))))
#define MINIM_ERROR(obj)            (*((MinimError**) VOID_PTR(PTR(obj, 8))))
#define MINIM_CHAR(obj)             (*((unsigned int*) VOID_PTR(PTR(obj, 4))))
#define MINIM_EXIT_VAL(obj)         (*((long*) VOID_PTR(PTR(obj, 8))))

// Compound accessors

#define MINIM_CAAR(obj)             MINIM_CAR(MINIM_CAR(obj))
#define MINIM_CADR(obj)             MINIM_CAR(MINIM_CDR(obj))
#define MINIM_CDAR(obj)             MINIM_CDR(MINIM_CAR(obj))
#define MINIM_CDDR(obj)             MINIM_CDR(MINIM_CDR(obj))

// Setters

#define MINIM_PROMISE_SET_STATE(obj, s)     (*((uint8_t*) VOID_PTR(PTR(obj, 1))) = (s))

//  Initialization

MinimObject *minim_exactnum(void *num);
MinimObject *minim_inexactnum(double num);
MinimObject *minim_symbol(char *sym);
MinimObject *minim_string(char *str);
MinimObject *minim_cons(void *car, void *cdr);
MinimObject *minim_vector(size_t len, void *arr);
MinimObject *minim_hash_table(void *ht);
MinimObject *minim_promise(void *val, void *env);
MinimObject *minim_builtin(void *func);
MinimObject *minim_syntax(void *func);
MinimObject *minim_closure(void *closure);
MinimObject *minim_tail_call(void *tc);
MinimObject *minim_transform(char *name, void *closure);
MinimObject *minim_ast(void *ast);
MinimObject *minim_sequence(void *seq);
MinimObject *minim_values(size_t len, void *arr);
MinimObject *minim_err(void *err);
MinimObject *minim_char(unsigned int ch);
MinimObject *minim_exit(long code);

//  Equivalence

bool minim_equalp(MinimObject *a, MinimObject *b);
#define minim_eqp(a, b)     ((a) == (b))

//  Miscellaneous

Buffer* minim_obj_to_bytes(MinimObject *obj);

#define coerce_into_bool(x)    ((x) != minim_false)
#define to_bool(x)             ((x) ? minim_true : minim_false)

#endif
