#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "../common/path.h"
#include "../gc/gc.h"

#include "arity.h"
#include "builtin.h"
#include "eval.h"
#include "error.h"
#include "global.h"
#include "jmp.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "string.h"
#include "syntax.h"
#include "tail_call.h"
#include "transform.h"

// forward declaration
static MinimObject *eval_ast_node(MinimEnv *env, MinimObject *node);

// eval terminals

static MinimObject *eval_symbol(MinimEnv *env, MinimObject *sym, bool err)
{
    MinimObject *res = env_get_sym(env, MINIM_SYMBOL(sym));
    
    if (!res)   // unknown
    {
        if (err)    THROW(env, minim_error("unrecognized identifier", MINIM_SYMBOL(sym)));
        else        return NULL;
    }

    if (MINIM_OBJ_SYNTAXP(res))
        THROW(env, minim_error("bad syntax", MINIM_SYMBOL(sym)));
    if (MINIM_OBJ_TRANSFORMP(res))
        THROW(env, minim_error("bad transform", MINIM_SYMBOL(sym)));
    
    return res;
}

// Specialized eval functions

#define CALL_BUILTIN(env, proc, argc, args, perr)   \
{                                                   \
    if (!minim_check_arity(proc, argc, env, perr))  \
        THROW(env, err);                            \
    log_proc_called();                              \
    return proc(env, argc, args);                   \
}

// optimized version for builtins - 0ary
static MinimObject *eval_builtin_0ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    MinimObject *err;
    CALL_BUILTIN(env, proc, 0, NULL, &err);
}

// optimized version for builtins - 1ary
static MinimObject *eval_builtin_1ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    MinimObject *arg, *err;
    uint8_t prev_flags = env->flags;

    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    arg = eval_ast_node(env, MINIM_STX_CADR(stx));
    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, 1, &arg, &err);
}

// optimized version for builtins - 2ary
static MinimObject *eval_builtin_2ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    MinimObject *args[2];
    MinimObject *err;
    uint8_t prev_flags = env->flags;

    args[1] = MINIM_STX_CDR(stx);
    args[0] = MINIM_CAR(args[1]);
    args[1] = MINIM_CADR(args[1]);

    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, args[0]);
    args[1] = eval_ast_node(env, args[1]);
    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, 2, args, &err);
}

// optimized version for builtins - 3ary
static MinimObject *eval_builtin_3ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    MinimObject *args[3];
    MinimObject *err;
    uint8_t prev_flags = env->flags;

    args[2] = MINIM_STX_CDR(stx);
    args[0] = MINIM_CAR(args[2]);
    args[2] = MINIM_CDR(args[2]);
    args[1] = MINIM_CAR(args[2]);
    args[2] = MINIM_CADR(args[2]);

    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, args[0]);
    args[1] = eval_ast_node(env, args[1]);
    args[2] = eval_ast_node(env, args[2]);
    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, 3, args, &err);
}

// general version for builtins
static MinimObject *eval_builtin(MinimEnv *env, MinimObject *stx, MinimBuiltin proc, size_t argc)
{
    MinimObject **args;
    MinimObject *err, *it;
    uint8_t prev_flags = env->flags;

    it = MINIM_STX_CDR(stx);
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
    {
        args[i] = eval_ast_node(env, MINIM_CAR(it));
        it = MINIM_CDR(it);
    }

    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, argc, args, &err);
}

// optimized version for syntax - 0ary
static MinimObject *eval_syntax_0ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    return proc(env, 0, NULL);
}

// optimized version for syntax - 1ary
static MinimObject *eval_syntax_1ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    MinimObject *arg = MINIM_STX_CADR(stx);
    return proc(env, 1, &arg);
}

// optimized version for syntax - 2ary
static MinimObject *eval_syntax_2ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    MinimObject *args[2];

    args[1] = MINIM_STX_CDR(stx);
    args[0] = MINIM_CAR(args[1]);
    args[1] = MINIM_CADR(args[1]);
    return proc(env, 2, args);
}

// optimized version for syntax - 3ary
static MinimObject *eval_syntax_3ary(MinimEnv *env, MinimObject *stx, MinimBuiltin proc)
{
    MinimObject *args[3];
    
    args[2] = MINIM_STX_CDR(stx);
    args[0] = MINIM_CAR(args[2]);
    args[2] = MINIM_CDR(args[2]);
    args[1] = MINIM_CAR(args[2]);
    args[2] = MINIM_CADR(args[2]);
    return proc(env, 3, args);
}

// general version for syntax
static MinimObject *eval_syntax(MinimEnv *env, MinimObject *stx, MinimBuiltin proc, size_t argc)
{
    MinimObject **args, *it;
    
    it = MINIM_STX_CDR(stx);
    args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
    {
        args[i] = MINIM_CAR(it);
        it = MINIM_CDR(it);
    }

    return proc(env, argc, args);
}

