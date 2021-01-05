#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "env.h"
#include "lambda.h"

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

void init_env(MinimEnv **penv, MinimEnv *parent)
{
    MinimEnv *env = malloc(sizeof(MinimEnv));
    env->names = NULL;
    env->name_count = 0;
    *penv = env;

    if (parent)
    {
        env->parent = parent;
        env->table = parent->table;
    }
    else
    {
        MinimSymbolTable *table;

        init_minim_symbol_table(&table);
        env->parent = NULL;
        env->table = table;
    }
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    return minim_symbol_table_get(env->table, sym);
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    MinimObject *owned = fresh_minim_object(obj);

    add_metadata(owned, sym);
    minim_symbol_table_add(env->table, sym, owned);

    if (env->parent)
    {
        ++env->name_count;
        env->names = realloc(env->names, env->name_count);
        env->names[env->name_count - 1] = malloc((strlen(sym) + 1) * sizeof(char));
        strcpy(env->names[env->name_count - 1], sym);
    }
}

int env_set_sym(MinimEnv *env, const char* sym, MinimObject *obj)
{
    MinimObject *owned = fresh_minim_object(obj);

    add_metadata(owned, sym);
    minim_symbol_table_add(env->table, sym, owned);
    return 0;
}

const char *env_peek_key(MinimEnv *env, MinimObject *value)
{
    return NULL;
}

MinimObject *env_peek_sym(MinimEnv *env, const char *sym)
{
    return minim_symbol_table_peek(env->table, sym);
}

void free_env(MinimEnv *env)
{
    if (env->parent)
    {
        free_env(env);
        for (size_t i = 0; i < env->name_count; ++i)
            free(env->names[i]);
        free(env->names);
    }
    else
    {
        free_minim_symbol_table(env->table);
    }

    free(env);
}

MinimEnv *pop_env(MinimEnv *env)
{
    MinimEnv *next;

    next = env->parent;
    for (size_t i = 0; i < env->name_count; ++i)
    {
        minim_symbol_table_pop(env->table, env->names[i]);
        free(env->names[i]);
    }

    free(env->names);
    return next;
}

size_t env_symbol_count(MinimEnv *env)
{
    return env->table->size;
}