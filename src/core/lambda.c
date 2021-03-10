#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "error.h"
#include "eval.h"
#include "lambda.h"
#include "list.h"
#include "variable.h"

void init_minim_lambda(MinimLambda **plam)
{
    MinimLambda *lam = malloc(sizeof(MinimLambda));
    
    lam->loc = NULL;
    lam->env = NULL;
    lam->args = NULL;
    lam->rest = NULL;
    lam->body = NULL;
    lam->name = NULL;
    lam->argc = 0;

    *plam = lam;
}

void copy_minim_lambda(MinimLambda **cp, MinimLambda *src)
{
    MinimLambda *lam = malloc(sizeof(MinimLambda));

    lam->argc = src->argc;
    if (src->args)
    {
        lam->args = malloc(lam->argc * sizeof(char*));
        for (size_t i = 0; i < lam->argc; ++i)
        {
            lam->args[i] = malloc((strlen(src->args[i]) + 1) * sizeof(char));
            strcpy(lam->args[i], src->args[i]);
        }
    }   
    else
    {
        lam->args = NULL;
    }

    if (src->rest)
    {
        lam->rest = malloc((strlen(src->rest) + 1) * sizeof(char));
        strcpy(lam->rest, src->rest);
    }
    else
    {
        lam->rest = NULL;
    }

    if (src->name)
    {
        lam->name = malloc((strlen(src->name) + 1) * sizeof(char));
        strcpy(lam->name, src->name);
    }
    else
    {
        lam->name = NULL;
    }

    if (src->env)   rcopy_env(&lam->env, src->env);
    else            lam->env = NULL;

    if (src->loc)   copy_syntax_loc(&lam->loc, src->loc);
    else            lam->loc = NULL;

    if (src->body)  copy_ast(&lam->body, src->body);
    else            lam->body = NULL;

    *cp = lam;
}

void free_minim_lambda(MinimLambda *lam)
{
    if (lam->args)
    {
        for (size_t i = 0; i < lam->argc; ++i)
            free(lam->args[i]);
        free(lam->args);
    }

    if (lam->env && lam->env->copied) // avoid deleting top environment
        free_env(lam->env);

    if (lam->rest)  free(lam->rest);
    if (lam->name)  free(lam->name);
    if (lam->body)  free_ast(lam->body);
    if (lam->loc)   free_syntax_loc(lam->loc);

    free(lam);
}

MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *val;
    MinimEnv *env2;

    if (lam->rest || assert_exact_argc(&res, ((lam->name) ? lam->name : "unnamed lambda"), lam->argc, argc))
    {   
        init_env(&env2, lam->env);

        if (lam->name)
        {
            MinimLambda *clam;
            MinimObject *self;

            copy_minim_lambda(&clam, lam);
            init_minim_object(&self, MINIM_OBJ_CLOSURE, clam);
            env_intern_sym(env2, lam->name, self);
        }

        for (size_t i = 0; i < lam->argc; ++i)
        {
            copy_minim_object(&val, args[i]);
            env_intern_sym(env2, lam->args[i], val);
            RELEASE_IF_REF(val);
        }

        if (lam->rest)
        {
            MinimObject **rest;
            size_t rcount = argc - lam->argc;

            rest = malloc(rcount * sizeof(MinimObject*));
            for (size_t i = 0; i < rcount; ++i)
                copy_minim_object(&rest[i], args[lam->argc + i]);

            val = minim_list(rest, rcount);
            env_intern_sym(env2, lam->rest, val);
            free(rest);
        }

        eval_ast(env2, lam->body, &val);
        res = fresh_minim_object(val);
        RELEASE_IF_REF(val);
        pop_env(env2);

        if (res->type == MINIM_OBJ_ERR && lam->loc && lam->name)
            minim_error_add_trace(res->data, lam->loc, lam->name);
    }
    
    return res;
}

bool assert_func(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!minim_funcp(arg))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool minim_lambdap(MinimObject *thing)
{
    return thing->type == MINIM_OBJ_CLOSURE;
}

bool minim_funcp(MinimObject *thing)
{
    return thing->type == MINIM_OBJ_CLOSURE || thing->type == MINIM_OBJ_FUNC;
}

