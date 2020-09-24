#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "math.h"
#include "util.h"

static int is_num_pred(const void *thing)
{
    MinimObject **pobj = (MinimObject**) thing;
    return ((*pobj)->type == MINIM_OBJ_NUM);
}

static bool assert_numerical_args(int argc, MinimObject **args, MinimObject** ret, const char* op)
{
    char msg[100];

    if (!all_of(args, argc, sizeof(MinimObject*), is_num_pred))
    {
        strcpy(msg, "Expected numerical arguments for ");
        strcat(msg, op);
        init_minim_object(ret, MINIM_OBJ_ERR, msg);
        return false;
    }

    return true;
}

static bool assert_min_argc(int argc, MinimObject **args, MinimObject** ret, const char* op, int min)
{
    char msg[100];
    char num[10];

    if (argc < min)
    {
        sprintf(num, "%d", min);
        strcpy(msg, "Expected at least ");
        strcpy(msg, num);
        if (min == 1)   strcpy(msg, " argument for ");
        else            strcpy(msg, " arguments for ");
        strcpy(msg, op);

        init_minim_object(ret, MINIM_OBJ_ERR, msg);
        return false;
    } 

    return true;
}

static bool assert_exact_argc(int argc, MinimObject **args, MinimObject** ret, const char* op, int count)
{
    char msg[100];
    char num[10];

    if (argc != count)
    {
        sprintf(num, "%d", count);
        strcpy(msg, "Expected ");
        strcpy(msg, num);
        if (count == 1)     strcpy(msg, " argument for ");
        else                strcpy(msg, " arguments for ");
        strcpy(msg, op);

        init_minim_object(ret, MINIM_OBJ_ERR, msg);
        return false;
    } 

    return true;
}

//
//  Visible functions
//

MinimObject *minim_builtin_add(int argc, MinimObject** args)
{
    MinimObject *res;

    if (!assert_numerical_args(argc, args, &res, "+") ||
        !assert_min_argc(argc, args, &res, "+", 1))
    {
        free_minim_objects(argc, args);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum += *pval;
        }
    }

    return res;
}

MinimObject *minim_builtin_sub(int argc, MinimObject** args)
{
    MinimObject *res;

    if (!assert_numerical_args(argc, args, &res, "-") ||
        !assert_min_argc(argc, args, &res, "-", 1))
    {
        free_minim_objects(argc, args);
    }
    else if (argc == 1)
    {
        init_minim_object(&res, MINIM_OBJ_NUM, - *((int*)args[0]->data));
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum -= *pval;
        }
    }

    return res;
}

MinimObject *minim_builtin_mul(int argc, MinimObject** args)
{
    MinimObject *res;

    if (!assert_numerical_args(argc, args, &res, "*") ||
        !assert_min_argc(argc, args, &res, "*", 1))
    {
        free_minim_objects(argc, args);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_NUM, *((int*)args[0]->data));
        for (int i = 1; i < argc; ++i)
        {
            int *accum = (int*)res->data;
            int *pval = (int*)args[i]->data;
            *accum *= *pval;
        }
    }

    return res;
}

MinimObject *minim_builtin_div(int argc, MinimObject** args)
{
    MinimObject *res;

    if (!assert_numerical_args(argc, args, &res, "/") ||
        !assert_exact_argc(argc, args, &res, "/", 2))
    {
        free_minim_objects(argc, args);
    }
    else if (*((int*) args[1]->data) == 0)
    {
        free_minim_objects(argc, args);
        init_minim_object(&res, MINIM_OBJ_ERR, "Divison by zero");
    }
    else
    {
        int num = *((int*)args[0]->data);
        int den = *((int*)args[1]->data);

        init_minim_object(&res, MINIM_OBJ_NUM, num / den);
    }

    return res;
}
