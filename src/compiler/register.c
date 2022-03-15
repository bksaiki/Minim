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

static void
add_use(MinimSymbolTable *table, MinimObject *last_use_box, MinimObject *stx, MinimObject *sym)
{
    if (!minim_symbol_table_get(table, MINIM_SYMBOL(sym)))
    {
        MinimObject *last_use = MINIM_VECTOR_REF(last_use_box, 0);
        if (!minim_nullp(last_use) && MINIM_CAAR(last_use) == stx)
            MINIM_CDAR(last_use) = minim_cons(sym, MINIM_CDAR(last_use));
        else
            MINIM_VECTOR_REF(last_use_box, 0) = minim_cons(minim_cons(stx, minim_cons(sym, minim_null)), last_use);
    
        minim_symbol_table_add(table, MINIM_SYMBOL(sym), minim_null); 
    }
}

static MinimObject *get_lasts(MinimObject *last_use, MinimObject *stx)
{
    for (MinimObject *it = last_use; !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (MINIM_CAAR(it) == stx)
            return MINIM_CDAR(it);
    }

    return NULL;
}

// Computes the last-use locations of variables.
static MinimObject* compute_last_use(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *reverse, *last_use_box;

    last_use_box = minim_vector2(1, minim_null);
    init_minim_symbol_table(&table);
    reverse = minim_list_reverse(func->pseudo);
    for (MinimObject *it = reverse; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$set")))
        {
            MinimObject *val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            if (MINIM_OBJ_PAIRP(val) && minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$eval")))
            {
                for (MinimObject *it2 = MINIM_CDR(val); !minim_nullp(it2); it2 = MINIM_CDR(it2))
                    add_use(table, last_use_box, MINIM_CAR(it), MINIM_STX_VAL(MINIM_CAR(it2)));
            }
        }
        // else if (minim_eqp(op, intern("$arg")))
        // {
        //     MinimObject *name = MINIM_CADR(line);
        //     add_use(table, last_use, MINIM_CAR(it), MINIM_STX_VAL(ift));
        // }
        else if (minim_eqp(op, intern("$join")))
        {
            MinimObject *ift, *iff;
            
            ift = MINIM_CAR(MINIM_CDDR(line));
            iff = MINIM_CADR(MINIM_CDDR(line));
            add_use(table, last_use_box, MINIM_CAR(it), MINIM_STX_VAL(ift));
            add_use(table, last_use_box, MINIM_CAR(it), MINIM_STX_VAL(iff));
        }
        else if (minim_eqp(op, intern("$bind")) || minim_eqp(op, intern("$gofalse")))
        {
            add_use(table, last_use_box, MINIM_CAR(it), MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line))));
        }
        else if (minim_eqp(op, intern("$return")))
        {
            add_use(table, last_use_box, MINIM_CAR(it), MINIM_STX_VAL(MINIM_CADR(line)));
        }
    }

    return MINIM_VECTOR_REF(last_use_box, 0);
}

static uint8_t
reserve_register(MinimEnv *env,
                 MinimObject *regs,
                 MinimSymbolTable *table,
                 uint8_t reg)
{
    if (!minim_falsep(MINIM_VECTOR_REF(regs, reg)))
    {
        printf("unimplemented [reserving occupied register]\n");
        return REG_RT;
    }
    else
    {
        MINIM_VECTOR_REF(regs, reg) = minim_true;
        return reg;
    }
}

