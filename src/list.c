#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "env.h"
#include "list.h"

static MinimObject *construct_list(int argc, MinimObject** args)
{
    MinimObject *obj;
    
    if (argc == 0)
        return NULL;

    init_minim_object(&obj,
                      MINIM_OBJ_PAIR,
                      args[0],
                      construct_list(argc - 1, args + 1));
    return obj;   
}

//
//  Visible functions
//

bool assert_list(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!minim_listp(arg))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool assert_list_len(MinimObject *arg, MinimObject **ret, int len, const char *msg)
{
    if (!assert_list(arg, ret, "Expected a list"))
        return false;

    if (minim_list_length(arg) != len)
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool assert_listof(MinimObject *arg, MinimObject **ret, MinimObjectPred pred, const char *msg)
{
    MinimObject *it;
    int len;

    if (!assert_list(arg, ret, "Expected a list"))
        return false;

    len = minim_list_length(arg);
    it = arg;
    for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
    {
        if (!pred(MINIM_CAR(it)))
        {
            minim_error(ret, "%s", msg);
            return false;
        }
    }

    return true;
}

bool minim_consp(MinimObject* thing)
{
    return (thing->type == MINIM_OBJ_PAIR && MINIM_CAR(thing));
}

bool minim_listp(MinimObject* thing)
{
    if (thing->type == MINIM_OBJ_PAIR)
    {
        MinimObject **pair = thing->data;
        return (!pair[0]|| !pair[1] || pair[1]->type == MINIM_OBJ_PAIR);
    }

    return false;
}

int minim_list_length(MinimObject *list)
{
    int len = 0;

    for (MinimObject *it = list; it != NULL; it = MINIM_CDR(it))
        if (MINIM_CAR(it))  ++len;
    
    return len;
}

void env_load_module_list(MinimEnv *env)
{
    env_load_builtin(env, "cons", MINIM_OBJ_FUNC, minim_builtin_cons);
    env_load_builtin(env, "pair?", MINIM_OBJ_FUNC, minim_builtin_consp);
    env_load_builtin(env, "car", MINIM_OBJ_FUNC, minim_builtin_car);
    env_load_builtin(env, "cdr", MINIM_OBJ_FUNC, minim_builtin_cdr);

    env_load_builtin(env, "list", MINIM_OBJ_FUNC, minim_builtin_list);
    env_load_builtin(env, "list?", MINIM_OBJ_FUNC, minim_builtin_listp);
    env_load_builtin(env, "head", MINIM_OBJ_FUNC, minim_builtin_head);
    env_load_builtin(env, "tail", MINIM_OBJ_FUNC, minim_builtin_tail);
    env_load_builtin(env, "length", MINIM_OBJ_FUNC, minim_builtin_length);
}

MinimObject *minim_builtin_cons(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "cons", 2))
    {
        init_minim_object(&res, MINIM_OBJ_PAIR, args[0], args[1]);
        args[0] = NULL;
        args[1] = NULL;
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_consp(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "pair?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_consp(args[0]));
    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_car(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, **pair;

    if (!assert_exact_argc(argc, args, &res, "car", 1) ||
        !assert_pair_arg(args[0], &res, "car"))
    {
        free_minim_objects(argc, args);
    }
    else if (!MINIM_CAR(args[0]))
    {
        free_minim_objects(argc, args);
        minim_error(&res, "Expected a pair for 'cdr'");
    }
    else
    {
        pair = ((MinimObject**) args[0]->data);
        res = pair[0];
        free_minim_object(pair[1]);

        pair[0] = NULL;
        pair[1] = NULL;
        free_minim_object(args[0]);
        free(args);
    }

    return res;
}

MinimObject *minim_builtin_cdr(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, **pair;

    if (!assert_exact_argc(argc, args, &res, "car", 1) ||
        !assert_pair_arg(args[0], &res, "car"))
    {
        free_minim_objects(argc, args);
    }
    else if (!MINIM_CAR(args[0]))
    {
        free_minim_objects(argc, args);
        minim_error(&res, "Expected a pair for 'cdr'");
    }
    else if (!MINIM_CDR(args[0]))
    {
        free_minim_objects(argc, args);
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    }
    else
    {
        pair = ((MinimObject**) args[0]->data);
        free_minim_object(pair[0]);
        res = pair[1];

        pair[0] = NULL;
        pair[1] = NULL;
        free_minim_object(args[0]);
        free(args);
    }

    return res;
}

MinimObject *minim_builtin_listp(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "list?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_listp(args[0]));
    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_list(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (argc == 0)
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    else
        res = construct_list(argc, args);

    free(args);
    return res;
}

MinimObject *minim_builtin_head(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, **pair;

    if (!assert_exact_argc(argc, args, &res, "head", 1))
    {
        free_minim_objects(argc, args);
    }
    else
    {
        if (MINIM_CAR(args[0]))
        {
            pair = ((MinimObject**) args[0]->data);
            res = pair[0];
            free_minim_object(pair[1]);

            pair[0] = NULL;
            pair[1] = NULL;
            free_minim_object(args[0]);
            free(args);
        }
        else
        {
            free_minim_objects(argc, args);
            minim_error(&res, "Expected a non-empty list");
        }
    }

    return res;
}

MinimObject *minim_builtin_tail(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, *it, **pair;

    if (assert_exact_argc(argc, args, &res, "tail", 1))
    {
        if (MINIM_CAR(args[0]))
        {
            it = args[0];
            while (MINIM_CDR(it))   it = MINIM_CDR(it);

            pair = ((MinimObject**) it->data);
            res = pair[0];
            free_minim_object(pair[1]);

            pair[0] = NULL;
            pair[1] = NULL;
        }
        else
        {
            minim_error(&res, "Expected a non-empty list");
        }
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_length(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "length", 1))
        init_minim_object(&res, MINIM_OBJ_NUM, minim_list_length(args[0]));

    free_minim_objects(argc, args);
    return res;
}