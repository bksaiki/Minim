#ifndef _MINIM_COMPILER_PRIV_H_
#define _MINIM_COMPILER_PRIV_H_

#include "../core/minimpriv.h"

// ================================ Top-level ================================

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

// Optimizes a function.
void function_optimize(MinimEnv *env, Function *func);

// Desugars higher-level psuedo-instructions.
void function_desugar(MinimEnv *env, Function *func);

// Applies register allocation.
void function_register_allocation(MinimEnv *env, Function *func);

// Prints out a function pseudocode
void debug_function(MinimEnv *env, Function *func);

// Adds a function to the compiler.
void compiler_add_function(Compiler *compiler, Function *func);

#endif
