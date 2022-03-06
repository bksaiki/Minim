#ifndef _MINIM_PRIV_H_
#define _MINIM_PRIV_H_

#include "minim.h"
#include "../common/system.h"
#include "../common/types.h"

//
//  Structures
//

// Location within syntax
struct SyntaxLoc
{
    MinimObject *src;       // source object (usually a string)
    size_t row, col;        // row and col of source location the syntax object
    size_t pos;             // pos of the source location the syntax object
    size_t span;            // span (width) of the source location of the object
} typedef SyntaxLoc;

// intern table bucket
typedef struct InternTableBucket {
    MinimObject *sym;
    struct InternTableBucket *next;
} InternTableBucket;

// intern table
typedef struct InternTable
{
    InternTableBucket **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} InternTable;

// symbol table bucket
struct MinimSymbolTableBucket
{
    char *key;
    MinimObject *val;
    struct MinimSymbolTableBucket *next;
} typedef MinimSymbolTableBucket;

// symbol table
struct MinimSymbolTable
{
    MinimSymbolTableBucket **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} typedef MinimSymbolTable;

// module cache
typedef struct MinimModuleCache
{
    struct MinimModule** modules;
    size_t modulec;
} MinimModuleCache;

// closure
typedef struct MinimLambda
{
    MinimObject *body;
    SyntaxLoc *loc;
    MinimEnv *env;
    char **args, *rest, *name;
    int argc;
} MinimLambda;

// tail call object
typedef struct MinimTailCall
{
    MinimLambda *lam;
    MinimObject **args;
    size_t argc;
} MinimTailCall;

// hash table bucket
typedef struct MinimHashBucket
{
    MinimObject *key, *val;
    struct MinimHashBucket *next;
} MinimHashBucket;

// hash table
typedef struct MinimHash
{
    MinimHashBucket **buckets;
    size_t *alloc_ptr;
    size_t alloc, size;
} MinimHash;

// sequence object
typedef struct MinimSeq
{
    MinimObject *val;       // current iterator
    MinimObject *first;     // closure or primitive that returns the current object
    MinimObject *rest;      // closure or primitive that updates the sequence
    MinimObject *donep;     // closure or primitive to query if the sequence has terminated
} MinimSeq;

// error stack trace
typedef struct MinimErrorTrace
{
    struct MinimErrorTrace *next;
    SyntaxLoc *loc;
    char *name;
    bool multiple;
} MinimErrorTrace;

// error descriptors
typedef struct MinimErrorDescTable
{
    char **keys, **vals;
    size_t len;
} MinimErrorDescTable;

// error structure
typedef struct MinimError
{
    MinimErrorTrace *top;
    MinimErrorTrace *bottom;
    MinimErrorDescTable *table;
    char *where;
    char *msg;
} MinimError;

//
// Reading / Parsing
//

// Loads minim file from cache. Returns a port object
// of the file or NULL on failure.
MinimObject *load_processed_file(MinimObject *fport);

// Outputs a module into a minim source code.
void emit_processed_file(MinimObject *fport, MinimModule *module);

//
//  Syntax
//

// Constructs a syntax location
SyntaxLoc *init_syntax_loc(MinimObject *src,
                           size_t row,
                           size_t col,
                           size_t pos,
                           size_t span);

// Returns the length of syntax list
size_t syntax_list_len(MinimObject *stx);

// Returns the length of syntax list. Return SIZE_MAX
// if the list is not proper.
size_t syntax_proper_list_len(MinimObject *stx);

// Throws an error if `ast` is ill-formatted syntax.
void check_syntax(MinimEnv *env, MinimObject *ast);

// Converts an object to syntax.
MinimObject *datum_to_syntax(MinimEnv *env, MinimObject *obj);

// Recursively unwraps a syntax object.
MinimObject *syntax_unwrap_rec(MinimEnv *env, MinimObject *stx);

