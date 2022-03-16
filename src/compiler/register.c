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

//
//  Last use analysis
//

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

//
//  Register allocation
//

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
            MINIM_VECTOR_REF(regs, i) = sym;
            return intern(get_register_string(i));
        }
    }

    if (policy != REG_REPLACE_TEMP_ONLY)
    {
        for (uint8_t i = REG_R0; i <= REG_R2; i++)
        {
            if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
            {
                MINIM_VECTOR_REF(regs, i) = sym;
                return intern(get_register_string(i));
            }
        }

        if (policy != REG_REPLACE_TEMP_ARG)
        {
            if (minim_falsep(MINIM_VECTOR_REF(regs, REG_RT)))
            {
                MINIM_VECTOR_REF(regs, REG_RT) = sym;
                return intern(REG_RT_STR);
            }
        }
    }

    printf("unimplemented [requesting fresh register when all are taken]\n");
    return intern(REG_RT_STR);
}

static MinimObject *
existing_or_fresh_register(MinimEnv *env,
                           MinimObject *regs,
                           MinimSymbolTable *table,
                           MinimObject *sym,
                           uint8_t policy)
{
    MinimObject *ref = minim_symbol_table_get(table, MINIM_SYMBOL(sym));
    return (ref ? ref : fresh_register(env, regs, table, sym, policy));
}

static void
reserve_register(MinimEnv *env,
                 MinimObject *regs,
                 MinimSymbolTable *table,
                 MinimObject **prev_instr,
                 uint8_t reg)
{
    MinimObject *old = MINIM_VECTOR_REF(regs, reg);
    if (!minim_falsep(MINIM_VECTOR_REF(regs, reg)))
    {
        MinimObject *fresh = fresh_register(env, regs, table, old, REG_REPLACE_TEMP_ONLY);

        // ($mov <fresh> <old>)
        MINIM_CDR(*prev_instr) = minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$mov"), NULL),
            minim_cons(minim_ast(fresh, NULL),
            minim_cons(minim_ast(intern(get_register_string(reg)), NULL),
            minim_null))),
            NULL),
            MINIM_CDR(*prev_instr));
        *prev_instr = MINIM_CDR(*prev_instr);

        // update
        minim_symbol_table_set(table, MINIM_SYMBOL(old), fresh);
        MINIM_VECTOR_REF(regs, get_register_index(fresh)) = old;
    }

    // set to true as a placeholder
    MINIM_VECTOR_REF(regs, reg) = minim_true;
}

static void unreserve_register(MinimObject *regs, uint8_t reg)
{
    MINIM_VECTOR_REF(regs, reg) = minim_false;
}

static MinimObject *
argument_location(MinimEnv *env,
                  MinimObject *regs,
                  MinimSymbolTable *table,
                  MinimObject *sym,
                  MinimObject **prev,
                  size_t idx)
{
    switch (idx)
    {
    case 0:
        reserve_register(env, regs, table, prev, REG_R0);
        MINIM_VECTOR_REF(regs, REG_R0) = sym;
        return intern(REG_R0_STR);
    case 1:
        reserve_register(env, regs, table, prev, REG_R1);
        MINIM_VECTOR_REF(regs, REG_R1) = sym;
        return intern(REG_R1_STR);
    case 2:
        reserve_register(env, regs, table, prev, REG_R2);
        MINIM_VECTOR_REF(regs, REG_R2) = sym;
        return intern(REG_R2_STR);
    default:
        printf("unimplemented [requesting location for argument %zu]\n", idx);
        reserve_register(env, regs, table, prev, REG_RT);
        MINIM_VECTOR_REF(regs, REG_RT) = sym;
        return intern(REG_RT_STR);
    }
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
unreserve_last_uses(MinimEnv *env,
                    MinimObject *regs,
                    MinimSymbolTable *table,
                    MinimObject *end_uses)
{
    if (end_uses)
    {
        for (MinimObject *it = end_uses; !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_SYMBOL(MINIM_CAR(it)));
            if (ref && !minim_eqp(ref, intern(REG_TC_STR)))
            {
                unreserve_register(regs, get_register_index(ref));
                minim_symbol_table_remove(table, MINIM_SYMBOL(MINIM_CAR(it)));
            }
        }
    }
}

