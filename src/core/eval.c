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

static MinimObject *str_to_node(const char *str, MinimEnv *env, bool quote, bool err)
{
    if (is_rational(str))
    {
        return str_to_number(str, MINIM_OBJ_EXACT);
    }
    else if (is_float(str))
    {
        return str_to_number(str, MINIM_OBJ_INEXACT);
    }
    else if (is_char(str))
    {
        return minim_char(str[2]);
    }
    else if (is_str(str))
    {
        size_t len = strlen(str) - 1;
        char *tmp = GC_alloc_atomic(len * sizeof(char));

        strncpy(tmp, &str[1], len - 1);
        tmp[len - 1] = '\0';
        return minim_string(tmp);
    }
    else
    {
        MinimObject *res;

        if (quote)
        {
            res = env_get_sym(env, str);
            return intern(str);
        }
        else
        {
            res = env_get_sym(env, str);

            if (!res)
            {
                if (err)    THROW(env, minim_error("unrecognized symbol", str));
                else        return NULL;
            }   
            else if (MINIM_OBJ_SYNTAXP(res))
            {
                THROW(env, minim_error("bad syntax", str));
            }
            else if (MINIM_OBJ_TRANSFORMP(res))
            {
                THROW(env, minim_error("bad transform", str));
            }

            return res;
        }
    }
}

// Unsyntax

#define UNSYNTAX_REC            0x1
#define UNSYNTAX_QUASIQUOTE     0x2

static MinimObject *unsyntax_ast_node(MinimEnv *env, MinimObject* node, uint8_t flags)
{
    MinimObject *res;

    if (node->type == SYNTAX_NODE_LIST)
    {
        MinimObject **args, *proc;

        if (node->childc > 2 && node->children[node->childc - 2]->sym &&
            strcmp(node->children[node->childc - 2]->sym, ".") == 0)
        {
            MinimObject *rest;
            size_t reduc = node->childc - 2;

            args = GC_alloc(reduc * sizeof(MinimObject*));
            for (size_t i = 0; i < reduc; ++i)
                args[i] = ((flags & UNSYNTAX_REC) ?
                            unsyntax_ast_node(env, node->children[i], flags) :
                            minim_ast(node->children[i]));

            res = minim_builtin_list(env, reduc, args);
            for (rest = res; !minim_nullp(MINIM_CDR(rest)); rest = MINIM_CDR(rest));
            
            MINIM_CDR(rest) = ((flags & UNSYNTAX_REC) ?
                                unsyntax_ast_node(env, node->children[node->childc - 1], flags) :
                                minim_ast(node->children[node->childc - 1]));

            return res;
        }

        if (flags & UNSYNTAX_QUASIQUOTE && node->children[0]->sym)
        {
            proc = env_get_sym(env, node->children[0]->sym);
            if (flags & UNSYNTAX_QUASIQUOTE && proc && MINIM_SYNTAX(proc) == minim_builtin_unquote)
                return eval_ast_no_check(env, node->children[1]);
        }

        args = GC_alloc(node->childc * sizeof(MinimObject*));
        for (size_t i = 0; i < node->childc; ++i)
            args[i] = ((flags & UNSYNTAX_REC) ?
                        unsyntax_ast_node(env, node->children[i], flags) :
                        minim_ast(node->children[i]));


        res = minim_builtin_list(env, node->childc, args);
    }
    else if (node->type == SYNTAX_NODE_VECTOR)
    {
        MinimObject **args;

        args = GC_alloc(node->childc * sizeof(MinimObject*));
        for (size_t i = 0; i < node->childc; ++i)
            args[i] = ((flags & UNSYNTAX_REC) ?
                        unsyntax_ast_node(env, node->children[i], flags) :
                        minim_ast(node->children[i]));


        res = minim_builtin_vector(env, node->childc, args);
    }
    else if (node->type == SYNTAX_NODE_PAIR)
    {
        MinimObject **args = GC_alloc(2 * sizeof(MinimObject*));

        if (flags & UNSYNTAX_REC)
        {
            args[0] = unsyntax_ast_node(env, node->children[0], flags);
            args[1] = unsyntax_ast_node(env, node->children[1], flags);
        }
        else
        {
            args[0] = minim_ast(node->children[0]);
            args[1] = minim_ast(node->children[1]);
        }

        res = minim_builtin_cons(env, 2, args);
    }
    else
    {
        res = str_to_node(node->sym, env, true, true);
    }

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
MinimObject *eval_builtin_0ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    MinimObject *err;
    CALL_BUILTIN(env, proc, 0, NULL, &err);
}

