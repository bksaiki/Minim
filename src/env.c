#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "env.h"
#include "list.h"
#include "math.h"
#include "variable.h"

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
    MinimObject *obj;

    for (int i = 0; i < env->count; ++i)
    {
        if (strcmp(sym, env->syms[i]) == 0)
        {
            copy_minim_object(&obj, env->vals[i]);
            return obj;
        }
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
            copy_minim_object(&env->vals[i], obj);
            return;
        }
    }

    ++env->count;
    env->syms = realloc(env->syms, env->count * sizeof(char*));
    env->vals = realloc(env->vals, env->count * sizeof(MinimObject**));

    env->syms[env->count - 1] = malloc(strlen(sym) + 1);
    env->vals[env->count - 1] = obj;
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

void env_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...)
{
    MinimObject *obj;
    va_list rest;

    va_start(rest, type);
    if (type == MINIM_OBJ_FUNC || type == MINIM_OBJ_SYNTAX)
    {
        MinimBuiltin func = va_arg(rest, MinimBuiltin);
        init_minim_object(&obj, type, func);
    }

    va_end(rest);
    env_intern_sym(env, name, obj);
}

void env_load_builtins(MinimEnv *env)
{
    env_load_math(env);
    env_load_list(env);
    env_load_module_variable(env);
}