// Recursively unwraps a syntax object.
// Allows evaluation for `unquote`.
MinimObject *syntax_unwrap_rec2(MinimEnv *env, MinimObject *stx);

//
//  Transform
//

// Applies a transformation on syntax
MinimObject *transform_loc(MinimEnv *env,
                           MinimObject *trans,
                           MinimObject *stx);

// Applies syntax transforms on ast
MinimObject* transform_syntax(MinimEnv *env, MinimObject* ast);

// Returns if a [match replace] transform is valid
void check_transform(MinimEnv *env,
                     MinimObject *match,
                     MinimObject *replace,
                     MinimObject *reserved);

//
//  Interning
//

extern size_t bucket_sizes[];

InternTable *init_intern_table();
MinimObject *intern_symbol(InternTable *itab, const char *sym);

//
//  Symbol table
//

void init_minim_symbol_table(MinimSymbolTable **ptable);

void minim_symbol_table_add(MinimSymbolTable *table, const char *name, MinimObject *obj);

void minim_symbol_table_add2(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj);

int minim_symbol_table_set(MinimSymbolTable *table, const char *name, size_t hash, MinimObject *obj);

MinimObject *minim_symbol_table_get(MinimSymbolTable *table, const char *name, size_t hash);

const char *minim_symbol_table_peek_name(MinimSymbolTable *table, MinimObject *obj);

void minim_symbol_table_merge(MinimSymbolTable *dest,
                              MinimSymbolTable *src);

void minim_symbol_table_merge2(MinimSymbolTable *dest,
                               MinimSymbolTable *src,
                               MinimObject *(*merge)(MinimObject *, MinimObject *),
                               MinimObject *(*add)(MinimObject *));

void minim_symbol_table_for_each(MinimSymbolTable *table,
                                 void (*func)(const char *, MinimObject *));

//
//  Global
//

// setters
#define intern(s)           intern_symbol(global.symbols, s)
#define set_builtin(n, o)   minim_symbol_table_add(global.builtins, MINIM_SYMBOL(intern(n)), o)

// modifiers
#if MINIM_TRACK_STATS == 1
#define log_expr_evaled()     ++global.stat_exprs
#define log_proc_called()     ++global.stat_procs
#define log_obj_created()     ++global.stat_objs
#else
#define log_expr_evaled()
#define log_proc_called()
#define log_obj_created()
#endif

//
//  Modules
//

#define MINIM_MODULE_LOADED     0x1
#define MINIM_MODULE_INIT       0x2

/*
    Module / Environment organization

    +---------------+
    |   Global Env  |
    +---------------+
            |              call stack grows -->
    +---------------+           +-----+           +-----+       
    |    Module     |   <---->  | Env |  <------  | Env |  <---- ....
    +---------------+           +-----+           +-----+ 
            |
    +---------------+           +-----+
    |    Module     |   <---->  | Env | 
    +---------------+           +-----+
            |
           ...
       imports grow
            |
           \ /

    Modules contain top level environments.
    When a new environment is created, it points to the previous environment.
    Imported modules are considered accessible to modules above

    Any identifier is visible in an environment if
      (a) it is stored directly in that environment
      (b) there exists a path to the environment that contains it
*/

#define MINIM_MODULE_BODY(m)    \
    MINIM_STX_VAL(MINIM_CADR(MINIM_CDDR(MINIM_STX_VAL((m)->body))))

void minim_module_add_expr(MinimModule *module, MinimObject *expr);
void minim_module_add_import(MinimModule *module, MinimModule *import);
void minim_module_set_path(MinimModule *module, const char *path);

MinimObject *minim_module_get_sym(MinimModuleInstance *module, const char *sym);
MinimModule *minim_module_get_import(MinimModule *module, const char *name, const char *path);

void init_minim_module_cache(MinimModuleCache **pcache);
void minim_module_cache_add(MinimModuleCache *cache, MinimModule *import);
MinimModule *minim_module_cache_get(MinimModuleCache *cache, const char *name, const char *path);