#define CALL_CLOSURE(env, lam, argc, args)              \
{                                                       \
    if (env_has_called(env, lam))                       \
    {                                                   \
        MinimTailCall *call;                            \
        init_minim_tail_call(&call, lam, argc, args);   \
        return minim_tail_call(call);                   \
    }                                                   \
    else                                                \
    {                                                   \
        log_proc_called();                              \
        return eval_lambda(lam, env, argc, args);       \
    }                                                   \
}

// optimized version for closures - 0ary
static MinimObject *eval_closure_0ary(MinimEnv *env, MinimObject *stx, MinimLambda *lam)
{
    CALL_CLOSURE(env, lam, 0, NULL);
}

// optimized version for closures - 1ary
static MinimObject *eval_closure_1ary(MinimEnv *env, MinimObject *stx, MinimLambda *lam)
{
    MinimObject *arg;
    uint8_t prev_flags;
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    arg = eval_ast_node(env, MINIM_STX_CADR(stx));
    env->flags = prev_flags;
    CALL_CLOSURE(env, lam, 1, &arg);
}

// optimized version for closures - 2ary
static MinimObject *eval_closure_2ary(MinimEnv *env, MinimObject *stx, MinimLambda *lam)
{
    MinimObject *args[2];
    uint8_t prev_flags;

    args[1] = MINIM_STX_CDR(stx);
    args[0] = MINIM_CAR(args[1]);
    args[1] = MINIM_CADR(args[1]);
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, args[0]);
    args[1] = eval_ast_node(env, args[1]);
    env->flags = prev_flags;

    CALL_CLOSURE(env, lam, 2, args);
}

// optimized version for closures - 3ary
static MinimObject *eval_closure_3ary(MinimEnv *env, MinimObject *stx, MinimLambda *lam)
{
    MinimObject *args[3];
    uint8_t prev_flags;

    args[2] = MINIM_STX_CDR(stx);
    args[0] = MINIM_CAR(args[2]);
    args[2] = MINIM_CDR(args[2]);
    args[1] = MINIM_CAR(args[2]);
    args[2] = MINIM_CADR(args[2]);
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, args[0]);
    args[1] = eval_ast_node(env, args[1]);
    args[2] = eval_ast_node(env, args[2]); 
    env->flags = prev_flags;
 
    CALL_CLOSURE(env, lam, 3, args);
}

// general version for closures
static MinimObject *eval_closure(MinimEnv *env, MinimObject *stx, MinimLambda *lam, size_t argc)
{
    MinimObject **args, *it;
    uint8_t prev_flags;
    
    it = MINIM_STX_CDR(stx);
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
    {
        args[i] = eval_ast_node(env, MINIM_CAR(it));
        it = MINIM_CDR(it);
    }

    env->flags = prev_flags;
    CALL_CLOSURE(env, lam, argc, args);
}

