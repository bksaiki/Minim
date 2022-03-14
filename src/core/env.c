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
    mrk(gc, env->module_inst);
    mrk(gc, env->table);
    mrk(gc, env->callee);
    mrk(gc, env->caller);
    mrk(gc, env->jmp);
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

#define INIT_SYMBOL_TABLE_IF_NULL(tab) \
    if ((tab) == NULL) { init_minim_symbol_table(&tab); }

#define RETURN_IF_NULL(x, ret) \
    if ((x) == NULL) { return (ret); }

//
//  Visible functions
//

MinimEnv *init_env(MinimEnv *parent)
{
    MinimEnv *env = GC_alloc_opt(sizeof(MinimEnv), NULL, gc_minim_env_mrk);

    env->parent = parent;
    env->module_inst = NULL;
    env->callee = NULL;
    env->caller = NULL;
    env->current_dir = NULL;
    env->jmp = NULL;
    env->table = NULL;
    env->flags = (env->parent ? env->parent->flags: 0x0);

    return env;
}


MinimEnv *init_env2(MinimEnv *parent, MinimLambda *callee)
{
    MinimEnv *env = GC_alloc_opt(sizeof(MinimEnv), NULL, gc_minim_env_mrk);

    env->parent = parent;
    env->module_inst = NULL;
    env->callee = callee;
    env->caller = NULL;
    env->current_dir = NULL;
    env->jmp = NULL;
    env->table = NULL;

    if (callee)             env->flags = MINIM_ENV_TAIL_CALLABLE;
    else if (env->parent)   env->flags = env->parent->flags;
    else                    env->flags = 0x0;

    return env;
}

static MinimObject *env_get_sym_hashed(MinimEnv *env, const char *sym, size_t hash)
{
    for (MinimEnv *it = env; it; it = it->parent)
    {   
        if (it->table)
        {
            MinimObject *val = minim_symbol_table_get2(it->table, sym, hash);
            if (val) return val;
        }
    }

    return minim_symbol_table_get2(global.builtins, sym, hash);
}

MinimObject *env_get_sym(MinimEnv *env, const char *sym)
{
    return env_get_sym_hashed(env, sym, hash_symbol(sym));
}

MinimObject *env_get_local_sym(MinimEnv *env, const char *sym)
{
    RETURN_IF_NULL(env->table, NULL);
    return minim_symbol_table_get2(env->table, sym, hash_symbol(sym));
}

void env_intern_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    add_metadata(obj, sym);
    INIT_SYMBOL_TABLE_IF_NULL(env->table);
    minim_symbol_table_add2(env->table, sym, hash_symbol(sym), obj);
}

static int env_set_sym_hashed(MinimEnv *env, const char *sym, size_t hash, MinimObject *obj)
{
    for (MinimEnv *it = env; it; it = it->parent)
    { 
        if (it->table && minim_symbol_table_set(it->table, sym, hash, obj))
            return 1;
    }
    
    return minim_symbol_table_set(global.builtins, sym, hash, obj);
}

int env_set_sym(MinimEnv *env, const char *sym, MinimObject *obj)
{
    add_metadata(obj, sym);
    return env_set_sym_hashed(env, sym, hash_symbol(sym), obj);
}

const char *env_peek_key(MinimEnv *env, MinimObject *obj)
{
    for (MinimEnv *it = env; it; it = it->parent)
    {   
        if (it->table)
        {
            const char *name = minim_symbol_table_peek_name(it->table, obj);
            if (name) return name;
        }
    }

    // check globals, last resort
    return minim_symbol_table_peek_name(global.builtins, obj);
}

void unwind_tail_call(MinimEnv *env, MinimTailCall *tc)
{
    for (MinimEnv *it = env; it; it = it->parent)
    {
        if (it->jmp)
        {
            MINIM_JUMP_VAL(it->jmp) = (MinimObject*) tc;
            longjmp(*MINIM_JUMP_BUF(it->jmp), 1);
        }
    }    

    THROW(env, minim_error("error when unwinding tail call: no caller found", NULL));
}

size_t env_symbol_count(MinimEnv *env)
{
    size_t count = 0;

    for (MinimEnv *it = env; it; it = it->parent)
    {
        if (it->table)
            count += it->table->size;
    }

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
        if (it->table)
        {
            env_for_print = it;
            minim_symbol_table_for_each(it->table, print_symbol_entry);
        }
    }

    minim_symbol_table_for_each(global.builtins, print_symbol_entry);
}

void env_dump_exports(MinimEnv *env)
{
    MinimModule *module = env->module_inst->module;
    if (env->module_inst && module->export)
    {
        env_for_print = module->export;
        minim_symbol_table_for_each(module->export->table, print_symbol_entry);
    }
    else
    {
        printf("()\n");
    }
}

MinimSymbolTable *env_get_table(MinimEnv *env)
{
    INIT_SYMBOL_TABLE_IF_NULL(env->table);
    return env->table;
}

void env_merge_local_symbols(MinimEnv *dest, MinimEnv *src)
{
    if (src->table == NULL)     // nothing to add
        return;

    INIT_SYMBOL_TABLE_IF_NULL(dest->table);
    minim_symbol_table_merge(dest->table, src->table);
}

void env_merge_local_symbols2(MinimEnv *dest,
                              MinimEnv *src,
                              MinimObject *(*merge)(MinimObject *, MinimObject *),
                              MinimObject *(*add)(MinimObject *))
{
    if (src->table == NULL)     // nothing to add
        return;

    INIT_SYMBOL_TABLE_IF_NULL(dest->table);
    minim_symbol_table_merge2(dest->table, src->table, merge, add); 
}

void env_for_each_local_symbol(MinimEnv *env,
                               void (*func)(const char *, MinimObject *))
{
    if (env->table == NULL)
        return;

    minim_symbol_table_for_each(env->table, func);
}
