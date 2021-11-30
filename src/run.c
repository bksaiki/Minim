#include <ctype.h>
#include <setjmp.h>
#include <string.h>

#include "../build/config.h"
#include "core/minim.h"
#include "run.h"

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

    // set up globals
    init_global_state(
        IF_FLAG_RAISED(flags, MINIM_FLAG_COMPILE, GLOBAL_FLAG_COMPILE, 0x0) |
        IF_FLAG_RAISED(flags, MINIM_FLAG_NO_CACHE, 0x0, GLOBAL_FLAG_CACHE)
    );
    init_builtins();

    // set up ports
    minim_error_port = minim_file_port(stderr, MINIM_PORT_MODE_WRITE |
                                               MINIM_PORT_MODE_OPEN |
                                               MINIM_PORT_MODE_READY);
    minim_output_port = minim_file_port(stdout, MINIM_PORT_MODE_WRITE |
                                                MINIM_PORT_MODE_OPEN |
                                                MINIM_PORT_MODE_READY);
    minim_input_port = minim_file_port(stdin, MINIM_PORT_MODE_READ |
                                              MINIM_PORT_MODE_OPEN |
                                              MINIM_PORT_MODE_READY |
                                              MINIM_PORT_MODE_ALT_EOF);

    MINIM_PORT_NAME(minim_input_port) = "stdin";
    GC_register_root(minim_error_port);
    GC_register_root(minim_output_port);
    GC_register_root(minim_input_port);

    set_default_print_params(&pp);
    exit_buf = GC_alloc_atomic(sizeof(jmp_buf));
    exit_handler = minim_jmp(exit_buf, NULL);
    if (setjmp(*exit_buf) == 0)
    {
        MinimPath *path;

        minim_exit_handler = exit_handler;
        minim_error_handler = exit_handler;
        init_env(&env, NULL, NULL);
        if (!(flags & MINIM_FLAG_LOAD_LIBS))
            minim_load_library(env);

        path = (is_absolute_path(str) ?
                build_path(1, str) :
                build_path(2, get_current_dir(), str));
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
