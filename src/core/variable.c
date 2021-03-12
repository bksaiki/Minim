#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/read.h"
#include "assert.h"
#include "ast.h"
#include "builtin.h"
#include "bool.h"
#include "eval.h"
#include "list.h"
#include "number.h"
#include "variable.h"

bool assert_symbol(MinimObject *obj, MinimObject **res, const char* msg)
{
    if (!MINIM_OBJ_SYMBOLP(obj))
    {
        minim_error(res, "%s", msg);
        return false;
    }

    return true;
}

MinimObject *minim_builtin_if(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    if (assert_exact_argc(&res, "if", 3, argc))
    {
        eval_ast(env, args[0]->u.ptrs.p1, &cond);

        if (!MINIM_OBJ_ERRORP(cond))
        {
            if (coerce_into_bool(cond))     eval_ast(env, args[1]->u.ptrs.p1, &res);
            else                            eval_ast(env, args[2]->u.ptrs.p1, &res);

            free_minim_object(cond);
        }
        else
        {
            res = cond;
        }   
    }

    return res;
}

MinimObject *minim_builtin_cond(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    bool eval = false;

    for (size_t i = 0; !eval && i < argc; ++i)
    {
        MinimObject *ce_pair, *cond, *val;

        unsyntax_ast(env, args[i]->u.ptrs.p1, &ce_pair);
        if (assert_list(ce_pair, &res, "Expected [<cond> <expr>] pair") &&
            assert_generic(&res, "Expected [<cond> <expr>] pair", minim_list_length(ce_pair) >= 2))
        {
            MinimAst *cond_syn = MINIM_CAR(ce_pair)->u.ptrs.p1;
            if (assert_generic(&res, "'else' clause must be last", i + 1 == argc ||
                !cond_syn->sym || strcmp(cond_syn->sym, "else") != 0))
            {
                eval_ast(env, cond_syn, &cond);
                if (coerce_into_bool(cond))
                {
                    if (minim_list_length(ce_pair) > 2)
                    {
                        MinimEnv *env2;

                        init_env(&env2, env);
                        for (MinimObject *it = MINIM_CDR(ce_pair); it; it = MINIM_CDR(it))
                        {
                            eval_ast(env2, MINIM_CAR(it)->u.ptrs.p1, &val);
                            if (MINIM_OBJ_ERRORP(val))
                            {
                                res = val;
                                break;
                            }

                            if (MINIM_CDR(it))  free_minim_object(val);
                            else                res = fresh_minim_object(val);               
                        }

                        RELEASE_IF_REF(val);
                        pop_env(env2);
                        eval = true;
                    }
                    else
                    {
                        eval_ast(env, MINIM_CADR(ce_pair)->u.ptrs.p1, &val);
                        res = fresh_minim_object(val);
                        RELEASE_IF_REF(val);
                        eval = true;
                    }
                }

                free_minim_object(cond);
            }
            else
            {
                eval = true;
            }
        }
        else
        {
            eval = true;
        }
    
        free_minim_object(ce_pair);
    }

    if (argc == 0 || !eval)
        init_minim_object(&res, MINIM_OBJ_VOID);

    return res;
}

MinimObject *minim_builtin_unless(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    if (assert_min_argc(&res, "unless", 2, argc))
    {
        eval_ast(env, args[0]->u.ptrs.p1, &cond);
        if (!MINIM_OBJ_ERRORP(cond))
        {
            if (!coerce_into_bool(cond))
                res = minim_builtin_begin(env, &args[1], argc - 1);
            else
                init_minim_object(&res, MINIM_OBJ_VOID);

            free_minim_object(cond);
        }
        else
        {
            res = cond;
        }
    }

    return res;
}