static MinimObject *eval_jump(MinimEnv *env, MinimObject *stx, size_t argc, MinimObject *jmp)
{
    MinimObject **args, *it;

    it = MINIM_STX_CDR(stx);
    args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
    {
        args[i] = eval_ast_node(env, MINIM_CAR(it));
        it = MINIM_CDR(it);
    }

    minim_long_jump(jmp, env, argc, args);
}

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, MinimObject *stx)
{
    MinimObject *val;
    
    val = MINIM_STX_VAL(stx);
    if (minim_nullp(val))
    {
        THROW(env, minim_error("missing procedure expression", NULL));
    }
    else if (MINIM_OBJ_PAIRP(val))
    {
        MinimObject *op;
        size_t argc;

        argc = syntax_proper_list_len(stx);
        if (argc == SIZE_MAX)
            THROW(env, minim_syntax_error("bad syntax", NULL, stx, NULL));

        --argc;
        op = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_CAR(val)));
        if (MINIM_OBJ_BUILTINP(op))
        {
            MinimBuiltin proc = MINIM_BUILTIN(op);

            // do not optimize: array must be allocaed
            if (proc == minim_builtin_values ||
                proc == minim_builtin_vector)
                return eval_builtin(env, stx, proc, argc);

            if (argc == 0)          return eval_builtin_0ary(env, stx, proc);
            else if (argc == 1)     return eval_builtin_1ary(env, stx, proc);
            else if (argc == 2)     return eval_builtin_2ary(env, stx, proc);
            else if (argc == 3)     return eval_builtin_3ary(env, stx, proc);
            else                    return eval_builtin(env, stx, proc, argc);
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimBuiltin proc;
            MinimObject *res;
            
            proc = MINIM_SYNTAX(op);
            if (proc == minim_builtin_unquote)
                THROW(env, minim_error("not in a quasiquote", "unquote"));
            
            if (proc == minim_builtin_def_syntaxes)
                THROW(env, minim_error("only allowed at the top-level", "def-syntaxes"));
            else if (proc == minim_builtin_import)
                THROW(env, minim_error("only allowed at the top-level", "%import"));
            else if (proc == minim_builtin_export)
                THROW(env, minim_error("only allowed at the top-level", "%export"));

            if (argc == 0)          res = eval_syntax_0ary(env, stx, proc);
            else if (argc == 1)     res = eval_syntax_1ary(env, stx, proc);
            else if (argc == 2)     res = eval_syntax_2ary(env, stx, proc);
            else if (argc == 3)     res = eval_syntax_3ary(env, stx, proc);
            else                    res = eval_syntax(env, stx, proc, argc);        

            if (MINIM_OBJ_CLOSUREP(res))
            {
                MinimLambda *lam = MINIM_CLOSURE(res);
                if (!lam->loc && MINIM_STX_LOC(MINIM_STX_CAR(stx)))
                    lam->loc = MINIM_STX_LOC(MINIM_STX_CAR(stx));
            }

            return res;
        }
        else if (MINIM_OBJ_CLOSUREP(op))
        {
            if (argc == 0)          return eval_closure_0ary(env, stx, MINIM_CLOSURE(op));
            else if (argc == 1)     return eval_closure_1ary(env, stx, MINIM_CLOSURE(op));
            else if (argc == 2)     return eval_closure_2ary(env, stx, MINIM_CLOSURE(op));
            else if (argc == 3)     return eval_closure_3ary(env, stx, MINIM_CLOSURE(op));
            else                    return eval_closure(env, stx, MINIM_CLOSURE(op), argc);
        }
        else if (MINIM_OBJ_JUMPP(op))
        {
            return eval_jump(env, stx, argc, op);
        }
        else
        {
            THROW(env, minim_syntax_error("unknown operator", NULL,
                                          stx, MINIM_STX_CAR(stx)));
        }
    }
    else if (MINIM_OBJ_VECTORP(val))
    {
        return syntax_unwrap_rec(env, stx);
    }
    else if (MINIM_OBJ_SYMBOLP(val))
    {
        return eval_symbol(env, val, true);
    }
    else
    {
        return val;
    }
}

static MinimObject *eval_top_level(MinimEnv *env, MinimObject *stx, MinimBuiltin fn)
{
    size_t argc = syntax_proper_list_len(stx);
    if (argc == SIZE_MAX)
        THROW(env, minim_syntax_error("bad syntax", NULL, stx, NULL));
    return eval_syntax(env, stx, fn, argc - 1);
}

static bool expr_is_module_level(MinimEnv *env, MinimObject *stx)
{
    MinimObject *val;
    MinimBuiltin fn;

    // not a valid application
    if (!MINIM_STX_PAIRP(stx) ||
        syntax_proper_list_len(stx) == SIZE_MAX ||
        !MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
        return false;
    
    val = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
    if (!val || !MINIM_OBJ_SYNTAXP(val))
        return false;

    fn = MINIM_SYNTAX(val);
    return (fn == minim_builtin_def_syntaxes ||
            fn == minim_builtin_import ||
            fn == minim_builtin_export);
}

static bool expr_is_macro(MinimEnv *env, MinimObject *stx)
{
    MinimObject *val;

    if (!MINIM_STX_PAIRP(stx) ||
        syntax_proper_list_len(stx) == SIZE_MAX ||
        !MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
        return false;

    val = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
    return (val && MINIM_OBJ_SYNTAXP(val) &&
            MINIM_SYNTAX(val) == minim_builtin_def_syntaxes);
}

static bool expr_is_import(MinimEnv *env, MinimObject *stx)
{
    MinimObject *val;

    if (!MINIM_STX_PAIRP(stx) ||
        syntax_proper_list_len(stx) == SIZE_MAX ||
        !MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
        return false;

    val = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
    return (val && MINIM_OBJ_SYNTAXP(val) &&
            MINIM_SYNTAX(val) == minim_builtin_import);
}

static bool expr_is_export(MinimEnv *env, MinimObject *stx)
{
    MinimObject *val;

    if (!MINIM_STX_PAIRP(stx) ||
        syntax_proper_list_len(stx) == SIZE_MAX ||
        !MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
        return false;

    val = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
    return (val && MINIM_OBJ_SYNTAXP(val) &&
            MINIM_SYNTAX(val) == minim_builtin_export);
}

// ================================ Public ================================

MinimObject *eval_ast(MinimEnv *env, MinimObject *ast)
{
    MinimEnv *env2;

    if (expr_is_export(env, ast))
        THROW(env, minim_error("%export not allowed in REPL", NULL));

    init_env(&env2, env, NULL);
    ast = transform_syntax(env, ast);
    if (expr_is_import(env, ast))
    {
        check_syntax(env2, ast);
        return eval_top_level(env, ast, minim_builtin_import);
    }
    else if (expr_is_macro(env, ast))
    {
        return eval_top_level(env, ast, minim_builtin_def_syntaxes);
    }
    else
    {
        check_syntax(env2, ast);
        ast = transform_syntax(env, ast);
        return eval_ast_no_check(env, ast);
    }
}

MinimObject *eval_ast_no_check(MinimEnv* env, MinimObject *ast)
{
    log_expr_evaled();
    return eval_ast_node(env, ast);
}

MinimObject *eval_ast_terminal(MinimEnv *env, MinimObject *stx)
{
    return (MINIM_STX_SYMBOLP(stx) ?
            eval_symbol(env, MINIM_STX_VAL(stx), false) :
            MINIM_STX_VAL(stx));
}

void eval_module_cached(MinimModule *module)
{
    // importing
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_import(module->env, module->exprs[i]))
        {
            check_syntax(module->env, module->exprs[i]);
            eval_top_level(module->env, module->exprs[i], minim_builtin_import);
        }
    }

    // define syntaxes
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_import(module->env, module->exprs[i]))
            continue;

        if (expr_is_macro(module->env, module->exprs[i]))
            eval_top_level(module->env, module->exprs[i], minim_builtin_def_syntaxes);
    }
}

