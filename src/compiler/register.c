#include "../core/minimpriv.h"
#include "compilepriv.h"

#define REG_TC      "$tc"
#define REG_R0      "$r0"
#define REG_R1      "$r1"
#define REG_R2      "$r2"
#define REG_T0      "$r3"
#define REG_T1      "$r4"
#define REG_T2      "$r5"
#define REG_T3      "$r6"

void function_desugar(MinimEnv *env, Function *func)
{
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$push-env")))
        {
            // MINIM_CAR(it) = minim_ast(
            //     minim_cons(minim_ast(intern("$mov"), NULL),
            //     minim_cons(minim_ast(intern("$mov"), NULL),
            //     NULL
            // )
        }
    }
}


void function_register_allocation(MinimEnv *env, Function *func)
{
    
}
