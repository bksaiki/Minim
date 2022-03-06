#ifndef _MINIM_H_
#define _MINIM_H_

// standard library headers
#include <limits.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// library headers
#include <gmp.h>

// garbage collector
#include "../gc/gc.h"

// common folder
#include "../common/buffer.h"
#include "../common/common.h"
#include "../common/path.h"

//
//  Forward declarations
//

typedef struct MinimLambda MinimLambda;
typedef struct MinimModule MinimModule;
typedef struct MinimModuleCache MinimModuleCache;
typedef struct InternTable InternTable;
typedef struct MinimSymbolTable MinimSymbolTable;

//
//  Structures
//

// universal object
typedef struct MinimObject
{
    uint8_t type;
} MinimObject;

// generic environment within scopes
typedef struct MinimEnv
{
    struct MinimEnv *parent, *caller;
    struct MinimModule *module;
    MinimSymbolTable *table;
    MinimLambda *callee;
    MinimObject *jmp;
    char *current_dir;
    uint8_t flags;
} MinimEnv;

// module
typedef struct MinimModule
{
    struct MinimModule *prev;
    struct MinimModule **imports;
    MinimObject **exprs;
    MinimObject *body;
    MinimEnv *env, *export;
    char *name;
    size_t exprc, importc;
} MinimModule;

// global table
typedef struct MinimGlobal
{
    // state
    MinimModuleCache *cache;            // cache of loaded modules
    MinimSymbolTable *builtins;         // primitive table
    InternTable *symbols;               // symbol table
    char *current_dir;                  // directory where Minim was started
    pid_t pid;                          // primary thread id

    // statistics
    size_t stat_exprs;
    size_t stat_procs;
    size_t stat_objs;

    // flags
    uint8_t flags;
} MinimGlobal;

// printing parameters
typedef struct PrintParams
{
    size_t maxlen;
    bool quote;
    bool display;
    bool syntax;
} PrintParams;

//
//  Objects  
//

#define PTR(x, offset)  (((intptr_t) (x)) + offset)
#define VOID_PTR(x)     ((void*)((intptr_t) (x)))
#define PTR_SIZE        (sizeof(void*))

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
    MINIM_OBJ_JUMP,
    MINIM_OBJ_PORT
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
#define minim_ast_size              (3 * sizeof(void*))
#define minim_sequence_size         (2 * sizeof(void*))
#define minim_values_size           (3 * sizeof(void*))
#define minim_error_size            (2 * sizeof(void*))
#define minim_char_size             (2 * sizeof(int))
#define minim_exit_size             (2 * sizeof(void*))
#define minim_jump_size             (3 * sizeof(void*))
#define minim_port_size             (7 * sizeof(void*))

// Special objects

#define minim_void      ((MinimObject*) 0x8)
#define minim_true      ((MinimObject*) 0x10)
#define minim_false     ((MinimObject*) 0x18)
#define minim_null      ((MinimObject*) 0x20)
#define minim_eof      ((MinimObject*) 0x28)

extern MinimObject *minim_error_handler;
extern MinimObject *minim_exit_handler;
extern MinimObject *minim_error_port;
extern MinimObject *minim_output_port;
extern MinimObject *minim_input_port;

// Predicates

#define MINIM_OBJ_SAME_TYPE(obj, t)     ((((uintptr_t) obj) & ~0xFF) && ((obj)->type == t))
#define MINIM_OBJ_TYPE_EQP(a, b)        ((((uintptr_t) a) & ~0xFF) && (((uintptr_t) b) & ~0xFF)&& ((a)->type == (b)->type))

#define minim_voidp(x)              ((x) == minim_void)
#define minim_truep(x)              ((x) == minim_true)
#define minim_falsep(x)             ((x) == minim_false)
#define minim_nullp(x)              ((x) == minim_null)
#define minim_eofp(x)               ((x) == minim_eof)

#define minim_booleanp(x)           (minim_truep(x) || minim_falsep(x))
#define minim_specialp(x)           (minim_voidp(x) ||      \
                                     minim_booleanp(x) ||   \
                                     minim_nullp(x) ||      \
                                     minim_eofp(x))

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
#define MINIM_OBJ_JUMPP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_JUMP)
#define MINIM_OBJ_PORTP(obj)        MINIM_OBJ_SAME_TYPE(obj, MINIM_OBJ_PORT)