void eval_module_macros(MinimModule *module)
{
    // importing
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_import(module->env, module->exprs[i]))
        {
            check_syntax(module->env, module->exprs[i]);
            eval_top_level(module->env, module->exprs[i], minim_builtin_import);
        }
    }

    // apply transforms from imports
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_import(module->env, module->exprs[i]))
            continue;

        module->exprs[i] = transform_syntax(module->env, module->exprs[i]);
    }

    // define syntaxes
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_import(module->env, module->exprs[i]))
            continue;

        if (expr_is_macro(module->env, module->exprs[i])) {
            eval_top_level(module->env, module->exprs[i], minim_builtin_def_syntaxes);
        }
    }

    // apply transforms
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_module_level(module->env, module->exprs[i]))
            continue;

        module->exprs[i] = transform_syntax(module->env, module->exprs[i]);
    }

    // constant fold
    // for (size_t i = 0; i < module->exprc; ++i)
    // {
    //     if (expr_is_module_level(module->env, module->exprs[i]))
    //         continue;

    //     module->exprs[i] = constant_fold(module->env, module->exprs[i]);
    // }
}

MinimObject *eval_module(MinimModule *module)
{
    PrintParams pp;
    MinimObject *res;

    // Evaluation
    set_default_print_params(&pp);
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_module_level(module->env, module->exprs[i]))
            continue;

        check_syntax(module->env, module->exprs[i]);
        // printf("eval: "); print_syntax_to_port(module->exprs[i], stdout); printf("\n");
        res = eval_ast_no_check(module->env, module->exprs[i]);
        if (!minim_voidp(res))
        {
            print_minim_object(res, module->env, &pp);
            printf("\n");
        }
    }

    // exports
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_export(module->env, module->exprs[i]))
            eval_top_level(module->env, module->exprs[i], minim_builtin_export);
    }

    return minim_void;
}

char *eval_string(char *str, size_t len)
{
    PrintParams pp;
    MinimEnv *env;
    MinimObject *obj, *exit_handler, *port;
    MinimObject *ast, *err;
    jmp_buf *exit_buf;
    FILE *tmp;

    // set up string as file
    tmp = tmpfile();
    fputs(str, tmp);
    rewind(tmp);

    port = minim_file_port(tmp, MINIM_PORT_MODE_READ |
                                MINIM_PORT_MODE_READY |
                                MINIM_PORT_MODE_OPEN);
    MINIM_PORT_NAME(port) = "string";

    // setup environment
    init_env(&env, NULL, NULL);
    set_default_print_params(&pp);
    exit_buf = GC_alloc_atomic(sizeof(jmp_buf));
    exit_handler = minim_jmp(exit_buf, NULL);
    if (setjmp(*exit_buf) == 0)
    {
        minim_error_handler = exit_handler;
        minim_exit_handler = exit_handler;
        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            ast = minim_parse_port(port, &err, 0);
            if (err)
            {
                const char *msg = "parsing failed";
                char *out = GC_alloc_atomic((strlen(msg) + 1) * sizeof(char));
                strcpy(out, msg);
                return out;
            }

            obj = eval_ast(env, ast);
        }
    }
    else
    {
        obj = MINIM_JUMP_VAL(exit_handler);
        if (MINIM_OBJ_ERRORP(obj))
        {
            print_minim_object(obj, env, &pp);
            printf("\n");
            return NULL;
        }
    }

    return print_to_string(obj, env, &pp);
}
