#include "minimpriv.h"

static void add_metadata(MinimObject *obj, const char *str)
{
    if (MINIM_OBJ_CLOSUREP(obj))
    {
        MinimLambda *lam = MINIM_CLOSURE(obj);
        if (!lam->name)
        {
            lam->name = GC_alloc_atomic((strlen(str) + 1) * sizeof(char));
            strcpy(lam->name, str);
        }
    }
}

static void gc_minim_env_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimEnv *env = (MinimEnv*) ptr;

    mrk(gc, env->parent);
    mrk(gc, env->module);
    mrk(gc, env->table);
    mrk(gc, env->callee);
    mrk(gc, env->caller);
    mrk(gc, env->current_dir);
}

static MinimEnv *env_for_print = NULL;

static void print_symbol_entry(const char *sym, MinimObject *obj)
{
    PrintParams pp;

    if (MINIM_OBJ_TAIL_CALLP(obj))
    {
        printf("(%s . <tail call>)\n", sym);
    }
    else if (MINIM_OBJ_TRANSFORMP(obj))
    {
        printf("(%s . <macro>)\n", sym);
    }
    else if (MINIM_OBJ_SYNTAXP(obj))
    {
        printf("(%s . <syntax>)\n", sym);
    }
    else
    {
        set_default_print_params(&pp);
        printf("(%s . ", sym);
        print_to_port(obj, env_for_print, &pp, MINIM_PORT_FILE(minim_output_port));
        printf(")\n");
    }
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
    env->caller = NULL;
    env->current_dir = NULL;
    init_minim_symbol_table(&env->table);
    env->flags = MINIM_ENV_TAIL_CALLABLE;

    *penv = env;
}

static MinimObject *env_get_sym_hashed(MinimEnv *env, const char *sym, size_t hash)
{
    MinimObject *val;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        val = minim_symbol_table_get(it->table, sym, hash);
        if (val) return val;
    }

    return minim_symbol_table_get(global.builtins, sym, hash);
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    size_t hash;

    hash = hash_symbol(sym);
    return env_get_sym_hashed(env, sym, hash);
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    size_t hash;

    add_metadata(obj, sym);
    hash = hash_symbol(sym);
    minim_symbol_table_add2(env->table, sym, hash, obj);
}

static int env_set_sym_hashed(MinimEnv *env, const char *sym, size_t hash, MinimObject *obj)
{
    for (MinimEnv *it = env; it; it = it->parent)
    { 
        if (minim_symbol_table_set(it->table, sym, hash, obj))
            return 1;
    }
    
    return minim_symbol_table_set(global.builtins, sym, hash, obj);
}

int env_set_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    size_t hash;

    add_metadata(obj, sym);
    hash = hash_symbol(sym);
    return env_set_sym_hashed(env, sym, hash, obj);
}

const char *env_peek_key(MinimEnv *env, MinimObject *obj)
{
    const char *name;

    for (MinimEnv *it = env; it; it = it->parent)
    {   
        name = minim_symbol_table_peek_name(it->table, obj);
        if (name) return name;
    }

    // check globals, last resort
    return minim_symbol_table_peek_name(global.builtins, obj);
}

size_t env_symbol_count(MinimEnv *env)
{
    size_t count = 0;

    for (MinimEnv *it = env; it; it = it->parent)
        count += it->table->size;

    return count + global.builtins->size;
}

bool env_has_called(MinimEnv *env, MinimLambda *lam)
{
    if (env->flags & MINIM_ENV_TAIL_CALLABLE)
    {
        if (env->callee && env->callee == lam)
            return true;
        
        if (env->caller)
            return env_has_called(env->caller, lam);

        if (env->parent)
            return env_has_called(env->parent, lam);
    }

    return false;
}

void env_dump_symbols(MinimEnv *env)
{
    for (MinimEnv *it = env; it; it = it->parent)
    {
        env_for_print = it;
        minim_symbol_table_for_each(it->table, print_symbol_entry);
    }

    minim_symbol_table_for_each(global.builtins, print_symbol_entry);
}

void env_dump_exports(MinimEnv *env)
{
    if (env->module && env->module->export)
    {
        env_for_print = env->module->export;
        minim_symbol_table_for_each(env->module->export->table, print_symbol_entry);
    }
    else
    {
        printf("()\n");
    }
}