//
//  Expander
//

void expand_minim_module(MinimEnv *env, MinimModule *module);
MinimObject *expand_module_level(MinimEnv *env, MinimObject *stx);

//
//  Environment
//

#define MINIM_ENV_TAIL_CALLABLE     0x1

// Returns a the object associated with the symbol. Returns NULL if
// the symbol is not in the table
MinimObject *env_get_sym(MinimEnv *env, const char *sym);

// Adds 'sym' and 'obj' to the variable table.
void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj);

// Sets 'sym' to 'obj'. Returns zero if 'sym' is not found.
int env_set_sym(MinimEnv *env, const char *sym, MinimObject *obj);

// Returns a pointer to the key associated with the values. Returns NULL
// if the value is not in the table
const char *env_peek_key(MinimEnv *env, MinimObject *obj);

/// Make tail call
NORETURN void unwind_tail_call(MinimEnv *env, MinimTailCall *tc);

// Returns the number of symbols in the environment
size_t env_symbol_count(MinimEnv *env);

// Returns true if 'lam' has been called previously.
bool env_has_called(MinimEnv *env, MinimLambda *lam);

// Debugging: dumps all symbols in environment
void env_dump_symbols(MinimEnv *env);

// Debugging: dumps exportable symbols in environment
void env_dump_exports(MinimEnv *env);

//
//  Evaluation
//

// Evaluates known syntax
MinimObject *eval_top_level(MinimEnv *env, MinimObject *stx, MinimBuiltin fn);

//
//  Closures
//

void init_minim_lambda(MinimLambda **plam);

// Evaluates the given lambda expression with `env` as the caller and
// lam->env as the previous namespace.
MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args);

// Like `eval_lambda` but transforms from the module of `env` are referencable.
MinimObject *eval_lambda2(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args);

void minim_lambda_to_buffer(MinimLambda *l, Buffer *bf);

//
//  Tail call
//

void init_minim_tail_call(MinimTailCall **ptail,
                          MinimLambda *lam,
                          size_t argc,
                          MinimObject **args);

//
//  Jumps
//

NORETURN
void minim_long_jump(MinimObject *jmp, MinimEnv *env, size_t argc, MinimObject **args);

//
//  Pairs / Lists
//

bool minim_consp(MinimObject* thing);
bool minim_listp(MinimObject* thing);
bool minim_listof(MinimObject* list, MinimPred pred);
bool minim_cons_eqp(MinimObject *a, MinimObject *b);
void minim_cons_to_bytes(MinimObject *obj, Buffer *bf);

MinimObject *minim_list(MinimObject **args, size_t len);
MinimObject *minim_list_drop(MinimObject *lst, size_t n);
MinimObject *minim_list_append2(MinimObject *a, MinimObject *b);
size_t minim_list_length(MinimObject *list);

#define minim_list_ref(lst, n)      MINIM_CAR(minim_list_drop(lst, n))

//
//  Numbers
//

/* Allocates mpq struct that can be GC'd */
mpq_ptr gc_alloc_mpq_ptr();
bool assert_numerical_args(MinimObject **args, size_t argc, MinimObject **res, const char *name);

//
//  Numbers
//

// String predicates
bool is_rational(const char *str);
bool is_float(const char *str);
bool is_char(const char *str);
bool is_str(const char *str);

// String to number
MinimObject *str_to_number(const char *str, MinimObjectType type);

//
//  Vectors
//

void minim_vector_bytes(MinimObject *vec, Buffer *bf);

//
//  Hash Table
//

#define MINIM_DEFAULT_HASH_TABLE_SIZE       10
#define MINIM_DEFAULT_HASH_TABLE_FACTOR     0.75

void init_minim_hash_table(MinimHash **pht);
void copy_minim_hash_table(MinimHash **pht, MinimHash *src);
bool minim_hash_table_eqp(MinimHash *a, MinimHash *b);

