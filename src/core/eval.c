#include <string.h>

#include "../common/path.h"
#include "../gc/gc.h"

#include "arity.h"
#include "builtin.h"
#include "eval.h"
#include "error.h"
#include "jmp.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "syntax.h"
#include "tail_call.h"
#include "transform.h"

static bool is_rational(char *str)
{
    char *it = str;

    if ((*it == '+' || *it == '-') &&
        (*(it + 1) >= '0' && *(it + 1) <= '9'))
    {
        it += 2;
    }

    while (*it >= '0' && *it <= '9')    ++it;

    if (*it == '/' && *(it + 1) >= '0' && *(it + 1) <= '9')
    {
        it += 2;
        while (*it >= '0' && *it <= '9')    ++it;
    }

    return (*it == '\0');
}

static bool is_float(const char *str)
{
    size_t idx = 0;
    bool digit = false;

    if (str[idx] == '+' || str[idx] == '-')
        ++idx;
    
    if (str[idx] >= '0' && str[idx] <= '9')
    {
        ++idx;
        digit = true;
    }
    
    while (str[idx] >= '0' && str[idx] <= '9')
        ++idx;

    if (str[idx] != '.' && str[idx] != 'e')
        return false;

    if (str[idx] == '.')
        ++idx;

    if (str[idx] >= '0' && str[idx] <= '9')
    {
        ++idx;
        digit = true;
    }

    while (str[idx] >= '0' && str[idx] <= '9')
        ++idx;

    if (str[idx] == 'e')
    {
        ++idx;
        if (!str[idx])  return false;

        if ((str[idx] == '+' || str[idx] == '-') &&
            str[idx + 1] >= '0' && str[idx + 1] <= '9')
            idx += 2;

        while (str[idx] >= '0' && str[idx] <= '9')
            ++idx;
    }

    return digit && !str[idx];
}

static bool is_char(char *str)
{
    return (str[0] == '#' && str[1] == '\\' && str[2] != '\0');
}

static bool is_str(char *str)
{
    size_t len = strlen(str);

    if (len < 2 || str[0] != '\"' || str[len - 1] != '\"')
        return false;

    for (size_t i = 1; i < len; ++i)
    {
        if (str[i] == '\"' && str[i - 1] != '\\' && i + 1 != len)
            return false;
    }

    return true;
}

static MinimObject *str_to_node(char *str, MinimEnv *env, bool quote)
{
    MinimObject *res;

    if (is_rational(str))
    {
        mpq_ptr rat = gc_alloc_mpq_ptr();

        mpq_set_str(rat, str, 0);
        mpq_canonicalize(rat);
        res = minim_exactnum(rat);
    }
    else if (is_float(str))
    {
        res = minim_inexactnum(strtod(str, NULL));
    }
    else if (is_char(str))
    {
        res = minim_char(str[2]);
    }
    else if (is_str(str))
    {
        size_t len = strlen(str) - 1;
        char *tmp = GC_alloc_atomic(len * sizeof(char));

        strncpy(tmp, &str[1], len - 1);
        tmp[len - 1] = '\0';
        res = minim_string(tmp);
    }
    else
    {
        if (quote)
        {
            res = minim_symbol(str);
        }
        else
        {
            res = env_get_sym(env, str);

            if (!res)
                return minim_error("unrecognized symbol", str);
            else if (MINIM_OBJ_SYNTAXP(res))
                return minim_error("bad syntax", str);
            else if (MINIM_OBJ_TRANSFORMP(res))
                return minim_error("bad transform", str);
        }
    }

    return res;
}

static MinimObject *error_or_exit(size_t argc, MinimObject **args)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (MINIM_OBJ_THROWNP(args[i]))
            return args[i];
    }

    return NULL;
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
            {
                eval_ast_no_check(env, node->children[1], &res);
                return res;
            }
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
        res = str_to_node(node->sym, env, true);
    }

    return res;
}

// Eval mainloop