// optimized version for builtins - 1ary
MinimObject *eval_builtin_1ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    MinimObject *arg, *err;
    uint8_t prev_flags = env->flags;

    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    arg = eval_ast_node(env, node->children[1]);
    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, 1, &arg, &err);
}

// optimized version for builtins - 2ary
MinimObject *eval_builtin_2ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    MinimObject *args[2];
    MinimObject *err;
    uint8_t prev_flags = env->flags;

    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, node->children[1]);
    args[1] = eval_ast_node(env, node->children[2]);
    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, 2, args, &err);
}

// optimized version for builtins - 3ary
MinimObject *eval_builtin_3ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    MinimObject *args[3];
    MinimObject *err;
    uint8_t prev_flags = env->flags;

    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, node->children[1]);
    args[1] = eval_ast_node(env, node->children[2]);
    args[2] = eval_ast_node(env, node->children[3]);
    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, 3, args, &err);
}

// general version for builtins
MinimObject *eval_builtin(MinimEnv *env, MinimObject *node, MinimBuiltin proc, size_t argc)
{
    MinimObject **args;
    MinimObject *err;
    uint8_t prev_flags = env->flags;

    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
        args[i] = eval_ast_node(env, node->children[i + 1]);
    env->flags = prev_flags;
    CALL_BUILTIN(env, proc, argc, args, &err);
}

// optimized version for syntax - 0ary
MinimObject *eval_syntax_0ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    return proc(env, 0, NULL);
}

// optimized version for syntax - 1ary
MinimObject *eval_syntax_1ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    MinimObject *arg = minim_ast(node->children[1]);
    return proc(env, 1, &arg);
}

// optimized version for syntax - 2ary
MinimObject *eval_syntax_2ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    MinimObject *args[2];
    args[0] = minim_ast(node->children[1]);
    args[1] = minim_ast(node->children[2]);
    return proc(env, 2, args);
}

// optimized version for syntax - 3ary
MinimObject *eval_syntax_3ary(MinimEnv *env, MinimObject *node, MinimBuiltin proc)
{
    MinimObject *args[3];
    args[0] = minim_ast(node->children[1]);
    args[1] = minim_ast(node->children[2]);
    args[2] = minim_ast(node->children[3]);
    return proc(env, 3, args);
}

// general version for syntax
MinimObject *eval_syntax(MinimEnv *env, MinimObject *node, MinimBuiltin proc, size_t argc)
{
    MinimObject **args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
        args[i] = minim_ast(node->children[i + 1]);
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
static MinimObject *eval_closure_0ary(MinimEnv *env, MinimObject *node, MinimLambda *lam)
{
    CALL_CLOSURE(env, lam, 0, NULL);
}

// optimized version for closures - 1ary
static MinimObject *eval_closure_1ary(MinimEnv *env, MinimObject *node, MinimLambda *lam)
{
    MinimObject *arg;
    uint8_t prev_flags;
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    arg = eval_ast_node(env, node->children[1]); 
    env->flags = prev_flags;
    CALL_CLOSURE(env, lam, 1, &arg);
}

// optimized version for closures - 2ary
static MinimObject *eval_closure_2ary(MinimEnv *env, MinimObject *node, MinimLambda *lam)
{
    MinimObject *args[2];
    uint8_t prev_flags;
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, node->children[1]);
    args[1] = eval_ast_node(env, node->children[2]); 
    env->flags = prev_flags;

    CALL_CLOSURE(env, lam, 2, args);
}

// optimized version for closures - 3ary
static MinimObject *eval_closure_3ary(MinimEnv *env, MinimObject *node, MinimLambda *lam)
{
    MinimObject *args[3];
    uint8_t prev_flags;
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args[0] = eval_ast_node(env, node->children[1]);
    args[1] = eval_ast_node(env, node->children[2]);
    args[2] = eval_ast_node(env, node->children[3]); 
    env->flags = prev_flags;
 
    CALL_CLOSURE(env, lam, 3, args);
}

