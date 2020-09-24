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
    else
    {
        pair = ((MinimObject**) args[0]->data);
        res = pair[0];
        free_minim_object(pair[1]);

        pair[0] = NULL;
        pair[1] = NULL;
        free_minim_object(args[0]);
    }

    free(args);
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
    else
    {
        pair = ((MinimObject**) args[0]->data);
        free_minim_object(pair[0]);
        res = pair[1];

        pair[0] = NULL;
        pair[1] = NULL;
        free_minim_object(args[0]);
    }

    free(args);
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