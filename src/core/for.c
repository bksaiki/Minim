#include <stdlib.h>

#include "../gc/gc.h"
#include "assert.h"
#include "error.h"
#include "eval.h"
#include "iter.h"
#include "list.h"

static bool iters_valid(MinimObject **iters, size_t argc)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (minim_iter_endp(iters[i]))
            return false;
    }

    return true;
}

// Builtins

MinimObject *minim_builtin_for(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it, *bindings, *val;
    MinimObject **syms, **iters, **objs;
    size_t len;

    // Convert iter/iterable pairs to list
    unsyntax_ast(env, args[0]->u.ptrs.p1, &bindings);
    len = minim_list_length(bindings);
    objs = GC_alloc(len * sizeof(MinimObject*));
    iters = GC_alloc(len * sizeof(MinimObject*));
    syms = GC_alloc(len * sizeof(MinimObject*));

    it = bindings;
    for (size_t i = 0; i < len; ++i, it = MINIM_CDR(it))
    {
        MinimObject *bind;

        unsyntax_ast(env, MINIM_CAR(it)->u.ptrs.p1, &bind);
        unsyntax_ast(env, MINIM_CAR(bind)->u.ptrs.p1, &syms[i]);
        eval_ast_no_check(env, MINIM_CADR(bind)->u.ptrs.p1, &val);
        if (MINIM_OBJ_THROWNP(val))
        {
            return val;
        }
        else if (minim_iterablep(val))
        {
            objs[i] = val;
            init_minim_iter(&iters[i], objs[i]);
        }
        else
        {
            res = minim_argument_error("iterable object", "for", SIZE_MAX, val);
            return res;
        }
    }

    while(iters_valid(iters, len))
    {
        MinimEnv *env2;

        init_env(&env2, env);
        for (size_t i = 0; i < len; ++i)
        {
            val = minim_iter_get(iters[i]);
            env_intern_sym(env2, syms[i]->u.str.str, val);
        }

        eval_ast_no_check(env2, args[1]->u.ptrs.p1, &val);
        if (MINIM_OBJ_THROWNP(val))
        {
            return val;
        }
        else
        {
            for (size_t i = 0; i < len; ++i)
                iters[i] = minim_iter_next(iters[i]);
        }

        GC_collect_minor();
    }   

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_for_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it, *bindings, *val;
    MinimObject **syms, **iters, **objs;
    size_t len;
    bool head;

    // Convert iter/iterable pairs to list
    unsyntax_ast(env, args[0]->u.ptrs.p1, &bindings);
    len = minim_list_length(bindings);
    objs = GC_alloc(len * sizeof(MinimObject*));
    iters = GC_alloc(len * sizeof(MinimObject*));
    syms = GC_alloc(len * sizeof(MinimObject*));

    it = bindings;
    for (size_t i = 0; i < len; ++i, it = MINIM_CDR(it))
    {
        MinimObject *bind;

        unsyntax_ast(env, MINIM_CAR(it)->u.ptrs.p1, &bind);
        unsyntax_ast(env, MINIM_CAR(bind)->u.ptrs.p1, &syms[i]);
        eval_ast_no_check(env, MINIM_CADR(bind)->u.ptrs.p1, &val);
        if (MINIM_OBJ_THROWNP(val))
        {
            return val;
        }
        else if (minim_iterablep(val))
        {
            objs[i] = val;
            init_minim_iter(&iters[i], objs[i]);
        }
        else
        {
            return minim_argument_error("iterable object", "for-list", SIZE_MAX, val);
        }
    }

    head = true;
    while(iters_valid(iters, len))
    {
        MinimEnv *env2;
        
        init_env(&env2, env);
        for (size_t i = 0; i < len; ++i)
        {
            val = minim_iter_get(iters[i]);
            env_intern_sym(env2, syms[i]->u.str.str, val);
        }

        eval_ast_no_check(env2, args[1]->u.ptrs.p1, &val);
        if (MINIM_OBJ_THROWNP(val))
        {
            return val;
        }
        else
        {
            if (head)
            {
                init_minim_object(&res, MINIM_OBJ_PAIR, val, NULL);
                it = res;
                head = false;
            }
            else
            {
                init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, val, NULL);
                it = MINIM_CDR(it);
            }

            for (size_t i = 0; i < len; ++i)
                iters[i] = minim_iter_next(iters[i]);
        }
    }   

    if (head) init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    return res;
}
