#include <ctype.h>
#include <string.h>
#include "../build/config.h"
#include "minim.h"
#include "read.h"

#define LOAD_FILE(env, file)            \
{                                       \
    if (minim_load_file(env, file))     \
        return 2;                       \
} 

int minim_run_file(const char *str, uint32_t flags)
{
    MinimEnv *env;

    init_env(&env, NULL, NULL);
    minim_load_builtins(env);
    if (!(flags & MINIM_FLAG_LOAD_LIBS))
        minim_load_library(env);

    return minim_load_file(env, str);
}

int minim_load_library(MinimEnv *env)
{
    LOAD_FILE(env, MINIM_LIB_PATH "lib/base.min");
    return 0;
}
