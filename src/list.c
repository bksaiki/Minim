#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assert.h"
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

MinimObject *minim_builtin_first(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, **pair;

    if (!assert_exact_argc(argc, args, &res, "first", 1))
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

MinimObject *minim_builtin_last(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, *it, **pair;

    if (assert_exact_argc(argc, args, &res, "first", 1))
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
    MinimObject *res, *it;
    int len = 0;

    if (assert_exact_argc(argc, args, &res, "length", 1))
    {
        it = args[0];
        while (it != NULL)
        {
            if (MINIM_CAR(it))  ++len;
            it = MINIM_CDR(it);
        }

        init_minim_object(&res, MINIM_OBJ_NUM, len);
    }

    free_minim_objects(argc, args);
    return res;
}