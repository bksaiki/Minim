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

// Single argument type

bool assert_pair_arg(MinimObject *arg, MinimObject** ret, const char* op)
{
    if (arg->type != MINIM_OBJ_PAIR)
    {
        minim_error(ret, "Expected a pair for '%s'", op);
        return false;
    }

    return true;
}

bool assert_sym_arg(MinimObject *arg, MinimObject **ret, const char *op)
{
    if (arg->type != MINIM_OBJ_SYM)
    {
        minim_error(ret, "Expected a symbol for '%s'", op);
        return false;
    }

    return true;
}

//  Multi-argument type

bool assert_numerical_args(int argc, MinimObject **args, MinimObject** ret, const char* op)
{
    if (!all_of(args, argc, sizeof(MinimObject*), is_num_pred))
    {
        minim_error(ret, "Expected numerical arguments for '%s'", op);
        return false;
    }

    return true;
}

//  Arity

bool assert_min_argc(int argc, MinimObject **args, MinimObject** ret, const char* op, int min)
{
    if (argc < min)
    {
        if (min == 1)   minim_error(ret, "Expected at least 1 argument for '%s'", op);
        else            minim_error(ret, "Expected at least %d arguments for '%s'", min, op);
        return false;
    } 

    return true;
}

bool assert_exact_argc(int argc, MinimObject **args, MinimObject** ret, const char* op, int count)
{
    if (argc != count)
    {
        if (count == 1) minim_error(ret, "Expected 1 argument for '%s'", op);
        else            minim_error(ret, "Expected %d arguments for '%s'", count, op);
        return false;
    } 

    return true;
}

bool assert_range_argc(int argc, MinimObject **args, MinimObject** ret, const char* op, int min, int max)
{
    if (argc < min || argc > max)
    {
        minim_error(ret, "Expected between %d and %d arguments for '%s'", min, max, op);
        return false;
    } 

    return true;
}

bool assert_for_all(int argc, MinimObject **args, MinimObject **ret, const char *msg, MinimPred pred)
{
    for (int i = 0; i < argc; ++i)
    {
        if (!pred(args[i]))
        {
            minim_error(ret, "%s", msg);
            return false;
        }
    }

    return true;
}