uint32_t hash_bytes(const void* data, size_t len);
#define hash_symbol(s)  hash_bytes(&s, sizeof(&s))

//
//  Sequences
//

void init_minim_seq(MinimSeq **pseq,
                    MinimObject *init,
                    MinimObject *first,
                    MinimObject *rest,
                    MinimObject *donep);

//
//  Ports
//

#define port_eof(p)         (MINIM_PORT_MODE(p) & MINIM_PORT_MODE_ALT_EOF ? '\n' : EOF)
#define next_ch(p)          (fgetc(MINIM_PORT_FILE(p)))
#define put_back(p, c)      (ungetc(c, MINIM_PORT_FILE(p)))

void update_port(MinimObject *port, char c);

//
//  Errors
//

void init_minim_error_trace(MinimErrorTrace **ptrace, SyntaxLoc* loc, const char* name);

void init_minim_error_desc_table(MinimErrorDescTable **ptable, size_t len);
void minim_error_desc_table_set(MinimErrorDescTable *table, size_t idx, const char *key, const char *val);

void init_minim_error(MinimError **perr, const char *msg, const char *where);
void minim_error_add_trace(MinimError *err, SyntaxLoc *loc, const char* name);

MinimObject *minim_syntax_error(const char *msg, const char *where, MinimObject *expr, MinimObject *subexpr);
MinimObject *minim_argument_error(const char *pred, const char *where, size_t pos, MinimObject *val);
MinimObject *minim_arity_error(const char *where, size_t min, size_t max, size_t actual);
MinimObject *minim_values_arity_error(const char *where, size_t expected, size_t actual, MinimObject *expr);
MinimObject *minim_error(const char *msg, const char *where, ...);

NORETURN void throw_minim_error(MinimEnv *env, MinimObject *err);
#define THROW(e, x)    throw_minim_error(e, x)

//
//  Primitives
//

typedef struct MinimArity {
    size_t low, high;
} MinimArity;

// Sets 'parity' to the arity of fun
bool minim_get_builtin_arity(MinimBuiltin fun, MinimArity *parity);

// Sets 'parity' to the arity of the syntax
bool minim_get_syntax_arity(MinimBuiltin fun, MinimArity *parity);

// Sets 'parity' to the arity of the lambda
bool minim_get_lambda_arity(MinimLambda *lam, MinimArity *parity);

// Checks the arity of a builtin function
bool minim_check_arity(MinimBuiltin fun, size_t argc, MinimEnv *env, MinimObject **perr);

// Checks the arity of syntax
bool minim_check_syntax_arity(MinimBuiltin fun, size_t argc, MinimEnv *env);

#define DEFINE_BUILTIN_FUN(name)  \
    MinimObject *minim_builtin_ ## name(MinimEnv *env, size_t argc, MinimObject **args);

// Variable / Control
DEFINE_BUILTIN_FUN(def_values)
DEFINE_BUILTIN_FUN(setb)
DEFINE_BUILTIN_FUN(if)
DEFINE_BUILTIN_FUN(let_values)
DEFINE_BUILTIN_FUN(letstar_values)
DEFINE_BUILTIN_FUN(begin)
DEFINE_BUILTIN_FUN(quote)
DEFINE_BUILTIN_FUN(quasiquote)
DEFINE_BUILTIN_FUN(unquote)
DEFINE_BUILTIN_FUN(lambda)
DEFINE_BUILTIN_FUN(exit)
DEFINE_BUILTIN_FUN(delay)
DEFINE_BUILTIN_FUN(force)
DEFINE_BUILTIN_FUN(values)
DEFINE_BUILTIN_FUN(callcc)

// Modules
DEFINE_BUILTIN_FUN(export)
DEFINE_BUILTIN_FUN(import)