#define MINIM_OBJ_NUMBERP(obj)      (MINIM_OBJ_EXACTP(obj) || MINIM_OBJ_INEXACTP(obj))
#define MINIM_OBJ_FUNCP(obj)        (MINIM_OBJ_BUILTINP(obj) || MINIM_OBJ_CLOSUREP(obj) || MINIM_OBJ_JUMPP(obj))

// Accessors 

#define MINIM_OBJ_TYPE(obj)         ((obj)->type)
#define MINIM_EXACTNUM(obj)         (*((mpq_ptr*) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_INEXACTNUM(obj)       (*((double*) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_SYMBOL_INTERN(obj)    (*((uint8_t*) VOID_PTR(PTR(obj, 1))))
#define MINIM_SYMBOL(obj)           (*((char**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_STRING(obj)           (*((char**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_CAR(obj)              (*((MinimObject**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_CDR(obj)              (*((MinimObject**) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_VECTOR(obj)           (*((MinimObject***) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_VECTOR_REF(obj, i)    ((*((MinimObject***) VOID_PTR(PTR(obj, PTR_SIZE))))[i])
#define MINIM_VECTOR_LEN(obj)       (*((size_t*) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_HASH_TABLE(obj)       (*((MinimHash**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_PROMISE_VAL(obj)      (*((MinimObject**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_PROMISE_ENV(obj)      (*((MinimEnv**) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_PROMISE_STATE(obj)    (*((uint8_t*) VOID_PTR(PTR(obj, 1))))
#define MINIM_BUILTIN(obj)          (*((MinimBuiltin*) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_SYNTAX(obj)           (*((MinimBuiltin*) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_CLOSURE(obj)          (*((MinimLambda**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_TAIL_CALL(obj)        (*((MinimTailCall**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_TRANSFORM_TYPE(obj)   (*((uint8_t*) VOID_PTR(PTR(obj, 1))))
#define MINIM_TRANSFORMER(obj)      (*((MinimObject**) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_STX_VAL(obj)          (*((MinimObject**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_STX_LOC(obj)          (*((SyntaxLoc**) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_SEQUENCE(obj)         (*((MinimSeq**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_VALUES(obj)           (*((MinimObject***) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_VALUES_REF(obj, i)    ((*((MinimObject***) VOID_PTR(PTR(obj, PTR_SIZE))))[i])
#define MINIM_VALUES_SIZE(obj)      (*((size_t*) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_ERROR(obj)            (*((MinimError**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_CHAR(obj)             (*((unsigned int*) VOID_PTR(PTR(obj, sizeof(int)))))
#define MINIM_JUMP_BUF(obj)         (*((jmp_buf**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_JUMP_VAL(obj)         (*((MinimObject**) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_PORT_TYPE(obj)        (*((uint8_t*) VOID_PTR(PTR(obj, 1))))
#define MINIM_PORT_MODE(obj)        (*((uint8_t*) VOID_PTR(PTR(obj, 2))))
#define MINIM_PORT_FILE(obj)        (*((FILE**) VOID_PTR(PTR(obj, PTR_SIZE))))
#define MINIM_PORT_NAME(obj)        (*((char**) VOID_PTR(PTR(obj, 2 * PTR_SIZE))))
#define MINIM_PORT_ROW(obj)         (*((size_t*) VOID_PTR(PTR(obj, 3 * PTR_SIZE))))
#define MINIM_PORT_COL(obj)         (*((size_t*) VOID_PTR(PTR(obj, 4 * PTR_SIZE))))
#define MINIM_PORT_POS(obj)         (*((size_t*) VOID_PTR(PTR(obj, 5 * PTR_SIZE))))

// Compound accessors

#define MINIM_CAAR(obj)             MINIM_CAR(MINIM_CAR(obj))
#define MINIM_CADR(obj)             MINIM_CAR(MINIM_CDR(obj))
#define MINIM_CDAR(obj)             MINIM_CDR(MINIM_CAR(obj))
#define MINIM_CDDR(obj)             MINIM_CDR(MINIM_CDR(obj))

#define MINIM_TAIL(dest, x)                     \
{                                               \
    dest = x;                                   \
    while (!minim_nullp(MINIM_CDR(dest)))       \
        dest = MINIM_CDR(dest);                 \
}

#define MINIM_CDNR(dest, x, it, n)                              \
{                                                               \
    dest = x;                                                   \
    for (size_t it = 0; it < n; ++it, dest = MINIM_CDR(dest));  \
}

