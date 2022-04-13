#include "../core/minimpriv.h"
#include "compilepriv.h"

// register replacement policy
#define REG_REPLACE_TEMP_ONLY   0
#define REG_REPLACE_TEMP_ARG    1
#define REG_REPLACE_EXCEPT_TC   2

#define INIT_SYMBOL_TABLE_IF_NULL(tab) \
    if ((tab) == NULL) { init_minim_symbol_table(&tab); }

#define INSERT_INSTR(prev, instr)                               \
    {                                                           \
        MINIM_CDR(prev) = minim_cons(instr, MINIM_CDR(prev));   \
        prev = MINIM_CDR(prev);                                 \
    }

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

    return minim_null;
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
//  Memory allocation
//

#define ARG_MEM(mem)    MINIM_VECTOR_REF(mem, 0)
#define TEMP_MEM(mem)   MINIM_VECTOR_REF(mem, 1)

static void
allocate_arg_memory(MinimObject *memory, size_t size)
{
    MinimObject *tail;
    size_t len;

    len = minim_list_length(ARG_MEM(memory));
    if (len == 0)
    {
        ARG_MEM(memory) = minim_cons(minim_false, minim_null);
        tail = ARG_MEM(memory);
        len = 1;
    }
    else
    {
        MINIM_TAIL(tail, ARG_MEM(memory));
    }
    

    for (size_t i = len; i < size; ++i)
    {
        MINIM_CDR(tail) = minim_cons(minim_false, minim_null);
        tail = MINIM_CDR(tail);
    }
}

static void
set_argument_memory(MinimObject *memory, size_t idx, MinimObject *sym)
{
    MinimObject *it = ARG_MEM(memory);
    for (size_t i = 0; i < idx; ++i)
        MINIM_CAR(it) = sym;
}

static MinimObject *
add_fresh_temp_memory(MinimObject *memory, MinimObject *sym)
{
    size_t len = minim_list_length(TEMP_MEM(memory));
    if (len == 0)
    {
        TEMP_MEM(memory) = minim_cons(sym, minim_null);
    }
    else
    {
        MinimObject *it;  
        MINIM_TAIL(it, TEMP_MEM(memory));
        MINIM_CDR(it) = minim_cons(sym, minim_null);
    }
    
    return minim_cons(minim_ast(intern("$mem"), NULL),
            minim_cons(minim_ast(uint_to_minim_number(len), NULL),
            minim_null));
}

static MinimObject *
add_temp_memory(MinimObject *memory, MinimObject *sym)
{
    size_t len = minim_list_length(TEMP_MEM(memory));
    if (len == 0)
    {
        TEMP_MEM(memory) = minim_cons(sym, minim_null);
    }
    else
    {
        MinimObject *it = TEMP_MEM(memory);
        size_t idx = 0;

        while (!minim_nullp(MINIM_CDR(it)))
        {
            if (minim_falsep(MINIM_CAR(it)))
            {
                MINIM_CAR(it) = sym;
                return minim_cons(minim_ast(intern("$mem"), NULL),
                        minim_cons(minim_ast(uint_to_minim_number(idx), NULL),
                        minim_null));
            }
            
            it = MINIM_CDR(it);
            ++idx;
        }

        MINIM_CDR(it) = minim_cons(sym, minim_null);
    }

    return minim_cons(minim_ast(intern("$mem"), NULL),
            minim_cons(minim_ast(uint_to_minim_number(len), NULL),
            minim_null));
}

static void
unreserve_temp_memory(MinimObject *memory, size_t idx)
{
    MinimObject *it = TEMP_MEM(memory);
    for (size_t i = 0; i < idx; ++i)
        it = MINIM_CDR(it);

    MINIM_CAR(it) = minim_false;
}

//
//  Register allocation
//