// Transforms
DEFINE_BUILTIN_FUN(def_syntaxes)
DEFINE_BUILTIN_FUN(syntax)
DEFINE_BUILTIN_FUN(syntaxp)
DEFINE_BUILTIN_FUN(unwrap)
DEFINE_BUILTIN_FUN(syntax_case)
DEFINE_BUILTIN_FUN(to_syntax)
DEFINE_BUILTIN_FUN(template)
DEFINE_BUILTIN_FUN(identifier_equalp)

// Errors
DEFINE_BUILTIN_FUN(error)
DEFINE_BUILTIN_FUN(argument_error)
DEFINE_BUILTIN_FUN(arity_error)
DEFINE_BUILTIN_FUN(syntax_error)

// Miscellaneous
DEFINE_BUILTIN_FUN(equalp)
DEFINE_BUILTIN_FUN(eqvp)
DEFINE_BUILTIN_FUN(eqp)
DEFINE_BUILTIN_FUN(symbolp)
DEFINE_BUILTIN_FUN(printf)
DEFINE_BUILTIN_FUN(void)
DEFINE_BUILTIN_FUN(version);
DEFINE_BUILTIN_FUN(symbol_count);
DEFINE_BUILTIN_FUN(dump_symbols);
DEFINE_BUILTIN_FUN(def_print_method)

// Procedure
DEFINE_BUILTIN_FUN(procedurep)
DEFINE_BUILTIN_FUN(procedure_arity)

// Promise
DEFINE_BUILTIN_FUN(promisep)

// Boolean
DEFINE_BUILTIN_FUN(boolp)
DEFINE_BUILTIN_FUN(not)

// Number
DEFINE_BUILTIN_FUN(numberp)
DEFINE_BUILTIN_FUN(zerop)
DEFINE_BUILTIN_FUN(positivep)
DEFINE_BUILTIN_FUN(negativep)
DEFINE_BUILTIN_FUN(evenp)
DEFINE_BUILTIN_FUN(oddp)
DEFINE_BUILTIN_FUN(exactp)
DEFINE_BUILTIN_FUN(inexactp)
DEFINE_BUILTIN_FUN(integerp)
DEFINE_BUILTIN_FUN(nanp)
DEFINE_BUILTIN_FUN(infinitep)
DEFINE_BUILTIN_FUN(eq)
DEFINE_BUILTIN_FUN(gt)
DEFINE_BUILTIN_FUN(lt)
DEFINE_BUILTIN_FUN(gte)
DEFINE_BUILTIN_FUN(lte)
DEFINE_BUILTIN_FUN(to_exact)
DEFINE_BUILTIN_FUN(to_inexact)

// Character
DEFINE_BUILTIN_FUN(charp)
DEFINE_BUILTIN_FUN(char_eqp)
DEFINE_BUILTIN_FUN(char_gtp)
DEFINE_BUILTIN_FUN(char_ltp)
DEFINE_BUILTIN_FUN(char_gtep)
DEFINE_BUILTIN_FUN(char_ltep)
DEFINE_BUILTIN_FUN(char_ci_eqp)
DEFINE_BUILTIN_FUN(char_ci_gtp)
DEFINE_BUILTIN_FUN(char_ci_ltp)
DEFINE_BUILTIN_FUN(char_ci_gtep)
DEFINE_BUILTIN_FUN(char_ci_ltep)
DEFINE_BUILTIN_FUN(char_alphabeticp)
DEFINE_BUILTIN_FUN(char_numericp)
DEFINE_BUILTIN_FUN(char_whitespacep)
DEFINE_BUILTIN_FUN(char_upper_casep)
DEFINE_BUILTIN_FUN(char_lower_casep)
DEFINE_BUILTIN_FUN(int_to_char)
DEFINE_BUILTIN_FUN(char_to_int)
DEFINE_BUILTIN_FUN(char_upcase)
DEFINE_BUILTIN_FUN(char_downcase)

