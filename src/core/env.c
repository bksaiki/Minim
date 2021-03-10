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
    MinimSymbolTable *table;

    env->parent = parent;
    *penv = env;

    init_minim_symbol_table(&table);
    env->table = table;

    if (parent) env->sym_count = parent->sym_count + parent->table->size;
    else        env->sym_count = 0;
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    MinimObject *val;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        val = minim_symbol_table_get(env->table, sym);
        if (val)    return val;
    }

    return NULL;
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    MinimObject *owned = fresh_minim_object(obj);

    add_metadata(owned, sym);
    minim_symbol_table_add(env->table, sym, owned);
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
    const char *name;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        name = minim_symbol_table_peek_name(env->table, value);
        if (name)    return name;
    }

    return NULL;
}

MinimObject *env_peek_sym(MinimEnv *env, const char *sym)
{
    MinimObject *val;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        val = minim_symbol_table_peek(env->table, sym);
        if (val)    return val;
    }

    return NULL;
}

void free_env(MinimEnv *env)
{
    free_minim_symbol_table(env->table);
    if (env->parent) free_env(env->parent);
    free(env);
}

MinimEnv *pop_env(MinimEnv *env)
{
    MinimEnv *next = env->parent;

    free_minim_symbol_table(env->table);
    free_env(env);

    return next;
}

size_t env_symbol_count(MinimEnv *env)
{
    return env->sym_count + env->table->size;
}