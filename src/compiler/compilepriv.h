#ifndef _MINIM_COMPILER_PRIV_H_
#define _MINIM_COMPILER_PRIV_H_

#include "../core/minimpriv.h"

// Register types
//  - TC: continuation register (usually corresponds to the first argument register)
//  - RT: result register
//  - BP: base register
//  - SP: stack register
//  - Rx: argument register
//  - Tx: scratch register (must save to stack before use)

// number of argument registers
//  - includes the tc register
//  - affects compilation especially stashing
#define USE_4_ARG_REGS      1

#if USE_4_ARG_REGS
    // argument register strings
    #define REG_R0_STR  "$r0"
    #define REG_R1_STR  "$r1"
    #define REG_R2_STR  "$r2"

    // temporary register strings
    #define REG_T0_STR  "$r3"
    #define REG_T1_STR  "$r4"
    #define REG_T2_STR  "$r5"
    #define REG_T3_STR  "$r6"

    // other register strings
    #define REG_TC_STR  "$tc"
    #define REG_RT_STR  "$rt"
    #define REG_BP_STR  "$bp"
    #define REG_SP_STR  "$sp"
    
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
    #define REGISTER_COUNT              8
    #define ARG_REGISTER_COUNT          (REG_R2 - REG_RT)
    #define SCRATCH_REGISTER_COUNT      (REG_T3 - REG_R2)
#else
    // argument register strings
    #define REG_R0_STR  "$r0"
    #define REG_R1_STR  "$r1"
    #define REG_R2_STR  "$r2"
    #define REG_R1_STR  "$r3"
    #define REG_R2_STR  "$r4"

    // temporary register strings
    #define REG_T0_STR  "$r5"
    #define REG_T1_STR  "$r6"
    #define REG_T2_STR  "$r7"
    #define REG_T3_STR  "$r8"

    // other register strings
    #define REG_TC_STR  "$tc"
    #define REG_RT_STR  "$rt"
    #define REG_BP_STR  "$bp"
    #define REG_SP_STR  "$sp"
    
    // register indexes
    #define REG_RT      0
    #define REG_R0      1
    #define REG_R1      2
    #define REG_R2      3
    #define REG_R3      4
    #define REG_R4      5
    #define REG_T0      6
    #define REG_T1      7
    #define REG_T2      8
    #define REG_T3      9

    // ignore REG_TC
    #define REGISTER_COUNT              10
    #define ARG_REGISTER_COUNT          (REG_R4 - REG_RT)
    #define SCRATCH_REGISTER_COUNT      (REG_T3 - REG_R4)
#endif

typedef struct Function {
    MinimObject *pseudo, *pseudo_it;        // code
    MinimObject *ret_sym;                   // return symbol for translation/optimization pass
    MinimObject *stash;                     // scratch register / memory use
    MinimObject *calls;                     // list of calls
    char *name;
    void *code;
    uint32_t argc;
} Function;

typedef struct Compiler {
    Function **funcs;
    Function *curr_func;
    MinimSymbolTable *table;
    size_t func_count;
} Compiler;

// Register Allocation
typedef struct RegAllocData {
    MinimEnv *env;                  // debugging
    Function *func;                 // current function
    MinimSymbolTable *table;        // map from names to locations
    MinimSymbolTable *tail_syms;    // table of tail-position symbols
    MinimObject *regs;              // register locations
    MinimObject *memory;            // memory location
    MinimObject *joins;             // join data
    MinimObject *last_uses;         // last use data
    MinimObject **prev;             // reference to previous instruction pointer
} RegAllocData;

// Initializes a function structure.
void init_function(Function **pfunc);

// Adds a line of pseudo-assembly to the function
void function_add_line(Function *func, MinimObject *instr);

// Optimizes a function.
void function_optimize(MinimEnv *env, Function *func);

// Applies register allocation.
void function_register_allocation(MinimEnv *env, Function *func);

// Prints out a function pseudocode.
void debug_function(MinimEnv *env, Function *func);

// Adds a function to the compiler.
void compiler_add_function(Compiler *compiler, Function *func);

// Returns a reference to a function in the compiler
Function *compiler_get_function(Compiler *compiler, MinimObject *name);

// Returns true if the reference is a function argument
bool is_argument_location(MinimObject *obj);

// Returns the string associated with a register index.
MinimObject *get_register_symbol(size_t reg);

// Returns the register index associated with a register string
size_t get_register_index(MinimObject *reg);

#endif
