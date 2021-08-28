#include <ctype.h>
#include <setjmp.h>
#include <string.h>

#include "../build/config.h"
#include "minim.h"
#include "read.h"

static bool is_int8(MinimObject *thing)
{
    MinimObject *limit;

    if (!MINIM_OBJ_NUMBERP(thing) || !minim_exact_nonneg_intp(thing))
        return false;

    limit = uint_to_minim_number(256);
    return minim_number_cmp(thing, limit) < 0;
}

int minim_run(const char *str, uint32_t flags)
{
    PrintParams pp;
    MinimEnv *env;
    MinimObject *exit_handler;
    jmp_buf *exit_buf;

    set_default_print_params(&pp);
    exit_buf = GC_alloc_atomic(sizeof(jmp_buf));
    exit_handler = minim_jmp(exit_buf, NULL);
    if (setjmp(*exit_buf) == 0)
    {
        MinimPath *path;

        minim_exit_handler = exit_handler;
        minim_error_handler = exit_handler;
        init_env(&env, NULL, NULL);
        minim_load_builtins(env);
        if (!(flags & MINIM_FLAG_LOAD_LIBS))
            minim_load_library(env);

        path = build_path(2, get_working_directory(), str);
        minim_load_file(env, extract_path(path));
    }
    else
    {
        MinimObject *thrown = MINIM_JUMP_VAL(exit_handler);
        if (MINIM_OBJ_ERRORP(thrown))
        {
            print_minim_object(thrown, env, &pp);
            printf("\n");
            return 0;
        }

        return is_int8(thrown) ? MINIM_NUMBER_TO_UINT(thrown): 0;
    }

    return 0;
}

void minim_load_library(MinimEnv *env)
{
    minim_run_file(env, MINIM_LIB_PATH "lib/base.min");
}