MinimObject *minim_builtin_when(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *cond;

    if (assert_min_argc(&res, "unless", 2, argc))
    {
        eval_ast(env, args[0]->u.ptrs.p1, &cond);
        if (!MINIM_OBJ_ERRORP(cond))
        {
            if (coerce_into_bool(cond))
                res = minim_builtin_begin(env, &args[1], argc - 1);
            else
                init_minim_object(&res, MINIM_OBJ_VOID);

            free_minim_object(cond);
        }
        else
        {
            res = cond;
        }
    }

    return res;
}

MinimObject *minim_builtin_def(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *sym, *val;

    if (assert_min_argc(&res, "def", 2, argc))
    {
        unsyntax_ast(env, args[0]->u.ptrs.p1, &sym);
        if (assert_symbol(sym, &res, "Expected a symbol in the 1st argument of 'def'"))
        {
            if (argc == 2)
            {
                eval_ast(env, args[1]->u.ptrs.p1, &val);
            }
            else
            {
                MinimAst *ast = args[0]->u.ptrs.p1;
                MinimLambda *lam;
                
                val = minim_builtin_lambda(env, &args[1], argc - 1);
                
                lam = val->u.ptrs.p1;
                copy_syntax_loc(&lam->loc, ast->loc);
            }
            
            if (!MINIM_OBJ_ERRORP(val))
            {
                env_intern_sym(env, ((char*) sym->u.str.str), val);
                init_minim_object(&res, MINIM_OBJ_VOID);
                RELEASE_IF_REF(val);
            }
            else
            {
                res = val;
            }
        }
        
        free_minim_object(sym);
    }

    return res;
}

