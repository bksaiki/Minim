#ifndef _MINIM_H_
#define _MINIM_H_

#include "common/buffer.h"
#include "common/common.h"
#include "core/ast.h"
#include "core/builtin.h"
#include "core/env.h"
#include "core/eval.h"
#include "core/module.h"
#include "core/parser.h"
#include "core/print.h"
#include "gc/gc.h"

#define MINIM_FLAG_LOAD_LIBS        0x1
#define MINIM_FLAG_NO_RUN           0x2

// Runs a file under the current environment 'env'
// `perr` will point to an error if `perr` is NULL when
// called and an error occured
int minim_load_file(MinimEnv *env, const char *str, MinimObject **perr);

#endif