#define MINIM_STX_CAR(o)        (MINIM_CAR(MINIM_STX_VAL(o)))
#define MINIM_STX_CDR(o)        (MINIM_CDR(MINIM_STX_VAL(o)))
#define MINIM_STX_CADR(o)       (MINIM_CADR(MINIM_STX_VAL(o)))
#define MINIM_STX_TAIL(d, o)    (MINIM_TAIL(d, MINIM_STX_VAL(o)))
#define MINIM_STX_SYMBOL(o)     (MINIM_SYMBOL(MINIM_STX_VAL(o)))

// Setters

#define MINIM_SYMBOL_SET_INTERNED(obj, s)   (*((uint8_t*) VOID_PTR(PTR(obj, 1))) = (s))
#define MINIM_PROMISE_SET_STATE(obj, s)     (*((uint8_t*) VOID_PTR(PTR(obj, 1))) = (s))

#define MINIM_VECTOR_RESIZE(v, s)                                               \
{                                                                               \
    MINIM_VECTOR_LEN(v) = s;                                                    \
    MINIM_VECTOR(v) = GC_realloc(MINIM_VECTOR(v),                               \
                                 MINIM_VECTOR_LEN(v) * sizeof(MinimObject*));   \
}

// Special values

#define MINIM_TRANSFORM_MACRO       0
#define MINIM_TRANSFORM_PATTERN     1
#define MINIM_TRANSFORM_UNKNOWN     2

#define MINIM_PORT_TYPE_FILE        0

#define MINIM_PORT_MODE_WRITE       0x01        // port open for writing
#define MINIM_PORT_MODE_READ        0x02        // port open for reading
#define MINIM_PORT_MODE_OPEN        0x04        // port is open
#define MINIM_PORT_MODE_READY       0x08        // port is ready
#define MINIM_PORT_MODE_ALT_EOF     0x10        // EOF will close program and '\n' acts as EOF
#define MINIM_PORT_MODE_NO_FREE     0x20        // GC will not run destructor on port

// Additional predicates

#define MINIM_STX_NULLP(x)          (MINIM_OBJ_ASTP(x) && minim_nullp(MINIM_STX_VAL(x)))
#define MINIM_STX_PAIRP(x)          (MINIM_OBJ_ASTP(x) && MINIM_OBJ_PAIRP(MINIM_STX_VAL(x)))
#define MINIM_STX_SYMBOLP(x)        (MINIM_OBJ_ASTP(x) && MINIM_OBJ_SYMBOLP(MINIM_STX_VAL(x)))
#define MINIM_STX_VECTORP(x)        (MINIM_OBJ_ASTP(x) && MINIM_OBJ_VECTORP(MINIM_STX_VAL(x)))

#define MINIM_INPUT_PORTP(x)        (MINIM_OBJ_PORTP(x) && (MINIM_PORT_MODE(x) & MINIM_PORT_MODE_READ))
#define MINIM_OUTPUT_PORTP(x)       (MINIM_OBJ_PORTP(x) && (MINIM_PORT_MODE(x) & MINIM_PORT_MODE_WRITE))

//  Initialization (simple)

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
MinimObject *minim_transform(void *binding, int type);
MinimObject *minim_ast(void *val, void *loc);
MinimObject *minim_sequence(void *seq);
MinimObject *minim_values(size_t len, void *arr);
MinimObject *minim_err(void *err);
MinimObject *minim_char(unsigned int ch);
MinimObject *minim_jmp(void *ptr, void *val);
MinimObject *minim_file_port(FILE *f, uint8_t mode);

//  Equivalence

bool minim_eqp(MinimObject *a, MinimObject *b);
bool minim_eqvp(MinimObject *a, MinimObject *b);
bool minim_equalp(MinimObject *a, MinimObject *b);

//  Miscellaneous

Buffer* minim_obj_to_bytes(MinimObject *obj);

#define coerce_into_bool(x)    ((x) != minim_false)
#define to_bool(x)             ((x) ? minim_true : minim_false)

//
//  Evaluation
//

// Evaluates the syntax tree stored at 'ast' and stores the
// result at 'pobj'. Returns a non-zero result on success.
MinimObject *eval_ast(MinimEnv* env, MinimObject *ast);

// Same as 'eval_ast' except it does not run the syntax checker
MinimObject *eval_ast_no_check(MinimEnv* env, MinimObject *ast);

// Evaluates a syntax terminal. Returns NULL on failure.
MinimObject *eval_ast_terminal(MinimEnv *env, MinimObject *ast);

// Evaluates `module` up to defining macros
void eval_module_cached(MinimModule *module);