// String
DEFINE_BUILTIN_FUN(stringp)
DEFINE_BUILTIN_FUN(make_string)
DEFINE_BUILTIN_FUN(string)
DEFINE_BUILTIN_FUN(string_length)
DEFINE_BUILTIN_FUN(string_ref)
DEFINE_BUILTIN_FUN(string_setb)
DEFINE_BUILTIN_FUN(string_copy)
DEFINE_BUILTIN_FUN(string_fillb)
DEFINE_BUILTIN_FUN(substring)
DEFINE_BUILTIN_FUN(string_to_symbol)
DEFINE_BUILTIN_FUN(symbol_to_string)
DEFINE_BUILTIN_FUN(string_to_number)
DEFINE_BUILTIN_FUN(number_to_string)
DEFINE_BUILTIN_FUN(format)

// Pair
DEFINE_BUILTIN_FUN(cons)
DEFINE_BUILTIN_FUN(consp)
DEFINE_BUILTIN_FUN(car)
DEFINE_BUILTIN_FUN(cdr)

DEFINE_BUILTIN_FUN(caar)
DEFINE_BUILTIN_FUN(cadr)
DEFINE_BUILTIN_FUN(cdar)
DEFINE_BUILTIN_FUN(cddr)

DEFINE_BUILTIN_FUN(caaar)
DEFINE_BUILTIN_FUN(caadr)
DEFINE_BUILTIN_FUN(cadar)
DEFINE_BUILTIN_FUN(caddr)
DEFINE_BUILTIN_FUN(cdaar)
DEFINE_BUILTIN_FUN(cdadr)
DEFINE_BUILTIN_FUN(cddar)
DEFINE_BUILTIN_FUN(cdddr)

DEFINE_BUILTIN_FUN(caaaar)
DEFINE_BUILTIN_FUN(caaadr)
DEFINE_BUILTIN_FUN(caadar)
DEFINE_BUILTIN_FUN(caaddr)
DEFINE_BUILTIN_FUN(cadaar)
DEFINE_BUILTIN_FUN(cadadr)
DEFINE_BUILTIN_FUN(caddar)
DEFINE_BUILTIN_FUN(cadddr)
DEFINE_BUILTIN_FUN(cdaaar)
DEFINE_BUILTIN_FUN(cdaadr)
DEFINE_BUILTIN_FUN(cdadar)
DEFINE_BUILTIN_FUN(cdaddr)
DEFINE_BUILTIN_FUN(cddaar)
DEFINE_BUILTIN_FUN(cddadr)
DEFINE_BUILTIN_FUN(cdddar)
DEFINE_BUILTIN_FUN(cddddr)

// List
DEFINE_BUILTIN_FUN(list)
DEFINE_BUILTIN_FUN(listp)
DEFINE_BUILTIN_FUN(nullp)
DEFINE_BUILTIN_FUN(length)
DEFINE_BUILTIN_FUN(append)
DEFINE_BUILTIN_FUN(reverse)
DEFINE_BUILTIN_FUN(list_ref)
DEFINE_BUILTIN_FUN(map)
DEFINE_BUILTIN_FUN(andmap)
DEFINE_BUILTIN_FUN(ormap)
DEFINE_BUILTIN_FUN(apply)

// Hash table
DEFINE_BUILTIN_FUN(hash)
DEFINE_BUILTIN_FUN(hashp)
DEFINE_BUILTIN_FUN(hash_keyp)
DEFINE_BUILTIN_FUN(hash_ref)
DEFINE_BUILTIN_FUN(hash_remove)
DEFINE_BUILTIN_FUN(hash_set)
DEFINE_BUILTIN_FUN(hash_setb)
DEFINE_BUILTIN_FUN(hash_removeb)
DEFINE_BUILTIN_FUN(hash_to_list)