bool minim_lambda_equalp(MinimLambda *a, MinimLambda *b)
{
    if (a->argc != b->argc)         return false;
    for (size_t i = 0; i < a->argc; ++i)
    {
        if (strcmp(a->args[i], b->args[i]) != 0)
            return false;
    }

    if ((!a->name && b->name) || (a->name && !b->name) ||
        (a->name && b->name && strcmp(a->name, b->name) != 0))
        return false;

    if ((!a->rest && b->rest) || (a->rest && !b->rest) ||
        (a->rest && b->rest && strcmp(a->rest, b->rest) != 0))
        return false;

    return ast_equalp(a->body, b->body);
}

void minim_lambda_to_buffer(MinimLambda *l, Buffer *bf)
{
    write_buffer(bf, &l->argc, sizeof(int));
    if (l->name)    writes_buffer(bf, l->name);
    if (l->rest)    writes_buffer(bf, l->rest);

    for (size_t i = 0; i < l->argc; ++i)
        writes_buffer(bf, l->args[i]);

    // do not dump ast into buffer
}

// Builtins

static void collect_exprs(MinimObject **exprs, size_t count, MinimLambda *lam)
{
    if (count > 1)
    {
        MinimAst *ast;

        init_ast_op(&ast, count + 1, 0);
        init_ast_node(&ast->children[0], "begin", 0);
        for (size_t i = 0; i < count; ++i)
            copy_ast(&ast->children[i + 1], exprs[i]->data);
        
        lam->body = ast;
    }
    else
    {
        copy_ast(&lam->body, exprs[0]->data);
    }
}

static size_t lambda_argc(MinimObject *bindings)
{
    size_t argc = 0;

    for (MinimObject *it = bindings; it && minim_consp(it) && MINIM_CAR(it);
         it = MINIM_CDR(it), ++argc);
    return argc;
}

MinimObject *minim_builtin_lambda(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_min_argc(&res, "lambda", 2, argc))
    {
        MinimObject *bindings;
        MinimLambda *lam;

        // Convert bindings to list
        unsyntax_ast(env, args[0]->data, &bindings);
        if (bindings->type != MINIM_OBJ_ERR)
        {
            if (minim_symbolp(bindings))
            {
                init_minim_lambda(&lam);
                lam->rest = bindings->data;
                collect_exprs(&args[1], argc - 1, lam);
                rcopy_env(&lam->env, env);
                init_minim_object(&res, MINIM_OBJ_CLOSURE, lam);

                bindings->data = NULL;
            }
            else if (minim_listp(bindings) || minim_consp(bindings))
            {
                MinimObject *it, *val, *val2;

                init_minim_lambda(&lam);
                lam->argc = lambda_argc(bindings);
                lam->args = calloc(lam->argc, sizeof(char*));
                it = bindings;

                for (size_t i = 0; i < lam->argc; ++i, it = MINIM_CDR(it))
                {
                    if (minim_consp(it) && MINIM_CDR(it) && MINIM_CDR(it) && !minim_consp(MINIM_CDR(it)))
                    {   
                        unsyntax_ast(env, MINIM_CAR(it)->data, &val);
                        unsyntax_ast(env, MINIM_CDR(it)->data, &val2);

                        if (assert_symbol(val, &res, "Expected a symbol for lambda variables") &&
                            assert_symbol(val2, &res, "Expected a symbol for the rest variable"))
                        {
                            lam->args[i] = val->data;
                            lam->rest = val2->data;

                            val->data = NULL;
                            val2->data = NULL;
                            free_minim_object(val);
                            free_minim_object(val2);
                        }
                        else
                        {
                            free_minim_object(val);
                            free_minim_object(val2);
                            free_minim_lambda(lam);
                            free_minim_object(bindings);
                            return res;
                        }
                    }
                    else
                    {
                        unsyntax_ast(env, MINIM_CAR(it)->data, &val);
                        if (assert_symbol(val, &res, "Expected a symbol for lambda variables"))
                        {
                            lam->args[i] = val->data;
                            val->data = NULL;
                            free_minim_object(val);
                        }
                        else
                        {
                            free_minim_lambda(lam);
                            free_minim_object(val);
                            free_minim_object(bindings);
                            return res;
                        }
                    }
                }

                collect_exprs(&args[1], argc - 1, lam);
                rcopy_env(&lam->env, env);
                init_minim_object(&res, MINIM_OBJ_CLOSURE, lam);
            }
            else
            {
                minim_error(&res, "Expected a symbol or list of symbols for lambda variables");
            }

            free_minim_object(bindings);
        }
        else
        {
            res = bindings;
        }
    }

    return res;
}