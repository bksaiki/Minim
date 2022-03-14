#include "../core/minimpriv.h"
#include "compilepriv.h"

// register replacement policy
#define REG_REPLACE_TEMP_ONLY   0
#define REG_REPLACE_TEMP_ARG    1
#define REG_REPLACE_EXCEPT_TC   2

static bool
free_register_exists(MinimObject *regs,
                     uint8_t policy)
{
    for (uint8_t i = REG_T0; i <= REG_T3; i++)
    {
        if (!minim_falsep(MINIM_VECTOR_REF(regs, i)))
            return true;
    }

    if (policy == REG_REPLACE_TEMP_ONLY)
        return false;

    for (uint8_t i = REG_R0; i <= REG_R2; i++)
    {
        if (!minim_falsep(MINIM_VECTOR_REF(regs, i)))
            return true;
    }

    if (policy == REG_REPLACE_TEMP_ARG)
        return false;

    return !minim_falsep(MINIM_VECTOR_REF(regs, REG_RT));
}

static MinimObject *
fresh_register(MinimEnv *env,
               MinimObject *regs,
               MinimSymbolTable *table)
{
    return NULL;
}

static void
reserve_register(MinimEnv *env,
                 MinimObject *regs,
                 MinimSymbolTable *table,
                 uint8_t reg)
{
    if (!minim_falsep(MINIM_VECTOR_REF(regs, reg)))
    {
        printf("unimplemented!!!");
    }
}

static MinimObject *
call_instruction(MinimObject *target, MinimObject *next)
{
    return minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$call"), NULL),
            minim_cons(target, minim_null)),
            NULL),
        next);
}

static MinimObject *
move_instruction(MinimObject *dest, MinimObject *src)
{
    return minim_ast(
            minim_cons(minim_ast(intern("$mov"), NULL),
            minim_cons(dest,
            minim_cons(src,
            minim_null))),
            NULL);
}

static MinimObject *
move_instruction2(MinimObject *dest, MinimObject *src, MinimObject *next)
{
    return minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$mov"), NULL),
            minim_cons(dest,
            minim_cons(src,
            minim_null))),
            NULL),
        next);
}

void function_register_allocation(MinimEnv *env, Function *func)
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
            // =>
            //  $tc = init_env($tc)
            reserve_register(env, regs, table, REG_RT);
            reserve_register(env, regs, table, REG_R0);

            //  ($mov $rt ($addr <init_env>))
            MINIM_CAR(it) = move_instruction(
                minim_ast(intern(REG_RT_STR), NULL),
                minim_ast(
                    minim_cons(minim_ast(intern("$addr"), NULL),
                    minim_cons(minim_ast(intern("init_env"), NULL),
                    minim_null)),
                    NULL));
            
            //  ($call $rt)
            MINIM_CDR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL), MINIM_CDR(it));
            it = MINIM_CDR(it);

            //  ($mov $tc $rt)
            MINIM_CDR(it) = move_instruction2(
                    minim_ast(intern(REG_TC_STR), NULL),
                    minim_ast(intern(REG_RT_STR), NULL),
                    MINIM_CDR(it));
            it = MINIM_CDR(it);
        }
        else if (minim_eqp(op, intern("$set")))
        {
            // ($set <t> ($eval ...)) =>
            //
            // ($set <t> ($top <v>)) =>
            //
            // ($set <t> ($load <v>)) =>
            //
            // ($set <t> ($quote <v>)) =>
            //  t = quote(v)
            // ($set <t> <v>) =>
            //  t = v
            
            MinimObject *name, *val;
           
            val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            if (MINIM_OBJ_SYMBOLP(val))
            {
                printf("unimplemented\n");
                continue;
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$quote")))
            {
                MinimObject *stx;

                reserve_register(env, regs, table, REG_RT);
                reserve_register(env, regs, table, REG_R0);
                stx = MINIM_CADR(val);

                // ($mov $rt ($addr <quote>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(intern(REG_RT_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(intern("quote"), NULL),
                        minim_null)),
                        NULL));

                // ($mov $r0 ($syntax <quote-syntax>))
                MINIM_CDR(it) = move_instruction2(
                    minim_ast(intern(REG_R0_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$syntax"), NULL),
                        minim_cons(stx, minim_null)),
                        NULL),
                    MINIM_CDR(it));
                it = MINIM_CDR(it);

                //  ($call $rt)
                MINIM_CDR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL), MINIM_CDR(it));
                it = MINIM_CDR(it);
            }
            else
            {
                printf("unimplemented\n");
                continue;
            }

            name = MINIM_STX_VAL(MINIM_CADR(line));
            if (minim_eqp(name, func->ret_sym))
            {
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                MINIM_VECTOR_REF(regs, REG_RT) = name;
            }
            else
            {
                printf("unimplemented\n");
                continue;
            }
        }
        else if (minim_eqp(op, intern("$return")))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(line)));
            if (minim_eqp(ref, intern(REG_RT_STR)))
            {
                MINIM_CAR(it) = minim_ast(
                    minim_cons(minim_ast(intern("$ret"), NULL),
                    minim_null),
                    NULL);
            }
            else
            {
                printf("unimplemented\n");
                continue;
            }
        }
    }
}
