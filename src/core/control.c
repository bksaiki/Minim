#include <string.h>

#include "../gc/gc.h"
#include "ast.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "lambda.h"
#include "list.h"
#include "tail_call.h"

MinimObject *minim_builtin_if(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    eval_ast_no_check(env, args[0]->u.ptrs.p1, &cond);
    if (!MINIM_OBJ_THROWNP(cond))
    {
        eval_ast_no_check(env, coerce_into_bool(cond) ?
                      args[1]->u.ptrs.p1 :
                      args[2]->u.ptrs.p1, &res);
    }
    else
    {
        res = cond;
    }

    return res;
}

MinimObject *minim_builtin_cond(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimEnv *env2;

    for (size_t i = 0; i < argc; ++i)
    {
        MinimObject *ce_pair, *cond, *val;

        unsyntax_ast(env, args[i]->u.ptrs.p1, &ce_pair);
        eval_ast_no_check(env, MINIM_DATA(MINIM_CAR(ce_pair)), &cond);
        if (coerce_into_bool(cond))
        {
            if (minim_list_length(ce_pair) > 2)
            {
                init_env(&env2, env, NULL);
                for (MinimObject *it = MINIM_CDR(ce_pair); it; it = MINIM_CDR(it))
                {
                    eval_ast_no_check(env2, MINIM_CAR(it)->u.ptrs.p1, &val);
                    if (MINIM_OBJ_THROWNP(val) || !MINIM_CDR(it))
                        return val;   
                }
            }
            else
            {
                eval_ast_no_check(env, MINIM_CADR(ce_pair)->u.ptrs.p1, &res);
                return res;
            }
        }
    }

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_unless(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    eval_ast_no_check(env, args[0]->u.ptrs.p1, &cond);
    if (!MINIM_OBJ_THROWNP(cond))
    {
        if (!coerce_into_bool(cond))
            res = minim_builtin_begin(env, &args[1], argc - 1);
        else
            init_minim_object(&res, MINIM_OBJ_VOID);
    }
    else
    {
        res = cond;
    }

    return res;
}

MinimObject *minim_builtin_when(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    eval_ast_no_check(env, args[0]->u.ptrs.p1, &cond);
    if (!MINIM_OBJ_THROWNP(cond))
    {
        if (coerce_into_bool(cond))
            res = minim_builtin_begin(env, &args[1], argc - 1);
        else
            init_minim_object(&res, MINIM_OBJ_VOID);
    }
    else
    {
        res = cond;
    }

    return res;
}

MinimObject *minim_let_func(MinimEnv *env, MinimObject **args, size_t argc, bool alt)
{
    MinimObject *bindings, *name, *res, *it;
    MinimLambda *lam;
    MinimEnv *env2;
    size_t len;
    bool err;

    // Get function name
    unsyntax_ast(env, MINIM_DATA(args[0]), &name);
    if (MINIM_OBJ_THROWNP(name))
        return name;

    // Convert bindings to list
    unsyntax_ast(env, MINIM_DATA(args[1]), &bindings);
    if (MINIM_OBJ_THROWNP(bindings))
        return bindings;

    len = minim_list_length(bindings);
    it = bindings;

    // Initialize lambda
    init_minim_lambda(&lam);
    lam->argc = len;
    lam->args = GC_alloc(lam->argc * sizeof(char*));

    // Initialize child environment
    init_env(&env2, env, lam);
    err = false;

    // Bind names and values
    for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym, *val;

        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &bind);
        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(bind)), &sym);
        
        eval_ast_no_check((alt ? env2 : env), MINIM_DATA(MINIM_CADR(bind)), &val);
        env_intern_sym(env2, MINIM_STRING(sym), val);

        lam->args[i] = GC_alloc_atomic((strlen(MINIM_STRING(sym)) + 1) * sizeof(char));
        strcpy(lam->args[i], MINIM_STRING(sym));
    }

    // Intern lambda
    copy_syntax_node(&lam->body, MINIM_DATA(args[2]));
    init_minim_object(&it, MINIM_OBJ_CLOSURE, lam);
    env_intern_sym(env2, MINIM_STRING(name), it);

    // Evaluate body
    eval_ast_no_check(env2, MINIM_DATA(args[2]), &res);
    if (MINIM_OBJ_TAIL_CALLP(res))
    {   
        MinimTailCall *call = MINIM_DATA(res);

        if (minim_lambda_equalp(call->lam, lam))
            return eval_lambda(lam, env, call->args, call->argc);
    }

    if (MINIM_OBJ_ERRORP(res) && lam->loc && lam->name)
        minim_error_add_trace(res->u.ptrs.p1, lam->loc, lam->name);
    return res;
}