// Vector
DEFINE_BUILTIN_FUN(vector)
DEFINE_BUILTIN_FUN(make_vector)
DEFINE_BUILTIN_FUN(vectorp)
DEFINE_BUILTIN_FUN(vector_length)
DEFINE_BUILTIN_FUN(vector_ref)
DEFINE_BUILTIN_FUN(vector_setb)
DEFINE_BUILTIN_FUN(vector_to_list)
DEFINE_BUILTIN_FUN(list_to_vector)
DEFINE_BUILTIN_FUN(vector_fillb)

// Sequence
DEFINE_BUILTIN_FUN(sequence)
DEFINE_BUILTIN_FUN(sequencep)
DEFINE_BUILTIN_FUN(sequence_first)
DEFINE_BUILTIN_FUN(sequence_rest)
DEFINE_BUILTIN_FUN(sequence_donep)

// Math
DEFINE_BUILTIN_FUN(add)
DEFINE_BUILTIN_FUN(sub)
DEFINE_BUILTIN_FUN(mul)
DEFINE_BUILTIN_FUN(div)
DEFINE_BUILTIN_FUN(abs)
DEFINE_BUILTIN_FUN(max)
DEFINE_BUILTIN_FUN(min)
DEFINE_BUILTIN_FUN(modulo)
DEFINE_BUILTIN_FUN(remainder)
DEFINE_BUILTIN_FUN(quotient)
DEFINE_BUILTIN_FUN(numerator)
DEFINE_BUILTIN_FUN(denominator)
DEFINE_BUILTIN_FUN(gcd)
DEFINE_BUILTIN_FUN(lcm)

DEFINE_BUILTIN_FUN(floor)
DEFINE_BUILTIN_FUN(ceil)
DEFINE_BUILTIN_FUN(trunc)
DEFINE_BUILTIN_FUN(round)

DEFINE_BUILTIN_FUN(sqrt)
DEFINE_BUILTIN_FUN(exp)
DEFINE_BUILTIN_FUN(log)
DEFINE_BUILTIN_FUN(pow)
DEFINE_BUILTIN_FUN(sin)
DEFINE_BUILTIN_FUN(cos)
DEFINE_BUILTIN_FUN(tan)
DEFINE_BUILTIN_FUN(asin)
DEFINE_BUILTIN_FUN(acos)
DEFINE_BUILTIN_FUN(atan)

// Port
DEFINE_BUILTIN_FUN(current_input_port)
DEFINE_BUILTIN_FUN(current_output_port)
DEFINE_BUILTIN_FUN(call_with_input_file)
DEFINE_BUILTIN_FUN(call_with_output_file)
DEFINE_BUILTIN_FUN(with_input_from_file)
DEFINE_BUILTIN_FUN(with_output_from_file)
DEFINE_BUILTIN_FUN(portp)
DEFINE_BUILTIN_FUN(input_portp)
DEFINE_BUILTIN_FUN(output_portp)
DEFINE_BUILTIN_FUN(open_input_file)
DEFINE_BUILTIN_FUN(open_output_file)
DEFINE_BUILTIN_FUN(close_input_port)
DEFINE_BUILTIN_FUN(close_output_port)

DEFINE_BUILTIN_FUN(read)
DEFINE_BUILTIN_FUN(read_char)
DEFINE_BUILTIN_FUN(peek_char)
DEFINE_BUILTIN_FUN(char_readyp)
DEFINE_BUILTIN_FUN(eofp)

DEFINE_BUILTIN_FUN(write)
DEFINE_BUILTIN_FUN(display)
DEFINE_BUILTIN_FUN(newline)
DEFINE_BUILTIN_FUN(write_char)

//
//  Printing
//

// Stores custom print methods
extern MinimObject **custom_print_methods;

// For debugging
void debug_print_minim_object(MinimObject *obj, MinimEnv *env);

// Writes a string representation of syntax to the buffer
int print_syntax_to_buffer(Buffer *bf, MinimObject *stx);

// Writes a string representation of syntax to a port
int print_syntax_to_port(MinimObject *stx, FILE *f);

#endif