static MinimObject *
fresh_register(MinimEnv *env,
               MinimObject *regs,
               MinimSymbolTable *table,
               MinimObject *sym,
               uint8_t policy)
{
    for (uint8_t i = REG_T0; i <= REG_T3; i++)
    {
        if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
        {
            uint8_t reg = reserve_register(env, regs, table, i);
            MINIM_VECTOR_REF(regs, reg) = sym;
            return intern(get_register_string(reg));
        }
    }

    if (policy != REG_REPLACE_TEMP_ONLY)
    {
        for (uint8_t i = REG_R0; i <= REG_R2; i++)
        {
            if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
            {
                uint8_t reg = reserve_register(env, regs, table, i);
                MINIM_VECTOR_REF(regs, reg) = sym;
                return intern(get_register_string(reg));
            }
        }

        if (policy != REG_REPLACE_TEMP_ARG)
        {
            if (minim_falsep(MINIM_VECTOR_REF(regs, REG_RT)))
            {
                uint8_t reg = reserve_register(env, regs, table, REG_RT);
                MINIM_VECTOR_REF(regs, reg) = sym;
                return intern(get_register_string(reg));
            }
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
    if (!minim_booleanp(ref))   // false signals empty, true signals reserved by anonymous
    {
        ref =  MINIM_VECTOR_REF(regs, reg);
        minim_symbol_table_set(table, MINIM_SYMBOL(ref),
                               hash_symbol(MINIM_SYMBOL(ref)), minim_false);
    }

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

static void
call_cleanup(MinimEnv *env,
             Function *func,
             MinimObject *stx_it,
             MinimObject *line,
             MinimObject *regs,
             MinimSymbolTable *table)
{
    MinimObject *name = MINIM_STX_VAL(MINIM_CADR(line));
    if (minim_eqp(name, func->ret_sym))
    {
        minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_RT_STR));
        MINIM_VECTOR_REF(regs, REG_RT) = name;
    }
    else
    {
        MinimObject *reg = fresh_register(env, regs, table, name, REG_REPLACE_TEMP_ONLY);
        minim_symbol_table_add(table, MINIM_SYMBOL(name), reg);

        // ($mov <reg> $rt)
        MINIM_CAR(stx_it) = move_instruction(
            minim_ast(reg, NULL),
            minim_ast(intern(REG_RT_STR), NULL));
    }
}

void function_register_allocation(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *regs, *trailing, *last_use;

    debug_function(env, func);

    regs = minim_vector2(REGISTER_COUNT, minim_false);
    last_use = compute_last_use(env, func);

    trailing = NULL; 
    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op, *end_uses;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        end_uses = get_lasts(last_use, MINIM_CAR(it));

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

            MINIM_CDR(trailing) = MINIM_CDR(it);
            it = trailing;
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
            
            MinimObject *val;
           
            val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            if (MINIM_OBJ_SYMBOLP(val))
            {
                printf("unimplemented [move between registers]\n");
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
                call_cleanup(env, func, it, line, regs, table);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$top")))
            {
                // TODO: top-level symbols should be checked before compilation
                MinimObject *sym, *reg;
                
                sym = MINIM_STX_VAL(MINIM_CADR(val));
                reg = fresh_register(env, regs, table, sym, REG_REPLACE_TEMP_ONLY);
                minim_symbol_table_add(table, MINIM_SYMBOL(sym), reg);

                // ($mov $rt ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(sym, NULL),
                        minim_null)),
                        NULL));

                unreserve_register(env, regs, table, get_register_index(reg));
            }
            else
            {
                printf("unimplemented [other forms of set]\n");
            }
        }
        else if (minim_eqp(op, intern("$return")))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(line)));

            if (!ref)
            {
                printf("unimplemented [temp]\n");
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

        trailing = it;
        if (end_uses)
        {
            for (MinimObject *it = end_uses; !minim_nullp(it); it = MINIM_CDR(it))
            {
                MinimObject *ref = minim_symbol_table_get(table, MINIM_SYMBOL(MINIM_CAR(it)));
                if (ref && !intern(REG_TC_STR)) unreserve_register(env, regs, table, get_register_index(ref));
            }
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

uint8_t get_register_index(MinimObject *reg)
{
    if (minim_eqp(reg, intern(REG_RT_STR)))         return REG_RT;
    else if (minim_eqp(reg, intern(REG_R0_STR)))    return REG_R0;
    else if (minim_eqp(reg, intern(REG_R1_STR)))    return REG_R1;
    else if (minim_eqp(reg, intern(REG_R2_STR)))    return REG_R2;
    else if (minim_eqp(reg, intern(REG_T0_STR)))    return REG_T0;
    else if (minim_eqp(reg, intern(REG_T1_STR)))    return REG_T1;
    else if (minim_eqp(reg, intern(REG_T2_STR)))    return REG_T2;
    else if (minim_eqp(reg, intern(REG_T3_STR)))    return REG_T3;
    else                                            return -1;
}
