#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "env.h"

// Modules

#include "bool.h"
#include "for.h"
#include "hash.h"
#include "iter.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "math.h"
#include "variable.h"
#include "string.h"
#include "sequence.h"

static void free_single_env(MinimEnv *env)
{
    if (env->count != 0)
    {
        for (int i = 0; i < env->count; ++i)
        {
            free(env->syms[i]);
            free_minim_object(env->vals[i]);
        }

        free(env->syms);
        free(env->vals);
    }

    if (env->iobjs) free_minim_iter_objs(env->iobjs);
    free(env);
}

static void add_metadata(MinimObject *obj, const char *str)
{
    if (obj->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam = obj->data;

        if (lam->name)      free(lam->name);
        lam->name = malloc((strlen(str) + 1) * sizeof(char));
        strcpy(lam->name, str);
    }
}

//
//  Visible functions
//

void init_env(MinimEnv **penv)
{
    MinimEnv *env = malloc(sizeof(MinimEnv));
    env->count = 0;
    env->syms = NULL;
    env->vals = NULL;
    env->parent = NULL;
    env->iobjs = NULL;
    *penv = env;
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    MinimObject *obj;

    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
        {
            copy_minim_object(&obj, env->vals[i]);
            return obj;
        }
    }

    if (env->parent)    return env_get_sym(env->parent, sym);
    else                return NULL;
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
        {
            free_minim_object(env->vals[i]);
            env->vals[i] = obj;
            add_metadata(env->vals[i], sym);
            return;
        }
    }

    ++env->count;
    env->syms = realloc(env->syms, env->count * sizeof(char*));
    env->vals = realloc(env->vals, env->count * sizeof(MinimObject**));

    env->syms[env->count - 1] = malloc(strlen(sym) + 1);
    env->vals[env->count - 1] = obj;
    strcpy(env->syms[env->count - 1], sym);
    add_metadata(env->vals[env->count - 1], sym);
}

int env_set_sym(MinimEnv *env, const char* sym, MinimObject *obj)
{
    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
        {
            free_minim_object(env->vals[i]);
            env->vals[i] = obj;
            add_metadata(env->vals[i], sym);
            return 1;
        }
    }

    if (env->parent)
        return env_set_sym(env->parent, sym, obj);
    return 0;
}

const char *env_peek_key(MinimEnv *env, MinimObject *value)
{
    for (int i = 0; i < env->count; ++i)
    {
        if (value->data == env->vals[i]->data)
            return env->syms[i];
    }

    if (env->parent)    return env_peek_key(env->parent, value);
    else                return NULL;
}

MinimObject *env_peek_sym(MinimEnv *env, const char *sym)
{
    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
            return env->vals[i];
    }

    if (env->parent)    return env_peek_sym(env->parent, sym);
    else                return NULL;
}

MinimIterObjs *env_get_iobjs(MinimEnv *env)
{
    if (env->iobjs)     return env->iobjs;
    if (env->parent)    return env_get_iobjs(env->parent);
    
    return NULL;
}

void free_env(MinimEnv *env)
{
    if (env->parent)  free_env(env->parent);
    free_single_env(env);
}

MinimEnv *pop_env(MinimEnv *env)
{
    MinimEnv *next;

    next = env->parent;
    free_single_env(env);
    return next;
}

void env_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...)
{
    MinimObject *obj;
    va_list rest;

    va_start(rest, type);
    if (type == MINIM_OBJ_FUNC || type == MINIM_OBJ_SYNTAX)
    {
        init_minim_object(&obj, type, va_arg(rest, MinimBuiltin));
    }
    else if (type == MINIM_OBJ_BOOL)
    {
        init_minim_object(&obj, type, va_arg(rest, int));
    }

    va_end(rest);
    env_intern_sym(env, name, obj);
}

void env_load_builtins(MinimEnv *env)
{
    env_load_module_bool(env);
    env_load_module_number(env);
    env_load_module_variable(env);
    env_load_module_string(env);

    env_load_module_for(env);
    env_load_module_iter(env);
    env_load_module_lambda(env);
    env_load_module_seq(env);

    env_load_module_hash(env);
    env_load_module_list(env);
    env_load_module_math(env);
}