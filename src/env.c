#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "env.h"    

#include "lambda.h"

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
    *penv = env;
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    MinimObject *obj;

    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
        {
            ref_minim_object(&obj, env->vals[i]);
            return obj;
        }
    }

    if (env->parent)    return env_get_sym(env->parent, sym);
    else                return NULL;
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    MinimObject *fresh = fresh_minim_object(obj);

    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
        {
            free_minim_object(env->vals[i]);
            env->vals[i] = fresh;
            add_metadata(env->vals[i], sym);
            return;
        }
    }

    ++env->count;
    env->syms = realloc(env->syms, env->count * sizeof(char*));
    env->vals = realloc(env->vals, env->count * sizeof(MinimObject**));

    env->syms[env->count - 1] = malloc(strlen(sym) + 1);
    env->vals[env->count - 1] = fresh;
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