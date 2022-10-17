#include "minimpriv.h"

void minim_long_jump(MinimObject *jmp, MinimEnv *env, size_t argc, MinimObject **args)
{
    jmp_buf *buf = MINIM_JUMP_BUF(jmp);

    // save result
    if (argc == 0)          MINIM_JUMP_VAL(jmp) = minim_void;
    else if (argc == 1)     MINIM_JUMP_VAL(jmp) = args[0];
    else                    MINIM_JUMP_VAL(jmp) = minim_values(argc, args);

    // jump
    longjmp(*buf, 1);
}
