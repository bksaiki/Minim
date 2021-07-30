#include <ctype.h>
#include <string.h>
#include "../build/config.h"
#include "minim.h"
#include "read.h"

int minim_run_file(const char *str, uint32_t flags)
{
    MinimEnv *env;
    MinimObject *err;
    int status;

    init_env(&env, NULL, NULL);
    minim_load_builtins(env);
    if (!(flags & MINIM_FLAG_LOAD_LIBS))
    {
        if (minim_load_library(env))
            return 1;
    }

    status = minim_load_file(env, str, &err);
    if (status > 0)
    {
        PrintParams pp;

        set_default_print_params(&pp);
        print_minim_object(err, env, &pp);
        printf("\n");
        return 2;
    }

    return status;
}

int minim_load_library(MinimEnv *env)
{
    MinimObject *err;

    if (minim_load_file(env, MINIM_LIB_PATH "lib/base.min", &err))
    {
        PrintParams pp;

        set_default_print_params(&pp);
        print_minim_object(err, env, &pp);
        printf("\n");
        return 1;
    }

    return 0;
}
