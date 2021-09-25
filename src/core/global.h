#ifndef _MINIM_GLOBAL_H_
#define _MINIM_GLOBAL_H_

#include "../common/system.h"

typedef struct MinimModuleCache MinimModuleCache;
typedef struct MinimSymbolTable MinimSymbolTable;
typedef struct MinimInternTable MinimInternTable;

// global object
extern struct MinimGlobal
{
    MinimModuleCache *cache;            // cache of loaded modules
    MinimSymbolTable *builtins;         // primitive table
    InternTable *symbols;               // symbol table
    char *current_dir;                  // directory where Minim was started
    pid_t pid;                          // primary thread id
} global;

// initialize builtins, intern table, other info
void init_global_state();

#endif
