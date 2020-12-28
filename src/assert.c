#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "assert.h"

// ***** Visible functions ***** //

//  Arity

bool assert_min_argc(MinimObject** ret, const char *op, size_t expected, size_t actual)
{
    if (actual < expected)
    {
        if (expected == 1)  minim_error(ret, "Expected at least 1 argument for '%s'", op);
        else                minim_error(ret, "Expected at least %d arguments for '%s'", expected, op);
        return false;
    } 

    return true;
}

bool assert_exact_argc(MinimObject** ret, const char *op, size_t expected, size_t actual)
{
    if (actual != expected)
    {
        if (expected == 1)  minim_error(ret, "Expected 1 argument for '%s'", op);
        else                minim_error(ret, "Expected %d arguments for '%s'", expected, op);
        return false;
    } 

    return true;
}

bool assert_range_argc(MinimObject** ret, const char *op, size_t min, size_t max, size_t actual)
{
    if (actual < min || actual > max)
    {
        minim_error(ret, "Expected between %d and %d arguments for '%s'", min, max, op);
        return false;
    } 

    return true;
}

bool assert_for_all(MinimObject **ret, MinimObject **args, size_t argc, const char *msg, MinimPred pred)
{
    for (size_t i = 0; i < argc; ++i)
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