static MinimObject *
fresh_register(MinimEnv *env,
               MinimObject *regs,
               MinimObject *memory,
               MinimSymbolTable *table,
               MinimObject *sym,
               size_t policy)
{
    for (size_t i = REG_T0; i <= REG_T3; i++)
    {
        if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
        {
            MINIM_VECTOR_REF(regs, i) = sym;
            return get_register_symbol(i);
        }
    }

    if (policy != REG_REPLACE_TEMP_ONLY)
    {
        for (size_t i = REG_R0; i <= REG_R2; i++)
        {
            if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
            {
                MINIM_VECTOR_REF(regs, i) = sym;
                return get_register_symbol(i);
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

    return add_temp_memory(memory, sym);
}

static MinimObject *
existing_or_fresh_register(MinimEnv *env,
                           MinimObject *regs,
                           MinimObject *memory,
                           MinimSymbolTable *table,
                           MinimObject *sym,
                           size_t policy)
{
    MinimObject *ref = minim_symbol_table_get(table, MINIM_SYMBOL(sym));
    if (ref && MINIM_OBJ_SYMBOLP(ref))
        return ref;
    else
        return fresh_register(env, regs, memory, table, sym, policy);
}

static void
reserve_register(MinimEnv *env,
                 MinimObject *regs,
                 MinimObject *memory,
                 MinimSymbolTable *table,
                 MinimObject **prev_instr,
                 size_t reg)
{
    MinimObject *old = MINIM_VECTOR_REF(regs, reg);
    if (!minim_falsep(MINIM_VECTOR_REF(regs, reg)))
    {
        MinimObject *fresh = fresh_register(env, regs, memory, table, old, REG_REPLACE_TEMP_ONLY);

        // ($mov <fresh> <old>)
        MINIM_CDR(*prev_instr) = minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$mov"), NULL),
            minim_cons(minim_ast(fresh, NULL),
            minim_cons(minim_ast(get_register_symbol(reg), NULL),
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

static void unreserve_register(MinimObject *regs, size_t reg)
{
    MINIM_VECTOR_REF(regs, reg) = minim_false;
}

static MinimObject *
temp_register(MinimEnv *env,
              MinimObject *regs,
              MinimObject *memory,
              MinimSymbolTable *table,
              MinimObject **prev_instr,
              MinimObject *sym)
{
    for (size_t i = REG_T0; i <= REG_T3; i++)
    {
        if (minim_falsep(MINIM_VECTOR_REF(regs, i)))
        {
            MINIM_VECTOR_REF(regs, i) = sym;
            return get_register_symbol(i);
        }
    }

    reserve_register(env, regs, memory, table, prev_instr, REG_T0);
    return get_register_symbol(REG_T0);
}

static bool
in_argument_location(MinimObject *loc, size_t idx)
{
    switch (idx)
    {
    case 0:
        return minim_eqp(loc, intern(REG_R0_STR));
    case 1:
        return minim_eqp(loc, intern(REG_R1_STR));
    case 2:
        return minim_eqp(loc, intern(REG_R2_STR));
    default:
        return false;
    }
}

static MinimObject *
argument_location(MinimEnv *env,
                  MinimObject *regs,
                  MinimObject *memory,
                  MinimSymbolTable *table,
                  MinimObject *sym,
                  MinimObject **prev,
                  size_t idx)
{
    switch (idx)
    {
    case 0:
        reserve_register(env, regs, memory, table, prev, REG_R0);
        MINIM_VECTOR_REF(regs, REG_R0) = sym;
        return intern(REG_R0_STR);
    case 1:
        reserve_register(env, regs, memory, table, prev, REG_R1);
        MINIM_VECTOR_REF(regs, REG_R1) = sym;
        return intern(REG_R1_STR);
    case 2:
        reserve_register(env, regs, memory, table, prev, REG_R2);
        MINIM_VECTOR_REF(regs, REG_R2) = sym;
        return intern(REG_R2_STR);
    default: 
        return add_fresh_temp_memory(memory, sym);
    }
}

static void
unregister_location(MinimObject *regs,
                    MinimObject *memory,
                    MinimSymbolTable *table,
                    MinimObject *name)
{
    MinimObject *ref;
    
    ref = minim_symbol_table_get(table, MINIM_SYMBOL(name));
    if (ref)
    {
        if (MINIM_OBJ_SYMBOLP(ref))
        {
            if (!minim_eqp(ref, intern(REG_TC_STR)))
            {
                unreserve_register(regs, get_register_index(ref));
                minim_symbol_table_remove(table, MINIM_SYMBOL(name));
            }
        }
        else
        {
            minim_symbol_table_remove(table, MINIM_SYMBOL(name));
            if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(ref)), intern("$mem")))
            {
                size_t idx = MINIM_NUMBER_TO_UINT(MINIM_STX_VAL(MINIM_CADR(ref)));
                unreserve_temp_memory(memory, idx);
            }
        }
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
                    MinimObject *memory,
                    MinimSymbolTable *table,
                    MinimObject *end_uses)
{
    for (MinimObject *it = end_uses; !minim_nullp(it); it = MINIM_CDR(it))
        unregister_location(regs, memory, table, MINIM_CAR(it));
}

static void
eliminate_unwind(MinimEnv *env, Function *func)
{
    MinimObject *reverse, *trailing;

    reverse = minim_list_reverse(func->pseudo);
    trailing = reverse;
    for (MinimObject *it = MINIM_CDR(reverse); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$mov")))
        {
            if (minim_eqp(MINIM_STX_VAL(MINIM_CADR(line)), intern("$tc")))
            {
                MINIM_CDR(trailing) = MINIM_CDR(it);
                it = trailing;
            }
        }
        else
        {
            break;
        }

        trailing = it;
    }

    func->pseudo = minim_list_reverse(reverse);
}

void function_register_allocation(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *regs, *memory, *prev, *last_use;

    regs = minim_vector2(REGISTER_COUNT, minim_false);
    memory = minim_vector2(2, minim_null);
    last_use = compute_last_use(env, func);
    init_minim_symbol_table(&table);
    prev = NULL; 
    
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op, *end_uses;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        end_uses = get_lasts(last_use, MINIM_CAR(it));

        // debug_print_minim_object(MINIM_CAR(it), env); printf("\n");

        if (minim_eqp(op, intern("$push-env")))
        {
            // =>
            //  $tc = init_env($tc)
            unreserve_last_uses(env, regs, memory, table, end_uses);
            reserve_register(env, regs, memory, table, &prev, REG_RT);

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
        else if (minim_eqp(op, intern("$pop-env")))
        {
            //  ($access $rt <type> <field>)
            MINIM_CAR(it) = minim_ast(
                minim_cons(minim_ast(intern("$access"), NULL),
                minim_cons(minim_ast(intern(REG_RT_STR), NULL),
                minim_cons(minim_ast(intern("env"), NULL),
                minim_cons(minim_ast(intern("prev"), NULL),
                minim_null)))),
                NULL);
        }
        else if (minim_eqp(op, intern("$arg")))
        {
            MinimObject *name, *offset;
            size_t mem_offset, idx;

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
                mem_offset = idx - 3;
                allocate_arg_memory(memory, mem_offset + 1);
                set_argument_memory(memory, mem_offset, name);
                minim_symbol_table_add(table, MINIM_SYMBOL(name),
                    minim_cons(intern("$arg-mem"),
                    minim_cons(uint_to_minim_number(mem_offset),
                    minim_null)));
            }

            unreserve_last_uses(env, regs, memory, table, end_uses);
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
            // ($set <t> ($interpret <v>))
            //  t = interpret(v)
            // ($set <t> <v>) =>
            //  t = v
            
            MinimObject *name, *val;

            name = MINIM_STX_VAL(MINIM_CADR(line));
            val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            if (MINIM_OBJ_SYMBOLP(val))
            {
                MinimObject *reg, *src;
                
                reg = existing_or_fresh_register(env, regs, memory, table, name, REG_REPLACE_TEMP_ONLY);
                src = minim_symbol_table_get(table, MINIM_SYMBOL(val));

                // ($mov <regd> <regt>)
                if (!minim_eqp(reg, src))
                {
                    MINIM_CAR(it) = move_instruction(
                        minim_ast(reg, NULL),
                        minim_ast(src, NULL));
                }

                // printf("warn [move between registers]\n");
                minim_symbol_table_remove(table, MINIM_SYMBOL(name));
                minim_symbol_table_add(table, MINIM_SYMBOL(name), reg);
                unreserve_last_uses(env, regs, memory, table, end_uses);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$eval")))
            {
                MinimObject *op, *it2, *tc_tmp, *tc_sym;
                size_t argc, idx;
                
                argc = minim_list_length(MINIM_CDDR(val));
                it2 = MINIM_CDDR(val);
                idx = 0;

                for (; !minim_nullp(it2) && idx < ARG_REGISTER_COUNT; it2 = MINIM_CDR(it2))
                {
                    MinimObject *name, *loc, *src;
                    
                    name = MINIM_STX_VAL(MINIM_CAR(it2));
                    src = minim_symbol_table_get(table, MINIM_SYMBOL(name));
                    if (in_argument_location(src, idx))
                    {
                        // ($mov <tmp> <arg>)
                        loc = fresh_register(env, regs, memory, table, name, REG_REPLACE_TEMP_ONLY);
                        INSERT_INSTR(prev, move_instruction(minim_ast(loc, NULL),
                                                            minim_ast(src, NULL)));
                        minim_symbol_table_set(table, MINIM_SYMBOL(name), loc);
                    }
                    else
                    {
                        // ($mov <reg> <arg>)
                        loc = argument_location(env, regs, memory, table, name, &prev, idx);
                        INSERT_INSTR(prev, move_instruction(minim_ast(loc, NULL),
                                                            minim_ast(src, NULL)));
                    }

                    MINIM_VECTOR_REF(regs, get_register_index(src)) = minim_false;
                    unreserve_register(regs, get_register_index(src));
                    ++idx;
                }
                
                if (!minim_nullp(it2))
                {
                    for (it2 = minim_list_reverse(it2); idx < argc; it2 = MINIM_CDR(it2))
                    {
                        MinimObject *name, *loc, *src;
                    
                        name = MINIM_STX_VAL(MINIM_CAR(it2));
                        loc = argument_location(env, regs, memory, table, name, &prev, idx);
                        src = minim_symbol_table_get(table, MINIM_SYMBOL(name));
                        
                        // ($mov <reg> <arg>)
                        if (!minim_eqp(loc, src))
                        {
                            INSERT_INSTR(prev, move_instruction(minim_ast(loc, NULL),
                                                                minim_ast(src, NULL)));
                        }

                        unreserve_register(regs, get_register_index(src));  
                        ++idx;
                    }
                }

                op = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(val)));
                if (!MINIM_OBJ_SYMBOLP(op))
                {
                    op = temp_register(env, regs, memory, table, &prev, name);
                    MINIM_VECTOR_REF(regs, get_register_index(op)) = name;
                }
                
                reserve_register(env, regs, memory, table, &prev, REG_RT);
                tc_sym = intern("$tc");
                tc_tmp = fresh_register(env, regs, memory, table, tc_sym, REG_REPLACE_EXCEPT_TC);
                INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                //  ($call <reg>)
                INSERT_INSTR(prev, call_instruction(minim_ast(op, NULL)));
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                MINIM_VECTOR_REF(regs, REG_RT) = name;

                // cleanup
                MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                unreserve_last_uses(env, regs, memory, table, end_uses);
                unregister_location(regs, memory, table, tc_sym);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$quote")))
            {
                MinimObject *stx, *tc_sym, *tc_tmp;

                reserve_register(env, regs, memory, table, &prev, REG_RT);
                reserve_register(env, regs, memory, table, &prev, REG_R0);
                stx = MINIM_CADR(val);

                // ($mov $rt ($addr <quote>))
                INSERT_INSTR(prev, move_instruction(
                    minim_ast(intern(REG_RT_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(intern("quote"), NULL),
                        minim_null)),
                        NULL)));

                // ($mov $r0 ($syntax <quote-syntax>))
                INSERT_INSTR(prev, move_instruction(
                    minim_ast(intern(REG_R0_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$syntax"), NULL),
                        minim_cons(stx, minim_null)),
                        NULL)));

                tc_sym = intern("$tc");
                tc_tmp = fresh_register(env, regs, memory, table, tc_sym, REG_REPLACE_EXCEPT_TC);
                INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                //  ($call $rt)
                INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                MINIM_VECTOR_REF(regs, REG_RT) = name;

                // cleanup
                MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                unreserve_last_uses(env, regs, memory, table, end_uses);
                unregister_location(regs, memory, table, tc_sym);
                unreserve_register(regs, REG_R0);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$interpret")))
            {
                MinimObject *stx, *tc_sym, *tc_tmp;

                reserve_register(env, regs, memory, table, &prev, REG_RT);
                reserve_register(env, regs, memory, table, &prev, REG_R0);
                stx = MINIM_CADR(val);

                // ($mov $rt ($addr <quote>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(intern(REG_RT_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(intern("interpret"), NULL),
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

                tc_sym = intern("$tc");
                tc_tmp = fresh_register(env, regs, memory, table, tc_sym, REG_REPLACE_EXCEPT_TC);
                INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                //  ($call $rt)
                INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));
                minim_symbol_table_add(table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                MINIM_VECTOR_REF(regs, REG_RT) = name;

                // cleanup
                MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                unreserve_last_uses(env, regs, memory, table, end_uses);
                unregister_location(regs, memory, table, tc_sym);
                unreserve_register(regs, REG_R0);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$top")))
            {
                // TODO: top-level symbols should be checked before compilation
                MinimObject *reg_or_mem = existing_or_fresh_register(env, regs, memory,
                                                                     table, name, REG_REPLACE_TEMP_ONLY);

                // ($mov <reg/mem> ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg_or_mem, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(MINIM_STX_VAL(MINIM_CADR(val)), NULL),
                        minim_null)),
                        NULL));

                minim_symbol_table_add(table, MINIM_SYMBOL(name), reg_or_mem);
                unreserve_last_uses(env, regs, memory, table, end_uses);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$load")))
            {
                MinimObject *reg = existing_or_fresh_register(env, regs, memory, table, name, REG_REPLACE_TEMP_ONLY);
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
                unreserve_last_uses(env, regs, memory, table, end_uses);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$func")))
            {
                MinimObject *reg, *val;
                
                val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
                reg = existing_or_fresh_register(env, regs, memory, table, name, REG_REPLACE_TEMP_ONLY);
                minim_symbol_table_add(table, MINIM_SYMBOL(name), reg);

                // ($mov $rt ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$func-addr"), NULL),
                        minim_cons(minim_ast(val, NULL),
                        minim_null)),
                        NULL));

                unreserve_register(regs, get_register_index(reg));
                unreserve_last_uses(env, regs, memory, table, end_uses);
            }
            else
            {
                printf("error [other forms of set]\n");
                unreserve_last_uses(env, regs, memory, table, end_uses);
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

            unreserve_last_uses(env, regs, memory, table, end_uses);
        }
        else if (minim_eqp(op, intern("$gofalse")))
        {
            MinimObject *label, *arg, *loc;
            
            label = MINIM_STX_VAL(MINIM_CADR(line));
            arg = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            loc = minim_symbol_table_get(table, MINIM_SYMBOL(arg));

            // ($jmpz <label> <arg>)
            MINIM_CAR(it) = minim_ast(
                minim_cons(minim_ast(intern("$jmpz"), NULL),
                minim_cons(minim_ast(label, NULL),
                minim_cons(minim_ast(loc, NULL),
                minim_null))),
                NULL);

            unreserve_last_uses(env, regs, memory, table, end_uses);
        }
        else if (minim_eqp(op, intern("$bind")))
        {
            // => ($bind sym val)
            //  intern(sym, val)
            //

            MinimObject *name, *val, *loc, *tc_sym, *tc_tmp;

            reserve_register(env, regs, memory, table, &prev, REG_R1);
            val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            loc = minim_symbol_table_get(table, MINIM_SYMBOL(val));

            // ($mov $r1 <loc>)
            MINIM_CAR(it) = move_instruction(
                minim_ast(intern(REG_R1_STR), NULL),
                minim_ast(loc, NULL));

            insert_empty_into_list(it);
            it = MINIM_CDR(it);
            
            reserve_register(env, regs, memory, table, &prev, REG_RT);
            reserve_register(env, regs, memory, table, &prev, REG_R0);
            name = MINIM_STX_VAL(MINIM_CADR(line));

            // ($mov $r0 ($sym-addr <sym>>
            MINIM_CAR(it) = move_instruction(
                minim_ast(intern(REG_R0_STR), NULL),
                minim_ast(
                    minim_cons(minim_ast(intern("$sym-addr"), NULL),
                    minim_cons(minim_ast(name, NULL),
                    minim_null)),
                    NULL));

            insert_empty_into_list(it);
            it = MINIM_CDR(it);
            
            // ($mov $rt ($addr set_sym))
            MINIM_CAR(it) = move_instruction(
                minim_ast(intern(REG_RT_STR), NULL),
                minim_ast(
                    minim_cons(minim_ast(intern("$addr"), NULL),
                    minim_cons(minim_ast(intern("set_sym"), NULL),
                    minim_null)),
                    NULL));

            insert_empty_into_list(it);
            it = MINIM_CDR(it);

            tc_sym = intern("$tc");
            tc_tmp = fresh_register(env, regs, memory, table, tc_sym, REG_REPLACE_EXCEPT_TC);
            INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

            //  ($call $rt)
            INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));

            // cleanup
            MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
            unreserve_last_uses(env, regs, memory, table, end_uses);
            unregister_location(regs, memory, table, tc_sym);
            unreserve_register(regs, REG_RT);
            unreserve_register(regs, REG_R0);
            unreserve_register(regs, REG_R1);
        }
        else if (minim_eqp(op, intern("$return")))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(line)));
            if (!ref)
            {
                printf("error [return value not found]\n");
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

            unreserve_last_uses(env, regs, memory, table, end_uses);
        }

        prev = it;
    }

    eliminate_unwind(env, func);
}

MinimObject *get_register_symbol(size_t reg)
{
    switch (reg)
    {
    case REG_RT:    return intern(REG_RT_STR);
    case REG_R0:    return intern(REG_R0_STR);
    case REG_R1:    return intern(REG_R1_STR);
    case REG_R2:    return intern(REG_R2_STR);
    case REG_T0:    return intern(REG_T0_STR);
    case REG_T1:    return intern(REG_T1_STR);
    case REG_T2:    return intern(REG_T2_STR);
    case REG_T3:    return intern(REG_T3_STR);
    default:        return NULL;
    }
}

size_t get_register_index(MinimObject *reg)
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
