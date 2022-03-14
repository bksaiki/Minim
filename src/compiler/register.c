#include "../core/minimpriv.h"
#include "compilepriv.h"

// register replacement policy
#define REG_REPLACE_TEMP_ONLY   0
#define REG_REPLACE_TEMP_ARG    1
#define REG_REPLACE_EXCEPT_TC   2

static void
insert_empty_into_list(MinimObject *before)
{
    MINIM_CDR(before) = minim_cons(minim_null, MINIM_CDR(before));
}

static MinimObject *
reserve_register(MinimEnv *env,
                 MinimObject *regs,
                 MinimSymbolTable *table,
                 uint8_t reg)
{
    if (!minim_falsep(MINIM_VECTOR_REF(regs, reg)))
    {
        printf("unimplemented [reserving occupied register]\n");
        return intern(REG_RT_STR);
    }
    else
    {
        MINIM_VECTOR_REF(regs, reg) = intern(get_register_string(reg));
        return MINIM_VECTOR_REF(regs, reg);
    }
}

static MinimObject *
fresh_register(MinimEnv *env,
               MinimObject *regs,
               MinimSymbolTable *table,
               uint8_t policy)
{
    for (uint8_t i = REG_T0; i <= REG_T3; i++)
    {
        if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
            return reserve_register(env, regs, table, i);
    }

    if (policy != REG_REPLACE_TEMP_ONLY)
    {
        for (uint8_t i = REG_R0; i <= REG_R2; i++)
        {
            if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
                return reserve_register(env, regs, table, i);
        }

        if (policy != REG_REPLACE_TEMP_ARG)
        {
            if (minim_falsep(MINIM_VECTOR_REF(regs, REG_RT)))
                return reserve_register(env, regs, table, REG_RT);
        }
    }

    printf("unimplemented [requesting fresh register when all are taken]\n");
    return intern(REG_RT_STR);
}

static void
unreserve_register(MinimEnv *env,
                  MinimObject *regs,
                  MinimSymbolTable *table,
                  uint8_t reg)
{
    MinimObject *ref = MINIM_VECTOR_REF(regs, reg);
    if (!minim_falsep(ref))
        minim_symbol_table_set(table, MINIM_SYMBOL(ref),
                               hash_symbol(MINIM_SYMBOL(ref)), minim_false);
    MINIM_VECTOR_REF(regs, reg) = minim_false;
}


static MinimObject *
call_instruction(MinimObject *target)
{
    return minim_ast(
        minim_cons(minim_ast(intern("$call"), NULL),
        minim_cons(target, minim_null)),
        NULL);
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

            insert_empty_into_list(it);
            it = MINIM_CDR(it);
            
            //  ($call $rt)
            MINIM_CAR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL));
            insert_empty_into_list(it);
            it = MINIM_CDR(it);

            //  ($mov $tc $rt)
            MINIM_CAR(it) = move_instruction(
                    minim_ast(intern(REG_TC_STR), NULL),
                    minim_ast(intern(REG_RT_STR), NULL));

            unreserve_register(env, regs, table, REG_RT);
            unreserve_register(env, regs, table, REG_R0);
        }
        else if (minim_eqp(op, intern("$arg")))
        {
            MinimObject *name, *offset;
            size_t idx;

            name = MINIM_STX_VAL(MINIM_CADR(line));
            offset = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            idx = MINIM_NUMBER_TO_UINT(offset);

            switch (idx)
            {
            case 0:
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_R0_STR));
                MINIM_VECTOR_REF(regs, REG_R0) = name;
                break;

            case 1:
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_R1_STR));
                MINIM_VECTOR_REF(regs, REG_R1) = name;
                break;

            case 2:
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_R2_STR));
                MINIM_VECTOR_REF(regs, REG_R2) = name;
                break;
            
            default:
                printf("unimplemented [args in memory]\n");
                continue;
            }
        }
        else if (minim_eqp(op, intern("$set")))
        {
            // ($set <t> ($eval ...)) =>
            //
            // ($set <t> ($top <v>)) =>
            //  t = get_top(v)
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
                printf("unimplemented [move between registers]\n");
                debug_print_minim_object(line, env);
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

                insert_empty_into_list(it);
                it = MINIM_CDR(it);

                // ($mov $r0 ($syntax <quote-syntax>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(intern(REG_R0_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$syntax"), NULL),
                        minim_cons(stx, minim_null)),
                        NULL));
                insert_empty_into_list(it);
                it = MINIM_CDR(it);

                //  ($call $rt)
                MINIM_CAR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL));
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$top")))
            {
                // TODO: top-level symbols should be checked before compilation
                MinimObject *sym, *reg;
                
                sym = MINIM_STX_VAL(MINIM_CADR(val));
                reg = fresh_register(env, regs, table, REG_REPLACE_TEMP_ONLY);
                minim_symbol_table_add(table, MINIM_SYMBOL(sym), reg);

                // ($mov $rt ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(sym, NULL),
                        minim_null)),
                        NULL));
            }
            else
            {
                printf("unimplemented [other forms of set]\n");
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
                printf("unimplemented [set destination is not RT]\n");
                continue;
            }
        }
        else if (minim_eqp(op, intern("$return")))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(line)));

            if (!ref)
            {
                printf("unimplemented [temp]\n");
                continue;
            }
            else if (!minim_eqp(ref, intern(REG_RT_STR)))
            {
                MINIM_CAR(it) = minim_ast(
                    minim_cons(minim_ast(intern("$mov"), NULL),
                    minim_cons(minim_ast(intern(REG_RT_STR), NULL),
                    minim_cons(minim_ast(ref, NULL),
                    minim_null))),
                    NULL);
                
                insert_empty_into_list(it);
                it = MINIM_CDR(it);
            }
            
            MINIM_CAR(it) = minim_ast(
                    minim_cons(minim_ast(intern("$ret"), NULL),
                    minim_null),
                    NULL);
        }
    }
}

const char *get_register_string(uint8_t reg)
{
    switch (reg)
    {
    case REG_RT:    return REG_RT_STR;
    case REG_R0:    return REG_R0_STR;
    case REG_R1:    return REG_R1_STR;
    case REG_R2:    return REG_R2_STR;
    case REG_T0:    return REG_T0_STR;
    case REG_T1:    return REG_T1_STR;
    case REG_T2:    return REG_T2_STR;
    case REG_T3:    return REG_T3_STR;
    default:        return NULL;
    }
}
