#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "assert.h"

// ***** Visible functions ***** //

//  Arity

bool assert_min_argc(int argc, MinimObject** ret, const char* op, int min)
{
    if (argc < min)
    {
        if (min == 1)   minim_error(ret, "Expected at least 1 argument for '%s'", op);
        else            minim_error(ret, "Expected at least %d arguments for '%s'", min, op);
        return false;
    } 

    return true;
}

bool assert_exact_argc(int argc, MinimObject** ret, const char* op, int count)
{
    if (argc != count)
    {
        if (count == 1) minim_error(ret, "Expected 1 argument for '%s'", op);
        else            minim_error(ret, "Expected %d arguments for '%s'", count, op);
        return false;
    } 

    return true;
}

bool assert_range_argc(int argc, MinimObject** ret, const char* op, int min, int max)
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

bool assert_generic(MinimObject **ret, const char *msg, bool pred)
{
    if (!pred)  minim_error(ret, "%s", msg);
    
    return pred;
}