void function_register_allocation(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *regs, *prev, *last_use;

    regs = minim_vector2(REGISTER_COUNT, minim_false);
    last_use = compute_last_use(env, func);
    init_minim_symbol_table(&table);
    prev = NULL; 

    // debug_function(env, func);
    
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
            unreserve_last_uses(env, regs, table, end_uses);
            reserve_register(env, regs, table, &prev, REG_RT);

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

            unreserve_register(regs, REG_RT);
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

            // unreserve_last_uses(env, regs, table, end_uses);

            MINIM_CDR(prev) = MINIM_CDR(it);
            it = prev;
        }
        else if (minim_eqp(op, intern("$set")))
        {
            // ($set <t> ($eval ...)) =>
            //
            // ($set <t> ($top <v>)) =>
            //  t = get_top(v)
            // ($set <t> ($load <v>)) =>
            //  t = env_get_sym(v)
            // ($set <t> ($quote <v>)) =>
            //  t = quote(v)
            // ($set <t> <v>) =>
            //  t = v
            
            MinimObject *name, *val;

            name = MINIM_STX_VAL(MINIM_CADR(line));
            val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            if (MINIM_OBJ_SYMBOLP(val))
            {
                MinimObject *reg, *src;
                
                reg = existing_or_fresh_register(env, regs, table, name, REG_REPLACE_TEMP_ONLY);
                src = minim_symbol_table_get(table, MINIM_SYMBOL(name));

                // ($mov <regd> <regt>)
                if (!minim_eqp(reg, src))
                {
                    MINIM_CAR(it) = move_instruction(
                        minim_ast(reg, NULL),
                        minim_ast(src, NULL));
                }

                printf("warn [move between registers]\n");
                unreserve_last_uses(env, regs, table, end_uses);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$eval")))
            {
                MinimObject *op;
                size_t idx = 0;

                for (MinimObject *it2 = MINIM_CDDR(val); !minim_nullp(it2); it2 = MINIM_CDR(it2))
                {
                    MinimObject *name, *loc, *src;
                    
                    name = MINIM_STX_VAL(MINIM_CAR(it2));
                    src = minim_symbol_table_get(table, MINIM_SYMBOL(name));
                    loc = argument_location(env, regs, table, name, &prev, idx);

                    // ($mov <reg> <arg>)
                    if (!minim_eqp(loc, src))
                    {
                        MINIM_CDR(prev) = minim_cons(
                            move_instruction(minim_ast(loc, NULL), minim_ast(src, NULL)),
                            MINIM_CDR(prev));
                        prev = MINIM_CDR(prev);
                    }

                    MINIM_VECTOR_REF(regs, get_register_index(src)) = minim_false;
                    minim_symbol_table_set(table, MINIM_SYMBOL(name), loc);
                    unreserve_register(regs, get_register_index(src));
                    ++idx;
                }

                op = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(val)));
                reserve_register(env, regs, table, &prev, REG_RT);

                //  ($call <reg>)
                MINIM_CAR(it) = call_instruction(minim_ast(op, NULL));
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                MINIM_VECTOR_REF(regs, REG_RT) = name;
                unreserve_last_uses(env, regs, table, end_uses);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$quote")))
            {
                MinimObject *stx;

                reserve_register(env, regs, table, &prev, REG_RT);
                reserve_register(env, regs, table, &prev, REG_R0);
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
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                MINIM_VECTOR_REF(regs, REG_RT) = name;
                unreserve_last_uses(env, regs, table, end_uses);
                unreserve_register(regs, REG_R0);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$top")))
            {
                // TODO: top-level symbols should be checked before compilation
                MinimObject *reg = existing_or_fresh_register(env, regs, table, name, REG_REPLACE_TEMP_ONLY);
                minim_symbol_table_add(table, MINIM_SYMBOL(name), reg);

                // ($mov $rt ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(MINIM_STX_VAL(MINIM_CADR(val)), NULL),
                        minim_null)),
                        NULL));

                unreserve_last_uses(env, regs, table, end_uses);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$load")))
            {
                // TODO: top-level symbols should be checked before compilation
                MinimObject *reg = existing_or_fresh_register(env, regs, table, name, REG_REPLACE_TEMP_ONLY);
                minim_symbol_table_add(table, MINIM_SYMBOL(name), reg);

                // ($mov $rt ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(intern("get_sym"), NULL),
                        minim_null)),
                        NULL));

                unreserve_register(regs, get_register_index(reg));
                unreserve_last_uses(env, regs, table, end_uses);
            }
            else
            {
                printf("unimplemented [other forms of set]\n");
                unreserve_last_uses(env, regs, table, end_uses);
            }
        }
        else if (minim_eqp(op, intern("$goto")))
        {
            MinimObject *label = MINIM_STX_VAL(MINIM_CADR(line));

            // ($jmp <label>)
            MINIM_CAR(it) = minim_ast(
                minim_cons(minim_ast(intern("$jmp"), NULL),
                minim_cons(minim_ast(label, NULL),
                minim_null)),
                NULL);

            unreserve_last_uses(env, regs, table, end_uses);
        }
        else if (minim_eqp(op, intern("$gofalse")))
        {
            MinimObject *label, *arg;
            
            label = MINIM_STX_VAL(MINIM_CADR(line));
            arg = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));

            // ($jmpz <label> <arg>)
            MINIM_CAR(it) = minim_ast(
                minim_cons(minim_ast(intern("$jmpz"), NULL),
                minim_cons(minim_ast(label, NULL),
                minim_cons(minim_ast(arg, NULL),
                minim_null))),
                NULL);

            unreserve_last_uses(env, regs, table, end_uses);
        }
        else if (minim_eqp(op, intern("$bind")))
        {
            printf("unimplemented [bind]\n");
        }
        else if (minim_eqp(op, intern("$return")))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(line)));
            if (!ref)
            {
                printf("unimplemented [return value not found]\n");
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
            unreserve_last_uses(env, regs, table, end_uses);
        }

        prev = it;
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
