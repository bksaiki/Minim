#ifndef _MINIM_JMP_H_
#define _MINIM_JMP_H_

#include "env.h"

NORETURN
void minim_long_jump(MinimObject *jmp, MinimEnv *env, size_t argc, MinimObject **args);

#endif