// Evaluates `module` up to applying syntax macros
void eval_module_macros(MinimModule *module);

// Evaluates `module` and returns the result
void eval_module(MinimModule *module);

// Evaluates an expression and returns a string.
char *eval_string(char *str, size_t len);

//
//  Boot
//

#define MINIM_FLAG_LOAD_LIBS        0x1
#define MINIM_FLAG_NO_RUN           0x2
#define MINIM_FLAG_NO_CACHE         0x4
#define MINIM_FLAG_COMPILE          0x8
#define IF_FLAG_RAISED(x, fl, t, f)     ((((x) & (fl)) == 0) ? (f) : (t))

#define GLOBAL_FLAG_DEFAULT     0x0
#define GLOBAL_FLAG_COMPILE     0x1
#define GLOBAL_FLAG_CACHE       0x2

// Loads builtins
void init_builtins();

// initialize builtins, intern table, other info
void init_global_state(uint8_t flags);

// global object
extern MinimGlobal global;

//
//  Module / Environment
//

// Initializes a new environment object.
void init_env(MinimEnv **penv, MinimEnv *parent, MinimLambda *callee);

void init_minim_module(MinimModule **pmodule);

//
//  Reading / Parsing
//

#define READ_FLAG_WAIT          0x1     // behavior upon encountering end of input

MinimObject *minim_parse_port(MinimObject *port, MinimObject **perr, uint8_t flags);

// Reads a file given by `str` and returns a new module whose previous module is `prev`.
MinimModule *minim_load_file_as_module(MinimModule *prev, const char *fname);

// Runs a file in a new environment where `env` contains the builtin environment
// at its lowest level.
void minim_load_file(MinimEnv *env, const char *fname);

// Runs a file in the environment `env`.
void minim_run_file(MinimEnv *env, const char *fname);

//
//  Numbers
//

bool minim_zerop(MinimObject *num);
bool minim_positivep(MinimObject *num);
bool minim_negativep(MinimObject *num);
bool minim_evenp(MinimObject *num);
bool minim_oddp(MinimObject *num);
bool minim_integerp(MinimObject *thing);
bool minim_exact_integerp(MinimObject *thing);
bool minim_exact_nonneg_intp(MinimObject *thing);
bool minim_nanp(MinimObject *thing);
bool minim_infinitep(MinimObject *thing);

MinimObject *int_to_minim_number(long int x);
MinimObject *uint_to_minim_number(size_t x);

#define MINIM_NUMBER_TO_UINT(obj)      mpz_get_ui(mpq_numref(MINIM_EXACTNUM(obj)))

int minim_number_cmp(MinimObject *a, MinimObject *b);

//
//  Printing
//

#define MINIM_DEFAULT_ERR_LOC_LEN       30

#define PRINT_PARAMS_DEFAULT_MAXLEN     UINT_MAX
#define PRINT_PARAMS_DEFAULT_QUOTE      false
#define PRINT_PARAMS_DEFAULT_DISPLAY    false
#define PRINT_PARAMS_DEFAULT_SYNTAX     false

// Sets the print param
void set_print_params(PrintParams *pp,
                      size_t maxlen,
                      bool quote,
                      bool display,
                      bool syntax);

// Sets print params to default
#define set_default_print_params(pp)                        \
    set_print_params(pp, PRINT_PARAMS_DEFAULT_MAXLEN,       \
                         PRINT_PARAMS_DEFAULT_QUOTE,        \
                         PRINT_PARAMS_DEFAULT_DISPLAY,      \
                         PRINT_PARAMS_DEFAULT_SYNTAX);       

// Sets print params for printing syntax
#define set_syntax_print_params(pp)                         \
    set_print_params(pp, PRINT_PARAMS_DEFAULT_MAXLEN,       \
                         true,                              \
                         PRINT_PARAMS_DEFAULT_DISPLAY,      \
                         true);

// Writes a string representation of the object to stdout
int print_minim_object(MinimObject *obj, MinimEnv *env, PrintParams *pp);

// Writes a string representation of the object to the buffer
int print_to_buffer(Buffer *bf, MinimObject* obj, MinimEnv *env, PrintParams *pp);

// Writes a string representation of the object to stream.
int print_to_port(MinimObject *obj, MinimEnv *env, PrintParams *pp, FILE *stream);

// Returns a string representation of the object.
char *print_to_string(MinimObject *obj, MinimEnv *env, PrintParams *pp);

#endif
