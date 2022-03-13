#include "../core/minimpriv.h"
#include "compilepriv.h"

// register strings
#define REG_TC      "$tc"
#define REG_R0      "$r0"
#define REG_R1      "$r1"
#define REG_R2      "$r2"
#define REG_T0      "$r3"
#define REG_T1      "$r4"
#define REG_T2      "$r5"
#define REG_T3      "$r6"

// register indexes
#define REG_R0_IDX      0
#define REG_R1_IDX      1
#define REG_R2_IDX      2
#define REG_T0_IDX      3
#define REG_T1_IDX      4
#define REG_T2_IDX      5
#define REG_T3_IDX      6

// ignore REG_TC
#define REGISTER_COUNT  7

static MinimObject *fresh_register(MinimEnv *env, MinimObject *regs, MinimSymbolTable *table)
{
    return NULL;
}

void function_desugar(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *regs;

    init_minim_symbol_table(&table);
    regs = minim_vector2(REGISTER_COUNT, minim_false);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$push-env")))
        {

        }
    }
}


void function_register_allocation(MinimEnv *env, Function *func)
{
    
}