static MinimObject *eval_ast_node(MinimEnv *env, SyntaxNode *node)
{
    if (node->type == SYNTAX_NODE_LIST)
    {
        MinimObject *res, *op, *possible_err, **args;
        size_t argc;

        if (node->childc == 0)
            return minim_error("missing procedure expression", NULL);

        argc = node->childc - 1;
        args = GC_alloc(argc * sizeof(MinimObject*));
        op = env_get_sym(env, node->children[0]->sym);

        if (!op)
        {
            res = minim_error("unknown operator", node->children[0]->sym);
            return res;
        }

        if (MINIM_OBJ_BUILTINP(op))
        {
            MinimBuiltin proc = MINIM_BUILTIN(op);
            uint8_t prev_flags = env->flags;

            env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]);
            env->flags = prev_flags;       

            possible_err = error_or_exit(argc, args);
            if (possible_err)
            {
                res = possible_err;
            }
            else
            {
                if (minim_check_arity(proc, argc, env, &res))
                    res = proc(env, argc, args);
            }
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimBuiltin proc = MINIM_BUILTIN(op);

            if (proc == minim_builtin_unquote)
            {
                res = minim_error("not in a quasiquote", "unquote");
            }
            else if (proc == minim_builtin_def_syntaxes ||
                     proc == minim_builtin_import ||
                     proc == minim_builtin_export)
            {
                res = minim_error("only allowed at the top-level", node->children[0]->sym);
            }
            else
            {
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
        }
        else if (MINIM_OBJ_CLOSUREP(op))
        {
            MinimLambda *lam = MINIM_CLOSURE(op);

            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]); 

            possible_err = error_or_exit(argc, args);
            if (possible_err)
            {
                res = possible_err;
            }
            else if (env_has_called(env, lam))
            {
                MinimTailCall *call;

                init_minim_tail_call(&call, lam, argc, args);
                res = minim_tail_call(call);
            }
            else
            {
                res = eval_lambda(lam, env, argc, args);
            }
        }
        else if (MINIM_OBJ_JUMPP(op))
        {
            for (size_t i = 0; i < argc; ++i)
                args[i] = eval_ast_node(env, node->children[i + 1]); 

            possible_err = error_or_exit(argc, args);
            if (possible_err)
            {
                res = possible_err;
            }
            else
            {
                // no return
                minim_long_jump(op, env, argc, args);
            }
        }
        else
        {
            res = minim_error("unknown operator", node->children[0]->sym);
        }

        return res;
    }
    else if (node->type == SYNTAX_NODE_VECTOR)
    {
        MinimObject **args;
        MinimObject *possible_err;

        args = GC_alloc(node->childc * sizeof(MinimObject*));
        for (size_t i = 0; i < node->childc; ++i)
            args[i] = eval_ast_node(env, node->children[i]);

        possible_err = error_or_exit(node->childc, args);
        return (possible_err) ? possible_err : minim_builtin_vector(env, node->childc, args);
    }
    else if (node->type == SYNTAX_NODE_PAIR)
    {
        MinimObject **args;
        MinimObject *possible_err;

        args = GC_alloc(2 * sizeof(MinimObject*));
        args[0] = eval_ast_node(env, node->children[0]);
        args[1] = eval_ast_node(env, node->children[1]);

        possible_err = error_or_exit(node->childc, args);
        return (possible_err) ? possible_err : minim_builtin_cons(env, 2, args);
    }
    else
    {
        return str_to_node(node->sym, env, false);
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

int eval_ast(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj)
{
    MinimEnv *env2;

    if (expr_is_export(env, ast))
    {
        *pobj = minim_error("%export not allowed in REPL", NULL);
        return 0;
    }

    init_env(&env2, env, NULL);
    if (expr_is_import(env, ast))
    {
        if (!check_syntax(env2, ast, pobj))
            return 0;

        *pobj = eval_top_level(env, ast, minim_builtin_import);
        if (MINIM_OBJ_ERRORP(*pobj))
            return 0;
    }
    else if (expr_is_macro(env, ast))
    {
        ast = transform_syntax(env, ast, pobj);
        if (*pobj)  return 0;

        *pobj = eval_top_level(env, ast, minim_builtin_def_syntaxes);
        if (MINIM_OBJ_ERRORP(*pobj))
            return 0;
    }
    else
    {
        if (!check_syntax(env2, ast, pobj))
            return 0;

        ast = transform_syntax(env, ast, pobj);
        if (*pobj)  return 0;

        *pobj = eval_ast_node(env, ast);
        if (MINIM_OBJ_ERRORP(*pobj))
            return 0;
    }

    return 1;
}

int eval_ast_no_check(MinimEnv* env, SyntaxNode *ast, MinimObject **pobj)
{
    *pobj = eval_ast_node(env, ast);
    return !MINIM_OBJ_ERRORP((*pobj));
}

int eval_module(MinimModule *module, MinimObject **pobj)
{
    PrintParams pp;
    MinimEnv *env2;

    // importing
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_import(module->env, module->exprs[i]))
        {
            if (!check_syntax(module->env, module->exprs[i], pobj))
                return 0;

            *pobj = eval_top_level(module->env, module->exprs[i], minim_builtin_import);
            if (MINIM_OBJ_ERRORP(*pobj))
                return 0;
        }
    }

    // syntax check / define transforms
    init_env(&env2, module->env, NULL);
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_import(module->env, module->exprs[i]))
            continue;

        if (expr_is_macro(module->env, module->exprs[i]))
        {
            module->exprs[i] = transform_syntax(module->env, module->exprs[i], pobj);
            if (*pobj)  return 0;

            *pobj = eval_top_level(module->env, module->exprs[i], minim_builtin_def_syntaxes);
            if (MINIM_OBJ_ERRORP(*pobj))
                return 0;
        }
    }

    // Syntax transform
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_module_level(module->env, module->exprs[i]))
            continue;

        module->exprs[i] = transform_syntax(module->env, module->exprs[i], pobj);
        if (*pobj)  return 0;

        if (expr_is_macro(module->env, module->exprs[i]))
        {
            *pobj = eval_top_level(module->env, module->exprs[i], minim_builtin_def_syntaxes);
            if (MINIM_OBJ_ERRORP(*pobj))
                return 0;
        }
    }

    // Evaluation
    set_default_print_params(&pp);
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (!check_syntax(env2, module->exprs[i], pobj))
            return 0;

        if (expr_is_module_level(module->env, module->exprs[i]))
            continue;

        eval_ast_no_check(module->env, module->exprs[i], pobj);
        if (MINIM_OBJ_ERRORP(*pobj))
        {    
            return 0;
        }
        else if (MINIM_OBJ_EXITP(*pobj))
        {
            *pobj = minim_void;
            return 1;
        }
        else if (!minim_voidp(*pobj))
        {
            print_minim_object(*pobj, module->env, &pp);
            printf("\n");
        }
    }

    // exports
    for (size_t i = 0; i < module->exprc; ++i)
    {
        if (expr_is_export(module->env, module->exprs[i]))
        {
            *pobj = eval_top_level(module->env, module->exprs[i], minim_builtin_export);
            if (MINIM_OBJ_ERRORP(*pobj))
                return 0;
        }
    }

    return 1;
}