MinimObject *minim_let_assign(MinimEnv *env, MinimObject **args, size_t argc, bool alt)
{
    MinimObject *bindings, *res, *it;
    MinimEnv *env2;
    size_t len;
    bool err;

    // Convert bindings to list
    unsyntax_ast(env, MINIM_DATA(args[0]), &bindings);
    if (MINIM_OBJ_THROWNP(bindings))
        return bindings;
    
    // Initialize child environment
    err = false;
    init_env(&env2, env, NULL);
    len = minim_list_length(bindings);
    it = bindings;

    // Bind names and values
    for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym, *val;

        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &bind);
        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(bind)), &sym);
        
        eval_ast_no_check((alt ? env2 : env), MINIM_DATA(MINIM_CADR(bind)), &val);
        env_intern_sym(env2, MINIM_STRING(sym), val);
    }

    // Evaluate body
    eval_ast_no_check(env2, MINIM_DATA(args[1]), &res);
    return res;
}

MinimObject *minim_builtin_let(MinimEnv *env, MinimObject **args, size_t argc)
{
    return (argc == 2) ?
           minim_let_assign(env, args, argc, false) :
           minim_let_func(env, args, argc, false);
}

MinimObject *minim_builtin_letstar(MinimEnv *env, MinimObject **args, size_t argc)
{
    return (argc == 2) ?
           minim_let_assign(env, args, argc, true) :
           minim_let_func(env, args, argc, true);
}

MinimObject *minim_builtin_begin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *val;
    MinimEnv *env2;

    if (argc == 0)
    {
        init_minim_object(&res, MINIM_OBJ_VOID);
        return res;
    }

    init_env(&env2, env, NULL);
    for (size_t i = 0; i < argc; ++i)
    {
        eval_ast_no_check(env2, args[i]->u.ptrs.p1, &val);
        if (MINIM_OBJ_THROWNP(val))
        {
            res = val;
            break;
        }

        if (i + 1 == argc)
            res = val;
    }

    return res;
}

MinimObject *minim_builtin_case(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *key;
    MinimEnv *env2;

    if (argc < 2)
    {
        init_minim_object(&res, MINIM_OBJ_VOID);
        return res;
    }
        

    unsyntax_ast(env, MINIM_DATA(args[0]), &key);
    for (size_t i = 1; i < argc; ++i)
    {
        MinimObject *ce_pair, *cs, *val;

        unsyntax_ast(env, MINIM_DATA(args[i]), &ce_pair);
        unsyntax_ast_rec(env, MINIM_DATA(MINIM_CAR(ce_pair)), &cs);
        if (minim_nullp(cs))
            continue;

        for (MinimObject *it = cs; it; it = MINIM_CDR(it))
        {
            if (minim_equalp(MINIM_CAR(it), key))
            {
                if (minim_list_length(ce_pair) > 2)
                {
                    init_env(&env2, env, NULL);
                    for (MinimObject *it = MINIM_CDR(ce_pair); it; it = MINIM_CDR(it))
                    {
                        eval_ast_no_check(env2, MINIM_DATA(MINIM_CAR(it)), &val);
                        if (MINIM_OBJ_THROWNP(val) || !MINIM_CDR(it))
                            return val;   
                    }
                }
                else
                {
                    eval_ast_no_check(env, MINIM_CADR(ce_pair)->u.ptrs.p1, &res);
                    return res;
                }
            }
        }
    }

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}
