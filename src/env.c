#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "env.h"

//
//  Visible functions
//

void init_env(MinimEnv **penv)
{
    MinimEnv *env = malloc(sizeof(MinimEnv));
    env->count = 0;
    env->syms = NULL;
    env->vals = NULL;
    *penv = env;
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
            return copy_minim_object(env->vals[i]);
    }

    return NULL;
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
        {
            free_minim_object(env->vals[i]);
            env->vals[i] = copy_minim_object(obj);
            return;
        }
    }

    ++env->count;
    env->syms = realloc(env->syms, env->count * sizeof(char*));
    env->vals = realloc(env->vals, env->count * sizeof(MinimObject**));

    env->syms[env->count - 1] = malloc(strlen(sym) + 1);
    env->vals[env->count - 1] = copy_minim_object(obj);
    strcpy(env->syms[env->count - 1], sym);
}

MinimObject *env_peek_sym(MinimEnv *env, const char *sym)
{
    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
            return env->vals[i];
    }

    return NULL;
}

void free_env(MinimEnv *env)
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