// general version for closures
static MinimObject *eval_closure(MinimEnv *env, MinimObject *node, MinimLambda *lam, size_t argc)
{
    MinimObject **args;
    uint8_t prev_flags;
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
        args[i] = eval_ast_node(env, node->children[i + 1]);
    env->flags = prev_flags;
 
    CALL_CLOSURE(env, lam, argc, args);
}

static MinimObject *eval_jump(MinimEnv *env, MinimObject *node, size_t argc, MinimObject *jmp)
{
    MinimObject **args;

    args = GC_alloc(node->childc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)
        args[i] = eval_ast_node(env, node->children[i + 1]); 
    minim_long_jump(jmp, env, argc, args);
}

static MinimObject *eval_vector(MinimEnv *env, MinimObject *node)
{
    MinimObject **args;

    args = GC_alloc(node->childc * sizeof(MinimObject*));
    for (size_t i = 0; i < node->childc; ++i)
        args[i] = eval_ast_node(env, node->children[i]);
    return minim_vector(node->childc, args);
}

static MinimObject *eval_pair(MinimEnv *env, MinimObject *node)
{
    return minim_cons(eval_ast_node(env, node->children[0]),
                      eval_ast_node(env, node->children[1]));
}

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, MinimObject *node)
{
    if (node->type == SYNTAX_NODE_LIST)
    {
        MinimObject *res, *op;
        size_t argc;

        if (node->childc == 0)
            THROW(env, minim_error("missing procedure expression", NULL));

        argc = node->childc - 1;
        if (node->children[0]->type == SYNTAX_NODE_LIST)
            op = eval_ast_node(env, node->children[0]);
        else if (node->children[0]->type == SYNTAX_NODE_DATUM)
            op = env_get_sym(env, node->children[0]->sym);
        else
            THROW(env, minim_error("not a procedure", NULL));

        if (!op)
            THROW(env, minim_error("unknown operator", node->children[0]->sym));

        if (MINIM_OBJ_BUILTINP(op))
        {
            MinimBuiltin proc = MINIM_BUILTIN(op);

            // do not optimize: array must be allocaed
            if (proc == minim_builtin_values ||
                proc == minim_builtin_vector)
                return eval_builtin(env, node, proc, argc);

            if (argc == 0)          return eval_builtin_0ary(env, node, proc);
            else if (argc == 1)     return eval_builtin_1ary(env, node, proc);
            else if (argc == 2)     return eval_builtin_2ary(env, node, proc);
            else if (argc == 3)     return eval_builtin_3ary(env, node, proc);
            else                    return eval_builtin(env, node, proc, argc);
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimBuiltin proc = MINIM_SYNTAX(op);

            if (proc == minim_builtin_unquote)
                THROW(env, minim_error("not in a quasiquote", "unquote"));
            
            if (proc == minim_builtin_def_syntaxes ||
                proc == minim_builtin_import ||
                proc == minim_builtin_export)
                THROW(env, minim_error("only allowed at the top-level", node->children[0]->sym));

            if (argc == 0)          res = eval_syntax_0ary(env, node, proc);
            else if (argc == 1)     res = eval_syntax_1ary(env, node, proc);
            else if (argc == 2)     res = eval_syntax_2ary(env, node, proc);
            else if (argc == 3)     res = eval_syntax_3ary(env, node, proc);
            else                    res = eval_syntax(env, node, proc, argc);        

            if (MINIM_OBJ_CLOSUREP(res))
            {
                MinimLambda *lam = MINIM_CLOSURE(res);
                if (!lam->loc && node->children[0]->loc)
                    copy_syntax_loc(&lam->loc, node->children[0]->loc);
            }

            return res;
        }
        else if (MINIM_OBJ_CLOSUREP(op))
        {
            if (argc == 0)          return eval_closure_0ary(env, node, MINIM_CLOSURE(op));
            else if (argc == 1)     return eval_closure_1ary(env, node, MINIM_CLOSURE(op));
            else if (argc == 2)     return eval_closure_2ary(env, node, MINIM_CLOSURE(op));
            else if (argc == 3)     return eval_closure_3ary(env, node, MINIM_CLOSURE(op));
            else                    return eval_closure(env, node, MINIM_CLOSURE(op), argc);
        }
        else if (MINIM_OBJ_JUMPP(op))
        {
            return eval_jump(env, node, argc, op);
        }
        else
        {
            THROW(env, minim_error("unknown operator", node->children[0]->sym));
        }
    }
    else if (node->type == SYNTAX_NODE_VECTOR)
    {
        return eval_vector(env, node);
    }
    else if (node->type == SYNTAX_NODE_PAIR)
    {
        return eval_pair(env, node);
    }
    else
    {
        return str_to_node(node->sym, env, false, true);
    }
}

