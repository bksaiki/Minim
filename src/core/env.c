#include <stdarg.h>
#include <string.h>

#include "../gc/gc.h"
#include "env.h"
#include "lambda.h"
#include "module.h"

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
    mrk(gc, env->module);
    mrk(gc, env->table);
    mrk(gc, env->callee);
    mrk(gc, env->current_dir);
}

//
//  Visible functions
//

void init_env(MinimEnv **penv, MinimEnv *parent, MinimLambda *callee)
{
    MinimEnv *env = GC_alloc_opt(sizeof(MinimEnv), NULL, gc_minim_env_mrk);

    env->parent = parent;
    env->module = NULL;
    env->callee = callee;
    env->current_dir = NULL;
    init_minim_symbol_table(&env->table);
    env->flags = MINIM_ENV_TAIL_CALLABLE;
    *penv = env;
}


void rcopy_env(MinimEnv **penv, MinimEnv *src)
{
    if (src->parent)
    {
        MinimEnv *env = GC_alloc_opt(sizeof(MinimEnv), NULL, gc_minim_env_mrk);

        rcopy_env(&env->parent, src->parent);
        env->module = src->module;
        copy_minim_symbol_table(&env->table, src->table);
        env->current_dir = src->current_dir;
        env->flags = (src->flags | MINIM_ENV_COPIED);
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
        if (val) return val;

        if (it->module && it->module->env == it)
        {
            val = env_get_sym(it->module->import, sym);
            if (val) return val;
        }
    }

    return NULL;
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    add_metadata(obj, sym);
    minim_symbol_table_add(env->table, sym, obj);
}

static int env_set_sym_int(MinimEnv *env, const char *sym, MinimObject *obj)
{
    for (MinimEnv *it = env; it; it = it->parent)
    { 
        if (minim_symbol_table_set(env->table, sym, obj))
            return 1;

        if (it->module && it->module->env == it)
        {
            if (env_set_sym_int(it->module->import, sym, obj))
                return 1;
        }
    }
    
    return 0;
}

int env_set_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    add_metadata(obj, sym);
    return env_set_sym_int(env, sym, obj);
}

const char *env_peek_key(MinimEnv *env, MinimObject *value)
{
    const char *name;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        name = minim_symbol_table_peek_name(it->table, value);
        if (name) return name;

        if (it->module && it->module->env == it)
        {
            name = env_peek_key(it->module->import, value);
            if (name) return name;
        }
    }

    return NULL;
}

size_t env_symbol_count(MinimEnv *env)
{
    size_t count = 0;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        count += it->table->size;
        if (it->module && it->module->env == it)
            count += it->module->import->table->size;
    }

    return count;
}

bool env_has_called(MinimEnv *env, MinimLambda *lam)
{
    if (env->flags & MINIM_ENV_TAIL_CALLABLE)
    {
        if (env->callee)
            return minim_lambda_equalp(env->callee, lam);
        
        if (env->parent)
            return env_has_called(env->parent, lam);
    }

    return false;
}
