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

static MinimObject *unsyntax_ast_node(MinimEnv *env, SyntaxNode* node, uint8_t flags)
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

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, SyntaxNode *node)
{
    if (node->type == SYNTAX_NODE_LIST)
    {
        MinimObject *res, *op, **args;
        size_t argc;

        if (node->childc == 0)
            THROW(env, minim_error("missing procedure expression", NULL));

        argc = node->childc - 1;
        args = GC_alloc(argc * sizeof(MinimObject*));
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
            uint8_t prev_flags = env->flags;

            env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);
            env->flags = prev_flags;       

            if (!minim_check_arity(proc, argc, env, &res))
                THROW(env, res);

            log_proc_called();
            res = proc(env, argc, args);
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimBuiltin proc = MINIM_BUILTIN(op);

            if (proc == minim_builtin_unquote)
                THROW(env, minim_error("not in a quasiquote", "unquote"));
            
            if (proc == minim_builtin_def_syntaxes ||
                proc == minim_builtin_import ||
                proc == minim_builtin_export)
                THROW(env, minim_error("only allowed at the top-level", node->children[0]->sym));

            for (size_t i = 0; i < argc; ++i)                   // initialize ast wrappers
                args[i] = minim_ast(node->children[i + 1]);
            
            res = proc(env, argc, args);
            if (MINIM_OBJ_CLOSUREP(res))
            {
                MinimLambda *lam = MINIM_CLOSURE(res);
                if (!lam->loc && node->children[0]->loc)
                    copy_syntax_loc(&lam->loc, node->children[0]->loc);
            }
        }
        else if (MINIM_OBJ_CLOSUREP(op))
        {
            MinimLambda *lam = MINIM_CLOSURE(op);
            uint8_t prev_flags = env->flags;
            
            env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]); 
            env->flags = prev_flags;     

            if (env_has_called(env, lam))
            {
                MinimTailCall *call;

                init_minim_tail_call(&call, lam, argc, args);
                res = minim_tail_call(call);
            }
            else
            {
                log_proc_called();
                res = eval_lambda(lam, env, argc, args);
            }
        }
        else if (MINIM_OBJ_JUMPP(op))
        {
            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]); 

            // no return
            minim_long_jump(op, env, argc, args);
        }
        else
        {
            THROW(env, minim_error("unknown operator", node->children[0]->sym));
        }

        return res;
    }
    else if (node->type == SYNTAX_NODE_VECTOR)
    {
        MinimObject **args;

        args = GC_alloc(node->childc * sizeof(MinimObject*));
        for (size_t i = 0; i < node->childc; ++i)
            args[i] = eval_ast_node(env, node->children[i]);

        return minim_builtin_vector(env, node->childc, args);
    }
    else if (node->type == SYNTAX_NODE_PAIR)
    {
        MinimObject **args;

        args = GC_alloc(2 * sizeof(MinimObject*));
        args[0] = eval_ast_node(env, node->children[0]);
        args[1] = eval_ast_node(env, node->children[1]);

        return minim_builtin_cons(env, 2, args);
    }
    else
    {
        return str_to_node(node->sym, env, false, true);
    }
}

static MinimObject *eval_top_level(MinimEnv *env, SyntaxNode *ast, MinimBuiltin fn)
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

static bool expr_is_module_level(MinimEnv *env, SyntaxNode *ast)
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

static bool expr_is_macro(MinimEnv *env, SyntaxNode *ast)
{
    MinimObject *val;

    if (ast->type != SYNTAX_NODE_LIST || !ast->children[0]->sym)
        return false;

    val = env_get_sym(env, ast->children[0]->sym);
    return (val && MINIM_OBJ_SYNTAXP(val) && MINIM_SYNTAX(val) == minim_builtin_def_syntaxes);
}

static bool expr_is_import(MinimEnv *env, SyntaxNode *ast)
{
    MinimObject *val;

    if (ast->type != SYNTAX_NODE_LIST || !ast->children[0]->sym)
        return false;

    val = env_get_sym(env, ast->children[0]->sym);
    return (val && MINIM_OBJ_SYNTAXP(val) && MINIM_SYNTAX(val) == minim_builtin_import);
}

static bool expr_is_export(MinimEnv *env, SyntaxNode *ast)
{
    MinimObject *val;

    if (ast->type != SYNTAX_NODE_LIST || !ast->children[0]->sym)
        return false;

    val = env_get_sym(env, ast->children[0]->sym);
    return (val && MINIM_OBJ_SYNTAXP(val) && MINIM_SYNTAX(val) == minim_builtin_export);
}

// ================================ Public ================================

MinimObject *eval_ast(MinimEnv *env, SyntaxNode *ast)
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

MinimObject *eval_ast_no_check(MinimEnv* env, SyntaxNode *ast)
{
    log_expr_evaled();
    return eval_ast_node(env, ast);
}

MinimObject *eval_ast_terminal(MinimEnv *env, SyntaxNode *ast)
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

MinimObject *unsyntax_ast(MinimEnv *env, SyntaxNode *ast)
{
    return unsyntax_ast_node(env, ast, 0);
}

MinimObject *unsyntax_ast_rec(MinimEnv *env, SyntaxNode *ast)
{
    return unsyntax_ast_node(env, ast, UNSYNTAX_REC);
}

MinimObject *unsyntax_ast_rec2(MinimEnv *env, SyntaxNode *ast)
{
    return unsyntax_ast_node(env, ast, UNSYNTAX_REC | UNSYNTAX_QUASIQUOTE);
}

char *eval_string(char *str, size_t len)
{
    PrintParams pp;
    MinimEnv *env;
    MinimObject *obj, *exit_handler, *port;
    SyntaxNode *ast, *err;
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
