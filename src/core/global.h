#ifndef _MINIM_GLOBAL_H_
#define _MINIM_GLOBAL_H_

#include "../common/system.h"
#include "intern.h"
#include "symbols.h"

#define GLOBAL_FLAG_DEFAULT     0x0
#define GLOBAL_FLAG_COMPILE     0x1
#define GLOBAL_FLAG_CACHE       0x2

typedef struct MinimModuleCache MinimModuleCache;
typedef struct MinimSymbolTable MinimSymbolTable;
typedef struct MinimInternTable MinimInternTable;

// global object
extern struct MinimGlobal
{
    // state
    MinimModuleCache *cache;            // cache of loaded modules
    MinimSymbolTable *builtins;         // primitive table
    InternTable *symbols;               // symbol table
    char *current_dir;                  // directory where Minim was started
    pid_t pid;                          // primary thread id

    // statistics
    size_t stat_exprs;
    size_t stat_procs;
    size_t stat_objs;

    // flags
    uint8_t flags;
} global;

// initialize builtins, intern table, other info
void init_global_state(uint8_t flags);

// setters
#define intern(s)           intern_symbol(global.symbols, s)
#define set_builtin(n, o)   minim_symbol_table_add(global.builtins, n, o)

// modifiers
#if MINIM_TRACK_STATS == 1
#define log_expr_evaled()     ++global.stat_exprs
#define log_proc_called()     ++global.stat_procs
#define log_obj_created()     ++global.stat_objs
#else
#define log_expr_evaled()
#define log_proc_called()
#define log_obj_created()
#endif

#endif
