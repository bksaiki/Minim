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

    env->parent = parent;
    init_minim_symbol_table(&env->table);
    env->copied = false;
    *penv = env;

    if (parent) env->sym_count = parent->sym_count + parent->table->size;
    else        env->sym_count = 0;
}


void rcopy_env(MinimEnv **penv, MinimEnv *src)
{
    if (src->parent)
    {
        MinimEnv *env = malloc(sizeof(MinimEnv));

        rcopy_env(&env->parent, src->parent);
        copy_minim_symbol_table(&env->table, src->table);
        env->sym_count = src->sym_count;
        env->copied = true;
        *penv = env;
    }
    else
    {
        *penv = src;
    }
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    MinimObject *val;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        val = minim_symbol_table_get(it->table, sym);
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
        name = minim_symbol_table_peek_name(it->table, value);
        if (name)    return name;
    }

    return NULL;
}

MinimObject *env_peek_sym(MinimEnv *env, const char *sym)
{
    MinimObject *val;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        val = minim_symbol_table_peek(it->table, sym);
        if (val)    return val;
    }

    return NULL;
}

void free_env(MinimEnv *env)
{
    free_minim_symbol_table(env->table);
    if(env->parent && (!env->copied || env->parent->copied))
        free_env(env->parent);
    free(env);
}

MinimEnv *pop_env(MinimEnv *env)
{
    MinimEnv *next = env->parent;

    free_minim_symbol_table(env->table);
    free(env);

    return next;
}

size_t env_symbol_count(MinimEnv *env)
{
    return env->sym_count + env->table->size;
}