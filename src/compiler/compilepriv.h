#ifndef _MINIM_COMPILER_PRIV_H_
#define _MINIM_COMPILER_PRIV_H_

#include "../core/minimpriv.h"

// ================================ Symbol Tables ================================

// // tuple that associates a var name to properties
// typedef struct VarEntry {
//     char *name;
// } VarEntry;

// // maps variables to locations
// typedef struct VarTable {
//     VarEntry *vars;
//     size_t count;
//     size_t set;
// } VarTable;

// typedef struct TopLevel {
//     VarTable vars;
// } TopLevel;

// typedef struct SymbolTable {
//     TopLevel tl;
// } SymbolTable;

// ================================ Top-level ================================

// // returns the number of top-level definitions
// size_t top_level_def_count(MinimEnv *env, MinimModule *module);

typedef struct Function {
    MinimObject *pseudo, *pseudo_it;
    char *name;
    void *code;
    uint32_t argc;
    bool variary;
} Function;

typedef struct Compiler {
    Function **funcs;
    Function *curr_func;
    MinimSymbolTable *table;
    size_t func_count;
} Compiler;

// Initializes a function structure.
void init_function(Function **pfunc);

// Adds a line of pseudo-assembly to the function
void function_add_line(Function *func, MinimObject *instr);

// Adds a function to the compiler.
void compiler_add_function(Compiler *compiler, Function *func);

#endif
