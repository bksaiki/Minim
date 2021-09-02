#include <string.h>

#include "../gc/gc.h"
#include "assert.h"
#include "error.h"
#include "eval.h"
#include "lambda.h"
#include "list.h"
#include "tail_call.h"

static void gc_minim_lambda_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimLambda *lam = (MinimLambda *) ptr;
    mrk(gc, lam->body);
    mrk(gc, lam->loc);
    mrk(gc, lam->env);
    mrk(gc, lam->args);
    mrk(gc, lam->rest);
    mrk(gc, lam->name);
}

void init_minim_lambda(MinimLambda **plam)
{
    MinimLambda *lam = GC_alloc_opt(sizeof(MinimLambda), NULL, gc_minim_lambda_mrk);
    
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
    MinimLambda *lam = GC_alloc_opt(sizeof(MinimLambda), NULL, gc_minim_lambda_mrk);

    lam->argc = src->argc;
    if (src->args)
    {
        lam->args = GC_alloc(lam->argc * sizeof(char*));
        for (size_t i = 0; i < lam->argc; ++i)
        {
            lam->args[i] = GC_alloc_atomic((strlen(src->args[i]) + 1) * sizeof(char));
            strcpy(lam->args[i], src->args[i]);
        }
    }   
    else
    {
        lam->args = NULL;
    }

    if (src->rest)
    {
        lam->rest = GC_alloc_atomic((strlen(src->rest) + 1) * sizeof(char));
        strcpy(lam->rest, src->rest);
    }
    else
    {
        lam->rest = NULL;
    }

    if (src->name)
    {
        lam->name = GC_alloc_atomic((strlen(src->name) + 1) * sizeof(char));
        strcpy(lam->name, src->name);
    }
    else
    {
        lam->name = NULL;
    }

    if (src->env)   init_env(&lam->env, src->env, NULL);
    else            lam->env = NULL;

    if (src->loc)   copy_syntax_loc(&lam->loc, src->loc);
    else            lam->loc = NULL;

    if (src->body)  copy_syntax_node(&lam->body, src->body);
    else            lam->body = NULL;

    *cp = lam;
}

static MinimEnv *lambda_env = NULL;

static void intern_transform(const char *sym, MinimObject *obj)
{
    if (MINIM_OBJ_TRANSFORMP(obj) && MINIM_TRANSFORM_TYPE(obj) == MINIM_TRANSFORM_MACRO)
        env_intern_sym(lambda_env, sym, obj);
}

static MinimModule *env_get_module(MinimEnv *env)
{
    for (MinimEnv *it = env; it; it = it->parent)
    {
        if (it->module)
            return it->module;
    }

    return false;
}

MinimObject *eval_lambda_core(MinimLambda* lam, MinimEnv *env, MinimEnv *env2, size_t argc, MinimObject **args)
{
    MinimObject *res, *val;

    // intern arguments
    for (size_t i = 0; i < lam->argc; ++i)
        env_intern_sym(env2, lam->args[i], args[i]);

    if (lam->rest)
    {
        MinimObject **rest;
        size_t rcount = argc - lam->argc;

        rest = GC_alloc(rcount * sizeof(MinimObject*));
        for (size_t i = 0; i < rcount; ++i)
            rest[i] = args[lam->argc + i];

        val = minim_list(rest, rcount);
        env_intern_sym(env2, lam->rest, val);
    }

    res = eval_ast_no_check(env2, lam->body);
    if (MINIM_OBJ_TAIL_CALLP(res))
    {   
        MinimTailCall *call = MINIM_TAIL_CALL(res);

        if (call->lam == lam)
            return eval_lambda(lam, env, call->argc, call->args);
    }

    return res;
}

MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimEnv *env2;

    if (lam->rest)
    {
        char *name = (lam->name ? lam->name : "");
        if (argc < lam->argc)
            THROW(env, minim_arity_error(name, lam->argc, SIZE_MAX, argc));

    }
    else if (argc != lam->argc)
    {
        char *name = (lam->name ? lam->name : "");
        THROW(env, minim_arity_error(name, lam->argc, lam->argc, argc));
    }

    // create internal environment
    init_env(&env2, lam->env, lam);
    env2->caller = env;

    return eval_lambda_core(lam, env, env2, argc, args);
}

MinimObject *eval_lambda2(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimEnv *env2;

    if (lam->rest)
    {
        char *name = (lam->name ? lam->name : "");
        if (argc < lam->argc)
            THROW(env, minim_arity_error(name, lam->argc, SIZE_MAX, argc));

    }
    else if (argc != lam->argc)
    {
        char *name = (lam->name ? lam->name : "");
        THROW(env, minim_arity_error(name, lam->argc, lam->argc, argc));
    }

    // create internal environment
    init_env(&env2, lam->env, lam);
    // env2->caller = env;

    // merge in transforms
    if (env)
    {
        MinimModule *mod = env_get_module(env);
        if (mod)
        {
            lambda_env = env2;
            minim_symbol_table_for_each(mod->env->table, intern_transform);
        }
    }

    return eval_lambda_core(lam, env, env2, argc, args);    
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
        SyntaxNode *ast;

        init_syntax_node(&ast, SYNTAX_NODE_LIST);
        ast->children = GC_alloc((count + 1) * sizeof(SyntaxNode*));
        ast->childc = count + 1;

        init_syntax_node(&ast->children[0], SYNTAX_NODE_DATUM);
        ast->children[0]->sym = GC_alloc_atomic(6 * sizeof(char));
        strcpy(ast->children[0]->sym, "begin");

        lam->body = ast;
        for (size_t i = 0; i < count; ++i)
            copy_syntax_node(&ast->children[i + 1], MINIM_AST(exprs[i]));
    }
    else
    {
        copy_syntax_node(&lam->body, MINIM_AST(exprs[0]));
    }
}

static size_t lambda_argc(MinimObject *bindings)
{
    size_t argc = 0;
    for (MinimObject *it = bindings; minim_consp(it); it = MINIM_CDR(it)) ++argc;
    return argc;
}

MinimObject *minim_builtin_lambda(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *bindings;
    MinimLambda *lam;

    // Convert bindings to list
    init_minim_lambda(&lam);
    bindings = unsyntax_ast(env, MINIM_AST(args[0]));
    if (MINIM_OBJ_SYMBOLP(bindings))
    {
        lam->rest = MINIM_SYMBOL(bindings);
        collect_exprs(&args[1], argc - 1, lam);
        init_env(&lam->env, env, NULL);

        res = minim_closure(lam);
        MINIM_SYMBOL(bindings) = NULL;
    }
    else // minim_listp(bindings) || minim_consp(bindings)
    {
        MinimObject *it, *val, *val2;

        lam->argc = lambda_argc(bindings);
        lam->args = GC_calloc(lam->argc, sizeof(char*));
        it = bindings;
        for (size_t i = 0; i < lam->argc; ++i, it = MINIM_CDR(it))
        {
            if (minim_nullp(MINIM_CDR(it)) || minim_consp(MINIM_CDR(it)))
            {
                val = unsyntax_ast(env, MINIM_AST(MINIM_CAR(it)));
                lam->args[i] = MINIM_SYMBOL(val);
            }
            else    // rest args
            {   
                val = unsyntax_ast(env, MINIM_AST(MINIM_CAR(it)));
                val2 = unsyntax_ast(env, MINIM_AST(MINIM_CDR(it)));
                lam->args[i] = MINIM_SYMBOL(val);
                lam->rest = MINIM_SYMBOL(val2);
            }
        }

        collect_exprs(&args[1], argc - 1, lam);
        init_env(&lam->env, env, NULL);
        res = minim_closure(lam);
    }

    return res;
}
