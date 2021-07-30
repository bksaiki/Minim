#ifndef _MINIM_MODULE_H_
#define _MINIM_MODULE_H_

#include "ast.h"
#include "env.h"

#define MINIM_MODULE_LOADED     0x1
#define MINIM_MODULE_INIT       0x2

typedef struct MinimModule
{
    SyntaxNode **exprs;
    size_t exprc;
    MinimEnv *env;
    size_t flags;
} MinimModule;

void init_minim_module(MinimModule **pmodule);
void minim_module_add_expr(MinimModule *module, SyntaxNode *expr);

#endif
