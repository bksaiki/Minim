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

typedef struct MinimModuleCache MinimModuleCache;

typedef struct MinimModule
{
    struct MinimModule *prev;
    struct MinimModuleCache *cache;
    struct MinimModule **imports;
    SyntaxNode **exprs;
    size_t exprc, importc;
    MinimEnv *env, *export;
    char *name;
    size_t flags;
} MinimModule;

typedef struct MinimModuleCache
{
    struct MinimModule** modules;
    size_t modulec;
} MinimModuleCache;

void init_minim_module(MinimModule **pmodule, MinimModuleCache *cache);
void copy_minim_module(MinimModule **pmodule, MinimModule *src);
void minim_module_add_expr(MinimModule *module, SyntaxNode *expr);
void minim_module_add_import(MinimModule *module, MinimModule *import);

MinimObject *minim_module_get_sym(MinimModule *module, const char *sym);
MinimModule *minim_module_get_import(MinimModule *module, const char *sym);

void init_minim_module_cache(MinimModuleCache **pcache);
void minim_module_cache_add(MinimModuleCache *cache, MinimModule *import);
MinimModule *minim_module_cache_get(MinimModuleCache *cache, const char *sym);

#endif