MinimObject *minim_builtin_let(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *bindings, *res;
    MinimEnv *env2;

    if (assert_exact_argc(&res, "let", 2, argc))
    {
        MinimObject *it;
        size_t len;
        bool err = false;
        
        // Convert bindings to list
        unsyntax_ast(env, args[0]->u.ptrs.p1, &bindings);
        if (MINIM_OBJ_ERRORP(bindings))
        {
            res = bindings;
            return res;
        }
    
        len = minim_list_length(bindings);
        it = bindings;
        
        // Initialize child environment
        init_env(&env2, env);
        for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
        {
            MinimObject *bind, *sym, *val;

            unsyntax_ast(env, MINIM_CAR(it)->u.ptrs.p1, &bind);
            if (assert_list(bind, &res, "Expected ((symbol value) ...) in the bindings of 'let'") &&
                assert_list_len(bind, &res, 2, "Expected ((symbol value) ...) in the bindings of 'let'"))
            {
                unsyntax_ast(env, MINIM_CAR(bind)->u.ptrs.p1, &sym);
                if (assert_symbol(sym, &res, "Variable names must be symbols in let"))
                {
                    eval_ast(env, MINIM_CADR(bind)->u.ptrs.p1, &val);
                    env_intern_sym(env2, sym->u.str.str, val);
                    RELEASE_IF_REF(val);
                }
                else
                {
                    err = true;
                }

                free_minim_object(sym);
            }
            else
            {
                err = true;
            }

            free_minim_object(bind);
        }

        if (!err) eval_ast(env2, args[1]->u.ptrs.p1, &it);
        res = fresh_minim_object(it);
        RELEASE_IF_REF(it);

        free_minim_object(bindings);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_letstar(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *bindings, *res;
    MinimEnv *env2;

    if (assert_exact_argc(&res, "let*", 2, argc))
    {
        MinimObject *it;
        size_t len;
        bool err = false;
        
        // Convert bindings to list
        unsyntax_ast(env, args[0]->u.ptrs.p1, &bindings);
        len = minim_list_length(bindings);
        it = bindings;
        
        // Initialize child environment
        init_env(&env2, env);
        for (size_t i = 0; !err && i < len; ++i, it = MINIM_CDR(it))
        {
            MinimObject *bind, *sym, *val;

            unsyntax_ast(env, MINIM_CAR(it)->u.ptrs.p1, &bind);
            if (assert_list(bind, &res, "Expected ((symbol value) ...) in the bindings of 'let*'") &&
                assert_list_len(bind, &res, 2, "Expected ((symbol value) ...) in the bindings of 'let*'"))
            {
                unsyntax_ast(env, MINIM_CAR(bind)->u.ptrs.p1, &sym);
                if (assert_symbol(sym, &res, "Variable names must be symbols in let*"))
                {
                    eval_ast(env2, MINIM_CADR(bind)->u.ptrs.p1, &val);
                    env_intern_sym(env2, sym->u.str.str, val);
                    RELEASE_IF_REF(val);
                }
                else
                {
                    err = true;
                }

                free_minim_object(sym);
            }
            else
            {
                err = true;
            }

            free_minim_object(bind);
        }

        if (!err) eval_ast(env2, args[1]->u.ptrs.p1, &it);
        res = fresh_minim_object(it);
        RELEASE_IF_REF(it);

        free_minim_object(bindings);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_quote(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "quote", 1, argc))
        unsyntax_ast_rec(env, args[0]->u.ptrs.p1, &res);
    return res;
}

MinimObject *minim_builtin_setb(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *sym;

    if (assert_exact_argc(&res, "set!", 2, argc))
    {
        unsyntax_ast(env, args[0]->u.ptrs.p1, &sym);
        if (assert_symbol(sym, &res, "Expected a symbol in the 1st argument of 'set!'"))
        {
            MinimObject *val, *peek = env_peek_sym(env, sym->u.ptrs.p1);
            if (peek)
            {
                eval_ast(env, args[1]->u.ptrs.p1, &val);
                if (!MINIM_OBJ_ERRORP(val))
                {
                    env_set_sym(env, sym->u.str.str, val);
                    init_minim_object(&res, MINIM_OBJ_VOID);
                }
                else
                {
                    res = val;
                }
            }
            else
            {
                minim_error(&res, "Variable not recognized '%s'", sym->u.ptrs.p1);
            }
        }

        free_minim_object(sym);
    }

    return res;
}

MinimObject *minim_builtin_begin(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "begin", 1, argc))
    {
        MinimObject *val;
        MinimEnv *env2;

        init_env(&env2, env);
        for (size_t i = 0; i < argc; ++i)
        {
            eval_ast(env2, args[i]->u.ptrs.p1, &val);
            if (MINIM_OBJ_ERRORP(val))
            {
                res = val;
                break;
            }

            if (i + 1 == argc)      res = fresh_minim_object(val);
            else                    free_minim_object(val);
        }

        if (!MINIM_OBJ_OWNERP(val)) free_minim_object(val);
        pop_env(env2);
    }

    return res;
}

MinimObject *minim_builtin_symbolp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "symbol?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_SYMBOLP(args[0]));
        
    return res;
}

MinimObject *minim_builtin_equalp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "equal?", 1, argc))
    {
        for (size_t i = 1; i < argc; ++i)
        {
            if (!minim_equalp(args[i - 1], args[i]))
            {
                init_minim_object(&res, MINIM_OBJ_BOOL, 0);
                return res;
            }
        }

        init_minim_object(&res, MINIM_OBJ_BOOL, 1);
    }

    return res;
}

MinimObject *minim_builtin_version(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    char *str;

    if (!assert_exact_argc(&res, "version", 0, argc))
        return res;

    str = malloc((strlen(MINIM_VERSION_STR) + 1) * sizeof(char));
    strcpy(str, MINIM_VERSION_STR);
    init_minim_object(&res, MINIM_OBJ_STRING, str);

    return res;
}

MinimObject *minim_builtin_symbol_count(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;

    if (!assert_exact_argc(&res, "symbol-count", 0, argc))
        return res;

    init_minim_number(&num, MINIM_NUMBER_EXACT);
    mpq_set_ui(num->rat, env->table->size, 1);
    init_minim_object(&res, MINIM_OBJ_NUM, num);
    
    return res;
}