static MinimObject *eval_top_level(MinimEnv *env, MinimObject *ast, MinimBuiltin fn)
{
    MinimObject **args;
    size_t argc;

    // transcription from eval_ast_node
    argc = ast->childc - 1;
    args = GC_alloc(argc * sizeof(MinimObject*));
    for (size_t i = 0; i < argc; ++i)                   // initialize ast wrappers
        args[i] = minim_ast(ast->children[i + 1]);
    
    return fn(env, argc, args);
}

static bool expr_is_module_level(MinimEnv *env, MinimObject *ast)
{
    MinimObject *val;
    MinimBuiltin builtin;

    if (ast->type != SYNTAX_NODE_LIST || !ast->children[0]->sym)
        return false;

    val = env_get_sym(env, ast->children[0]->sym);
    if (!val || !MINIM_OBJ_SYNTAXP(val))
        return false;

    builtin = MINIM_BUILTIN(val);
    return (builtin == minim_builtin_def_syntaxes ||
            builtin == minim_builtin_import ||
            builtin == minim_builtin_export);
}

static bool expr_is_macro(MinimEnv *env, MinimObject *ast)
{
    MinimObject *val;

    if (ast->type != SYNTAX_NODE_LIST || !ast->children[0]->sym)
        return false;

    val = env_get_sym(env, ast->children[0]->sym);
    return (val && MINIM_OBJ_SYNTAXP(val) && MINIM_SYNTAX(val) == minim_builtin_def_syntaxes);
}

static bool expr_is_import(MinimEnv *env, MinimObject *ast)
{
    MinimObject *val;

    if (ast->type != SYNTAX_NODE_LIST || !ast->children[0]->sym)
        return false;

    val = env_get_sym(env, ast->children[0]->sym);
    return (val && MINIM_OBJ_SYNTAXP(val) && MINIM_SYNTAX(val) == minim_builtin_import);
}

static bool expr_is_export(MinimEnv *env, MinimObject *ast)
{
    MinimObject *val;

    if (ast->type != SYNTAX_NODE_LIST || !ast->children[0]->sym)
        return false;

    val = env_get_sym(env, ast->children[0]->sym);
    return (val && MINIM_OBJ_SYNTAXP(val) && MINIM_SYNTAX(val) == minim_builtin_export);
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

MinimObject *eval_ast_terminal(MinimEnv *env, MinimObject *ast)
{
    return str_to_node(ast->sym, env, false, false);
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

        if (expr_is_macro(module->env, module->exprs[i]))
            eval_top_level(module->env, module->exprs[i], minim_builtin_def_syntaxes);
    }

    // apply transforms
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_module_level(module->env, module->exprs[i]))
            continue;

        module->exprs[i] = transform_syntax(module->env, module->exprs[i]);
    }

    // constant fold
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_module_level(module->env, module->exprs[i]))
            continue;

        module->exprs[i] = constant_fold(module->env, module->exprs[i]);
    }
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

        // printf("eval: "); print_ast(module->exprs[i]); printf("\n");

        check_syntax(module->env, module->exprs[i]);
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

MinimObject *unsyntax_ast(MinimEnv *env, MinimObject *ast)
{
    return unsyntax_ast_node(env, ast, 0);
}

MinimObject *unsyntax_ast_rec(MinimEnv *env, MinimObject *ast)
{
    return unsyntax_ast_node(env, ast, UNSYNTAX_REC);
}

MinimObject *unsyntax_ast_rec2(MinimEnv *env, MinimObject *ast)
{
    return unsyntax_ast_node(env, ast, UNSYNTAX_REC | UNSYNTAX_QUASIQUOTE);
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
