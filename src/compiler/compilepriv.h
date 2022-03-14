#ifndef _MINIM_COMPILER_PRIV_H_
#define _MINIM_COMPILER_PRIV_H_

#include "../core/minimpriv.h"

// register strings
#define REG_TC_STR  "$tc"
#define REG_RT_STR  "$rt"
#define REG_R0_STR  "$r0"
#define REG_R1_STR  "$r1"
#define REG_R2_STR  "$r2"
#define REG_T0_STR  "$r3"
#define REG_T1_STR  "$r4"
#define REG_T2_STR  "$r5"
#define REG_T3_STR  "$r6"

// register indexes
#define REG_RT      0
#define REG_R0      1
#define REG_R1      2
#define REG_R2      3
#define REG_T0      4
#define REG_T1      5
#define REG_T2      6
#define REG_T3      7

// ignore REG_TC
#define REGISTER_COUNT  8

typedef struct Function {
    MinimObject *pseudo, *pseudo_it;
    MinimObject *ret_sym;
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

// Applies register allocation.
void function_register_allocation(MinimEnv *env, Function *func);

// Prints out a function pseudocode
void debug_function(MinimEnv *env, Function *func);

// Adds a function to the compiler.
void compiler_add_function(Compiler *compiler, Function *func);

// Returns true if the reference is a function argument
bool is_argument_location(MinimObject *obj);

#endif