int unsyntax_ast(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj)
{
    MinimObject *obj = unsyntax_ast_node(env, ast, 0);
    *pobj = obj;
    return !MINIM_OBJ_ERRORP(obj);
}

int unsyntax_ast_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj)
{
    MinimObject *obj = unsyntax_ast_node(env, ast, UNSYNTAX_REC);
    *pobj = obj;
    return !MINIM_OBJ_ERRORP(obj);
}

int unsyntax_ast_rec2(MinimEnv *env, SyntaxNode *ast, MinimObject **pobj)
{
    MinimObject *obj = unsyntax_ast_node(env, ast, UNSYNTAX_REC | UNSYNTAX_QUASIQUOTE);
    *pobj = obj;
    return !MINIM_OBJ_ERRORP(obj);
}

char *eval_string(char *str, size_t len)
{
    SyntaxNode *ast, *err;
    MinimObject *obj;
    MinimEnv *env;
    PrintParams pp;
    ReadTable rt;
    FILE *tmp;
    char *out;

    init_env(&env, NULL, NULL);
    minim_load_builtins(env);
    set_default_print_params(&pp);

    tmp = tmpfile();
    fputs(str, tmp);
    rewind(tmp);

    rt.idx = 0;
    rt.row = 1;
    rt.col = 0;
    rt.flags = 0x0;
    rt.eof = EOF;

    set_default_print_params(&pp);
    while (~rt.flags & READ_TABLE_FLAG_EOF)
    {
        minim_parse_port(tmp, "", &ast, &err, &rt);
        if (!ast || rt.flags & READ_TABLE_FLAG_BAD)
        {
            char *s = "Parsing failed!";
            out = GC_alloc_atomic((strlen(s) + 1) * sizeof(char));
            strcpy(out, s);

            fclose(tmp);
            return out;
        }

        eval_ast(env, ast, &obj);
        if (MINIM_OBJ_ERRORP(obj))
        {
            fclose(tmp);
            return print_to_string(obj, env, &pp);
        }
    }

    fclose(tmp);
    return print_to_string(obj, env, &pp);
}
