#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "assert.h"
#include "util.h"

static int is_num_pred(const void *thing)
{
    MinimObject **pobj = (MinimObject**) thing;
    return ((*pobj)->type == MINIM_OBJ_NUM);
}

// ***** Visible functions ***** //

bool assert_numerical_args(int argc, MinimObject **args, MinimObject** ret, const char* op)
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

bool assert_pair_arg(MinimObject *arg, MinimObject** ret, const char* op)
{
    char msg[100];

    if (arg->type != MINIM_OBJ_PAIR)
    {
        strcpy(msg, "Expected a pair for ");
        strcpy(msg, op);
        init_minim_object(ret, MINIM_OBJ_ERR, msg);
        return false;
    }

    return true;
}

bool assert_min_argc(int argc, MinimObject **args, MinimObject** ret, const char* op, int min)
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

bool assert_exact_argc(int argc, MinimObject **args, MinimObject** ret, const char* op, int count)
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