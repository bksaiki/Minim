#ifndef _MINIM_MODULE_H_
#define _MINIM_MODULE_H_

#include "ast.h"
#include "env.h"

#define MINIM_MODULE_LOADED     0x1
#define MINIM_MODULE_INIT       0x2

/*
    Module / Environment organization

                               top level      call stack grows -->
    +---------------+           +-----+           +-----+       
    |    Module     |   <---->  | Env |  <------  | Env |  <---- ....
    +---------------+           +-----+           +-----+ 
            |
    +---------------+           +-----+
    |    Module     |   <---->  | Env | 
    +---------------+           +-----+
            |
           ...
         imports

    Modules contain top level environments.
    When a new environment is created, it points to previous environment.
    Imported modules are considered accessible to modules above

    Any identifier is valid in an environment if
      (a) it is stored directly in that environment
      (b) there exists a path to the environment that contains it
*/

typedef struct MinimModule
{
    struct MinimModule *prev;
    SyntaxNode **exprs;
    size_t exprc;
    MinimEnv *env, *import;
    size_t flags;
} MinimModule;

void init_minim_module(MinimModule **pmodule);
void minim_module_add_expr(MinimModule *module, SyntaxNode *expr);

#endif
