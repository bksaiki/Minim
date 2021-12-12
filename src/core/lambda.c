#include "minimpriv.h"

static void gc_minim_lambda_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimLambda *lam = (MinimLambda *) ptr;
    mrk(gc, lam->body);
    mrk(gc, lam->loc);
    mrk(gc, lam->env);
    mrk(gc, lam->args);
    mrk(gc, lam->rest);
    mrk(gc, lam->name);
}

void init_minim_lambda(MinimLambda **plam)
{
    MinimLambda *lam = GC_alloc_opt(sizeof(MinimLambda), NULL, gc_minim_lambda_mrk);
    
    lam->loc = NULL;
    lam->env = NULL;
    lam->args = NULL;
    lam->rest = NULL;
    lam->body = NULL;
    lam->name = NULL;
    lam->argc = 0;

    *plam = lam;
}

static MinimEnv *lambda_env = NULL;

static void intern_transform(const char *sym, MinimObject *obj)
{
    if (MINIM_OBJ_TRANSFORMP(obj) && MINIM_TRANSFORM_TYPE(obj) == MINIM_TRANSFORM_MACRO)
        env_intern_sym(lambda_env, sym, obj);
}

static MinimModule *env_get_module(MinimEnv *env)
{
    for (MinimEnv *it = env; it; it = it->parent)
    {
        if (it->module)
            return it->module;
    }

    return false;
}

MinimObject *eval_lambda(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val, *jmp;
    MinimEnv *env2;
    jmp_buf *pjmp_buf;

    pjmp_buf = GC_alloc_atomic(sizeof(jmp_buf));
    jmp = minim_jmp(pjmp_buf, NULL);
    if (setjmp(*pjmp_buf) != 0)      // if jumped back
    {
        MinimTailCall *tc = (MinimTailCall*) MINIM_JUMP_VAL(jmp);
        lam = tc->lam;
        argc = tc->argc;
        args = tc->args;
    }

    if (lam->rest)
    {
        char *name = (lam->name ? lam->name : "");
        if (argc < lam->argc)
            THROW(env, minim_arity_error(name, lam->argc, SIZE_MAX, argc));

    }
    else if (argc != lam->argc)
    {
        char *name = (lam->name ? lam->name : "");
        THROW(env, minim_arity_error(name, lam->argc, lam->argc, argc));
    }

    // create internal environment
    init_env(&env2, lam->env, lam);
    env2->caller = env;
    env2->jmp = jmp;

    // intern arguments
    for (size_t i = 0; i < lam->argc; ++i)
        env_intern_sym(env2, lam->args[i], args[i]);

    if (lam->rest)
    {
        MinimObject **rest;
        size_t rcount = argc - lam->argc;

        rest = GC_alloc(rcount * sizeof(MinimObject*));
        for (size_t i = 0; i < rcount; ++i)
            rest[i] = args[lam->argc + i];

        val = minim_list(rest, rcount);
        env_intern_sym(env2, lam->rest, val);
    }

    log_proc_called();
    return eval_ast_no_check(env2, lam->body);
}

MinimObject *eval_lambda2(MinimLambda* lam, MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val;
    MinimEnv *env2;

    if (lam->rest)
    {
        char *name = (lam->name ? lam->name : "");
        if (argc < lam->argc)
            THROW(env, minim_arity_error(name, lam->argc, SIZE_MAX, argc));

    }
    else if (argc != lam->argc)
    {
        char *name = (lam->name ? lam->name : "");
        THROW(env, minim_arity_error(name, lam->argc, lam->argc, argc));
    }

    // create internal environment
    init_env(&env2, lam->env, lam);
    env2->flags = 0x0;

    // merge in transforms
    if (env)
    {
        MinimModule *mod = env_get_module(env);
        if (mod)
        {
            lambda_env = env2;
            minim_symbol_table_for_each(mod->env->table, intern_transform);
        }
    }

    // intern arguments
    for (size_t i = 0; i < lam->argc; ++i)
        env_intern_sym(env2, lam->args[i], args[i]);

    if (lam->rest)
    {
        MinimObject **rest;
        size_t rcount = argc - lam->argc;

        rest = GC_alloc(rcount * sizeof(MinimObject*));
        for (size_t i = 0; i < rcount; ++i)
            rest[i] = args[lam->argc + i];

        val = minim_list(rest, rcount);
        env_intern_sym(env2, lam->rest, val);
    }

    log_proc_called();
    return eval_ast_no_check(env2, lam->body);
}

void minim_lambda_to_buffer(MinimLambda *l, Buffer *bf)
{
    write_buffer(bf, &l->argc, sizeof(int));
    if (l->name)    writes_buffer(bf, l->name);
    if (l->rest)    writes_buffer(bf, l->rest);

    for (size_t i = 0; i < l->argc; ++i)
        writes_buffer(bf, l->args[i]);

    // do not dump ast into buffer
}

// Builtins

static void collect_exprs(MinimObject **exprs, size_t count, MinimLambda *lam)
{
    if (count > 1)
    {
        MinimObject *h, *t, *beg;
        
        h = minim_cons(exprs[0], minim_null);
        t = h;
        for (size_t i = 1; i < count; ++i)
        {
            MINIM_CDR(t) = minim_cons(exprs[i], minim_null);
            t = MINIM_CDR(t);
        }

        beg = minim_ast(intern("begin"), MINIM_STX_LOC(exprs[0]));
        lam->body = minim_ast(minim_cons(beg, h), MINIM_STX_LOC(exprs[0]));
    }
    else
    {
        lam->body = exprs[0];
    }
}

static size_t lambda_argc(MinimObject *bindings)
{
    size_t argc = 0;
    for (MinimObject *it = bindings; minim_consp(it); it = MINIM_CDR(it)) ++argc;
    return argc;
}

MinimObject *minim_builtin_lambda(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *bindings;
    MinimLambda *lam;

    // Convert bindings to list
    init_minim_lambda(&lam);
    bindings = MINIM_STX_VAL(args[0]);
    if (MINIM_OBJ_SYMBOLP(bindings))
    {
        lam->rest = MINIM_SYMBOL(bindings);
        collect_exprs(&args[1], argc - 1, lam);
        init_env(&lam->env, env, NULL);

        res = minim_closure(lam);
    }
    else // minim_listp(bindings) || minim_consp(bindings)
    {
        MinimObject *it;

        lam->argc = lambda_argc(bindings);
        lam->args = GC_calloc(lam->argc, sizeof(char*));
        it = bindings;
        for (size_t i = 0; i < lam->argc; ++i, it = MINIM_CDR(it))
        {
            if (minim_nullp(MINIM_CDR(it)) || minim_consp(MINIM_CDR(it)))
            {
                lam->args[i] = MINIM_STX_SYMBOL(MINIM_CAR(it));
            }
            else    // rest args
            {   
                lam->args[i] = MINIM_STX_SYMBOL(MINIM_CAR(it));
                lam->rest = MINIM_STX_SYMBOL(MINIM_CDR(it));
            }
        }

        collect_exprs(&args[1], argc - 1, lam);
        init_env(&lam->env, env, NULL);
        res = minim_closure(lam);
    }

    return res;
}
