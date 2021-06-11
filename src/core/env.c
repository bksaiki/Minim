#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../gc/gc.h"
#include "env.h"
#include "lambda.h"

static void add_metadata(MinimObject *obj, const char *str)
{
    if (obj->type == MINIM_OBJ_CLOSURE)
    {
        MinimLambda *lam = obj->u.ptrs.p1;

        lam->name = GC_alloc_atomic((strlen(str) + 1) * sizeof(char));
        strcpy(lam->name, str);
    }
}

static void gc_minim_env_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimEnv *env = (MinimEnv*) ptr;
    mrk(gc, env->parent);
    mrk(gc, env->table);
    mrk(gc, env->callee);
}

//
//  Visible functions
//

void init_env(MinimEnv **penv, MinimEnv *parent, MinimLambda *callee)
{
    MinimEnv *env = GC_alloc_opt(sizeof(MinimEnv), NULL, gc_minim_env_mrk);

    env->parent = parent;
    env->callee = callee;
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
        MinimEnv *env = GC_alloc_opt(sizeof(MinimEnv), NULL, gc_minim_env_mrk);

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
    add_metadata(obj, sym);
    minim_symbol_table_add(env->table, sym, obj);
}

int env_set_sym(MinimEnv *env, const char* sym, MinimObject *obj)
{
    add_metadata(obj, sym);
    minim_symbol_table_add(env->table, sym, obj);
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

size_t env_symbol_count(MinimEnv *env)
{
    return env->sym_count + env->table->size;
}

bool env_has_called(MinimEnv *env, MinimLambda *lam)
{
    if (env->callee && minim_lambda_equalp(env->callee, lam))
        return true;
    
    if (env->parent)
        return env_has_called(env->parent, lam);

    return false;
}
