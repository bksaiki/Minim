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

#define UNRESERVE_REGISTER(regs, reg)   \
    { MINIM_VECTOR_REF(regs, reg) = minim_false; }

//
//  Join canonicalization
//

static void compute_joins(MinimEnv *env, RegAllocData *data)
{
    for (MinimObject *it = data->func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;

        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$join")))
        {
            MinimObject *canon, *left, *right, *joins_canon, *joins_assign;

            canon = MINIM_STX_VAL(MINIM_CADR(line));
            left = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            right = MINIM_STX_VAL(MINIM_CADR(MINIM_CDDR(line)));
            
            joins_canon = MINIM_VECTOR_REF(data->joins, 0);
            joins_assign = MINIM_VECTOR_REF(data->joins, 1);
            MINIM_VECTOR_REF(data->joins, 0) = minim_cons(minim_cons(left, canon),
                minim_cons(minim_cons(right, canon),
                joins_canon));
            MINIM_VECTOR_REF(data->joins, 1) = minim_cons(minim_cons(canon, minim_false),
                joins_assign);
        }
    }
}

static MinimObject *join_canonicalize(MinimObject *joins, MinimObject *sym)
{
    for (MinimObject *it = MINIM_VECTOR_REF(joins, 0); !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (minim_eqp(MINIM_CAAR(it), sym))
            return MINIM_CDAR(it);
    }

    return NULL;
}

static MinimObject *
join_register(MinimObject *joins,
              MinimSymbolTable *table,
              MinimObject *sym)
{
    for (MinimObject *it = MINIM_VECTOR_REF(joins, 1); !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (minim_eqp(MINIM_CAAR(it), sym))
            return minim_symbol_table_get(table, MINIM_SYMBOL(MINIM_CDAR(it)));
    }

    return NULL;
}

//
//  Last use analysis
//

static MinimObject *get_lasts(MinimObject *last_use, MinimObject *stx)
{
    for (MinimObject *it = last_use; !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (MINIM_CAAR(it) == stx)
            return MINIM_CDAR(it);
    }

    return minim_null;
}

static void
update_last_use(MinimObject *table,
                MinimObject *sym,
                MinimObject *line)
{
    MinimObject *dict = MINIM_VECTOR_REF(table, 0);
    for (MinimObject *it = dict; !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (minim_eqp(MINIM_CAAR(it), sym))
        {
            MINIM_CDAR(it) = line;
            return;
        }
    }

    MINIM_VECTOR_REF(table, 0) = minim_cons(
        minim_cons(sym, line),
        dict);
}

// Computes the last-use locations of variables.
static MinimObject* compute_last_use(MinimEnv *env, RegAllocData *data)
{
    MinimObject *table, *last_use;

    table = minim_vector2(1, minim_null);
    for (MinimObject *it = data->func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
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
                    update_last_use(table, MINIM_STX_VAL(MINIM_CAR(it2)), MINIM_CAR(it));
            }
            
            if (join_canonicalize(data->joins, MINIM_STX_VAL(MINIM_CADR(line))))
                update_last_use(table, MINIM_STX_VAL(MINIM_CADR(line)), MINIM_CAR(it));
        }
        else if (minim_eqp(op, intern("$bind")) || minim_eqp(op, intern("$gofalse")))
        {
            update_last_use(table, MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line))), MINIM_CAR(it));
        }
        else if (minim_eqp(op, intern("$join")) || minim_eqp(op, intern("$return")))
        {
            update_last_use(table, MINIM_STX_VAL(MINIM_CADR(line)), MINIM_CAR(it));
        }
    }

    last_use = minim_vector2(1, minim_null);
    for (MinimObject *it = MINIM_VECTOR_REF(table, 0); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *dict = MINIM_VECTOR_REF(last_use, 0);
        bool inserted = false;
        for (MinimObject *it2 = dict; !minim_nullp(it2); it2 = MINIM_CDR(it2))
        {
            if (MINIM_CAAR(it2) == MINIM_CDAR(it))
            {
                MINIM_CDAR(it2) = minim_cons(MINIM_CAAR(it), MINIM_CDAR(it2));
                inserted = true;
                break;
            }
        }

        if (!inserted)
        {
            MINIM_VECTOR_REF(last_use, 0) = minim_cons(
                minim_cons(MINIM_CDAR(it), minim_cons(MINIM_CAAR(it), minim_null)),
                dict);
        }
    }

    return MINIM_VECTOR_REF(last_use, 0);
}

//
//  Tail evaluation symbols
//

void compute_tail_symbols(MinimEnv *env, RegAllocData *data)
{
    MinimObject *reverse;
    
    reverse = minim_list_reverse(data->func->pseudo);
    for (MinimObject *it = reverse; !minim_nullp(it) && !minim_nullp(MINIM_CDR(it)); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;

        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$return")))
        {
            MinimObject *next_line, *next_op, *ret_sym;

            ret_sym = MINIM_STX_VAL(MINIM_CADR(line));
            next_line = MINIM_STX_VAL(MINIM_CADR(it));
            next_op = MINIM_STX_VAL(MINIM_CAR(next_line));
            // while (minim_eqp(next_op, intern("$bind"))) {
            //     it = MINIM_CDR(it);
            //     next_line = MINIM_STX_VAL(MINIM_CADR(it));
            //     next_op = MINIM_STX_VAL(MINIM_CAR(next_line));
            // }

            if (minim_eqp(next_op, intern("$set")))
            {
                MinimObject *dest_sym = MINIM_STX_VAL(MINIM_CADR(next_line));
                if (minim_eqp(ret_sym, dest_sym))
                    minim_symbol_table_add(data->tail_syms, MINIM_SYMBOL(dest_sym), minim_true);
            }

            // skip ahead two
            it = MINIM_CDR(it);
        }
    }
}

//
//  Instructions
//

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
set_temp_memory(MinimObject *memory, size_t idx, MinimObject *sym)
{
    MinimObject *it = TEMP_MEM(memory);
    size_t count = 0;

    while (!minim_nullp(MINIM_CDR(it)))
    {
        if (idx == count)
        {
            MINIM_CAR(it) = sym;
            return;
        }
        
        it = MINIM_CDR(it);
        ++count;
    }
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

static MinimObject *fresh_register(RegAllocData *data, MinimObject *sym, size_t policy)
{
    for (size_t i = REG_T0; i <= REG_T3; i++)
    {
        if (minim_falsep(MINIM_VECTOR_REF(data->regs, i)))
        {
            MINIM_VECTOR_REF(data->regs, i) = sym;
            return get_register_symbol(i);
        }
    }

    if (policy != REG_REPLACE_TEMP_ONLY)
    {
        for (size_t i = REG_R0; i <= REG_R2; i++)
        {
            if (minim_falsep(MINIM_VECTOR_REF(data->regs, i)))
            {
                MINIM_VECTOR_REF(data->regs, i) = sym;
                return get_register_symbol(i);
            }
        }

        if (policy != REG_REPLACE_TEMP_ARG)
        {
            if (minim_falsep(MINIM_VECTOR_REF(data->regs, REG_RT)))
            {
                MINIM_VECTOR_REF(data->regs, REG_RT) = sym;
                return intern(REG_RT_STR);
            }
        }
    }

    return add_temp_memory(data->memory, sym);
}

static MinimObject *
existing_or_fresh_register(RegAllocData *data,
                           MinimObject *sym,
                           size_t policy)
{
    MinimObject *ref = minim_symbol_table_get(data->table, MINIM_SYMBOL(sym));
    return (ref && MINIM_OBJ_SYMBOLP(ref)) ? ref :  fresh_register(data, sym, policy);
}

static void reserve_register(RegAllocData *data, size_t reg)
{
    MinimObject *old = MINIM_VECTOR_REF(data->regs, reg);
    if (!minim_falsep(MINIM_VECTOR_REF(data->regs, reg)))
    {
        MinimObject *fresh = fresh_register(data, old, REG_REPLACE_TEMP_ONLY);

        // ($mov <fresh> <old>)
        INSERT_INSTR(*data->prev, minim_ast(
            minim_cons(minim_ast(intern("$mov"), NULL),
            minim_cons(minim_ast(fresh, NULL),
            minim_cons(minim_ast(get_register_symbol(reg), NULL),
            minim_null))),
            NULL));

        // update
        minim_symbol_table_set(data->table, MINIM_SYMBOL(old), fresh);
        MINIM_VECTOR_REF(data->regs, get_register_index(fresh)) = old;
    }

    // set to true as a placeholder
    MINIM_VECTOR_REF(data->regs, reg) = minim_true;
}

static MinimObject *temp_register(RegAllocData *data, MinimObject *sym)
{
    for (size_t i = REG_T0; i <= REG_T3; i++)
    {
        if (minim_falsep(MINIM_VECTOR_REF(data->regs, i)))
        {
            MINIM_VECTOR_REF(data->regs, i) = sym;
            return get_register_symbol(i);
        }
    }

    reserve_register(data, REG_T0);
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

static MinimObject *argument_location(RegAllocData *data, MinimObject *sym, size_t idx)
{
    switch (idx)
    {
    case 0:
        reserve_register(data, REG_R0);
        MINIM_VECTOR_REF(data->regs, REG_R0) = sym;
        return intern(REG_R0_STR);
    case 1:
        reserve_register(data, REG_R1);
        MINIM_VECTOR_REF(data->regs, REG_R1) = sym;
        return intern(REG_R1_STR);
    case 2:
        reserve_register(data, REG_R2);
        MINIM_VECTOR_REF(data->regs, REG_R2) = sym;
        return intern(REG_R2_STR);
    default: 
        return add_fresh_temp_memory(data->memory, sym);
    }
}

static void prepare_call(RegAllocData *data, bool tail_pos)
{
    MinimObject *ref, *loc;
    size_t idx;

    for (idx = REG_R0; idx < ARG_REGISTER_COUNT; ++idx)
    {
        ref = MINIM_VECTOR_REF(data->regs, idx);
        if (!minim_falsep(ref))
        {
            loc = fresh_register(data, ref, REG_REPLACE_TEMP_ONLY);
            INSERT_INSTR(*data->prev, move_instruction(
                minim_ast(loc, NULL),
                minim_ast(get_register_symbol(idx), NULL)));

            if (!tail_pos)
            {
                minim_symbol_table_set(data->table, MINIM_SYMBOL(ref), loc);
                UNRESERVE_REGISTER(data->regs, idx);
            }
        }
    }

    reserve_register(data, REG_RT);
}

static void unreserve_location(RegAllocData *data, MinimObject *loc)
{
    if (MINIM_OBJ_SYMBOLP(loc))
    {
        UNRESERVE_REGISTER(data->regs, get_register_index(loc));
    }
    else
    {
        if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(loc)), intern("$mem")))
        {
            size_t idx = MINIM_NUMBER_TO_UINT(MINIM_STX_VAL(MINIM_CADR(loc)));
            unreserve_temp_memory(data->memory, idx);
        }
    }
}

static void register_location(RegAllocData *data, MinimObject *name, MinimObject *loc)
{
    if (MINIM_OBJ_PAIRP(loc))
    {
        size_t idx = MINIM_NUMBER_TO_UINT(MINIM_STX_VAL(MINIM_CADR(loc)));
        set_temp_memory(data->memory, idx, name);
    }
    else
    {
        size_t idx = get_register_index(loc);
        reserve_register(data, idx);
        MINIM_VECTOR_REF(data->regs, idx) = name;
    }

    minim_symbol_table_add(data->table, MINIM_SYMBOL(name), loc);
}

static void unregister_location(RegAllocData *data, MinimObject *name)
{
    MinimObject *ref;
    
    ref = minim_symbol_table_get(data->table, MINIM_SYMBOL(name));
    if (ref)
    {
        if (MINIM_OBJ_SYMBOLP(ref))
        {
            if (!minim_eqp(ref, intern(REG_TC_STR)))
            {
                UNRESERVE_REGISTER(data->regs, get_register_index(ref));
                minim_symbol_table_remove(data->table, MINIM_SYMBOL(name));
            }
        }
        else
        {
            minim_symbol_table_remove(data->table, MINIM_SYMBOL(name));
            if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(ref)), intern("$mem")))
            {
                size_t idx = MINIM_NUMBER_TO_UINT(MINIM_STX_VAL(MINIM_CADR(ref)));
                unreserve_temp_memory(data->memory, idx);
            }
        }
    }
}

static void unreserve_last_uses(RegAllocData *data, MinimObject *end_uses)
{
    for (MinimObject *it = end_uses; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *loc, *ref;

        loc = minim_symbol_table_get(data->table, MINIM_SYMBOL(MINIM_CAR(it)));
        ref = join_canonicalize(data->joins, MINIM_CAR(it));
        if (ref)
        {
            for (MinimObject *it2 = MINIM_VECTOR_REF(data->joins, 1); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                if (minim_eqp(MINIM_CAAR(it2), ref) && minim_falsep(MINIM_CDAR(it2)))
                {
                    MINIM_CDAR(it2) = MINIM_CAR(it);
                    break;
                }
            }
        }

        unregister_location(data, MINIM_CAR(it));
        minim_symbol_table_add(data->table, MINIM_SYMBOL(MINIM_CAR(it)), loc);
    }
}

static void eliminate_unwind(MinimEnv *env, Function *func)
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

static void record_memory(MinimEnv *env, Function *func)
{
    MinimObject *scratch_regs = minim_vector2(SCRATCH_REGISTER_COUNT, minim_false);
    size_t memory_count = 0;

    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$mov")))
        {
            MinimObject *target = MINIM_STX_VAL(MINIM_CADR(line));
            if (MINIM_STX_PAIRP(target))        // ($mem n)
            {

            }
            else
            {
                uint8_t index = get_register_index(target);
                if (index >= REG_T0 && index <= REG_T3)
                    MINIM_VECTOR_REF(scratch_regs, index - REG_T0) = minim_true;
            }
        }
    }

    for (size_t i = 0; i < SCRATCH_REGISTER_COUNT; ++i) {
        if (minim_truep(MINIM_VECTOR_REF(scratch_regs, i)))
            ++memory_count;
    }

    // round up to nearest multiple of two
    if (memory_count % 2) {
        func->stash = minim_vector2(memory_count + 1, minim_true);
        MINIM_VECTOR_REF(func->stash, memory_count) = minim_false;
    } else {
        func->stash = minim_vector2(memory_count, minim_false);
    }

    // store scratch register information
    for (size_t i = 0, j = 0; i < SCRATCH_REGISTER_COUNT; ++i) {
        if (minim_truep(MINIM_VECTOR_REF(scratch_regs, i))) {
            MINIM_VECTOR_REF(func->stash, j) = get_register_symbol(j + REG_T0);
            ++j;
        }
    }
}

void function_register_allocation(MinimEnv *env, Function *func)
{
    RegAllocData data;
    MinimObject *prev;

    // intialize register allocation data structure
    data.env = env;
    data.func = func;
    init_minim_symbol_table(&data.table);
    init_minim_symbol_table(&data.tail_syms);
    data.regs = minim_vector2(REGISTER_COUNT, minim_false);
    data.memory = minim_vector2(2, minim_null);
    data.joins = minim_vector2(2, minim_null);
    data.last_uses = NULL;
    data.prev = &prev;
    
    // populate tables
    compute_joins(env, &data);
    compute_tail_symbols(env, &data);
    data.last_uses = compute_last_use(env, &data);
    prev = NULL;

    // add temporary 0th instruction
    func->pseudo = minim_cons(minim_null, func->pseudo);
    prev = func->pseudo;

    for (MinimObject *it = MINIM_CDR(func->pseudo); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op, *end_uses;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        end_uses = get_lasts(data.last_uses, MINIM_CAR(it));

        if (minim_eqp(op, intern("$push-env")))
        {
            // =>
            //  $tc = init_env($tc)

            prepare_call(&data, false);
            INSERT_INSTR(prev, move_instruction(
                minim_ast(intern(REG_RT_STR), NULL),
                minim_ast(
                    minim_cons(minim_ast(intern("$addr"), NULL),
                    minim_cons(minim_ast(intern("init_env"), NULL),
                    minim_null)),
                    NULL)));
            
            //  ($call $rt)
            INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));

            //  ($mov $tc $rt)
            MINIM_CAR(it) = move_instruction(
                    minim_ast(intern(REG_TC_STR), NULL),
                    minim_ast(intern(REG_RT_STR), NULL));

            UNRESERVE_REGISTER(data.regs, REG_RT);
        }
        else if (minim_eqp(op, intern("$pop-env")))
        {
            //  ($mov $tc ($access $tc <type> <field>)
            MINIM_CAR(it) = minim_ast(
                minim_cons(minim_ast(intern("$env-parent"), NULL),
                minim_cons(minim_ast(intern(REG_TC_STR), NULL),
                minim_null)),
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
                minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_R0_STR));
                MINIM_VECTOR_REF(data.regs, REG_R0) = name;
                break;

            case 1:
                minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_R1_STR));
                MINIM_VECTOR_REF(data.regs, REG_R1) = name;
                break;

            case 2:
                minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_R2_STR));
                MINIM_VECTOR_REF(data.regs, REG_R2) = name;
                break;
            
            default:
                mem_offset = idx - ARG_REGISTER_COUNT;
                allocate_arg_memory(data.memory, mem_offset + 1);
                set_argument_memory(data.memory, mem_offset, name);
                minim_symbol_table_add(data.table, MINIM_SYMBOL(name),
                    minim_cons(intern("$arg-mem"),
                    minim_cons(uint_to_minim_number(mem_offset),
                    minim_null)));
            }

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
            bool is_tail;

            name = MINIM_STX_VAL(MINIM_CADR(line));
            val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            is_tail = minim_symbol_table_get(data.tail_syms, MINIM_STX_SYMBOL(MINIM_CADR(line)));
            if (MINIM_OBJ_SYMBOLP(val))
            {
                MinimObject *reg, *src;

                reg = existing_or_fresh_register(&data, name, REG_REPLACE_TEMP_ONLY);
                src = minim_symbol_table_get(data.table, MINIM_SYMBOL(val));

                // ($mov <regd> <regt>)
                if (!minim_eqp(reg, src))
                {
                    MINIM_CAR(it) = move_instruction(
                        minim_ast(reg, NULL),
                        minim_ast(src, NULL));
                }
                else
                {
                    MINIM_CDR(prev) = MINIM_CDR(it);
                    it = prev;
                }

                minim_symbol_table_remove(data.table, MINIM_SYMBOL(name));
                minim_symbol_table_add(data.table, MINIM_SYMBOL(name), reg);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$eval")))
            {
                MinimObject *op, *it2, *tc_tmp, *tc_sym;
                size_t argc, idx;
                
                it2 = MINIM_CDDR(val);
                argc = minim_list_length(it2);
                idx = 0;

                // stash values in argument registers
                prepare_call(&data, is_tail);

                // load values into argument registers
                while (!minim_nullp(it2) && idx < ARG_REGISTER_COUNT)
                {
                    MinimObject *name, *loc, *src;
                    
                    name = MINIM_STX_VAL(MINIM_CAR(it2));
                    src = minim_symbol_table_get(data.table, MINIM_SYMBOL(name));
                    if (!in_argument_location(src, idx))
                    {
                        loc = argument_location(&data, name, idx);
                        INSERT_INSTR(prev, move_instruction(minim_ast(loc, NULL),
                                                            minim_ast(src, NULL)));

                        unreserve_location(&data, src);
                        minim_symbol_table_set(data.table, MINIM_SYMBOL(name), loc);
                    }

                    it2 = MINIM_CDR(it2);
                    ++idx;
                }                

                if (!minim_nullp(it2))
                {
                    for (it2 = minim_list_reverse(it2); idx < argc; it2 = MINIM_CDR(it2))
                    {
                        MinimObject *arg, *loc, *src;
                    
                        arg = MINIM_STX_VAL(MINIM_CAR(it2));
                        loc = argument_location(&data, arg, idx);
                        src = minim_symbol_table_get(data.table, MINIM_SYMBOL(arg));
                        
                        // ($mov <reg> <arg>)
                        if (!minim_eqp(loc, src))
                        {
                            INSERT_INSTR(prev, move_instruction(minim_ast(loc, NULL),
                                                                minim_ast(src, NULL)));
                            unreserve_location(&data, src);
                        }

                        ++idx;
                    }
                }

                op = minim_symbol_table_get(data.table, MINIM_STX_SYMBOL(MINIM_CADR(val)));
                if (!MINIM_OBJ_SYMBOLP(op))
                {
                    op = temp_register(&data, name);
                    MINIM_VECTOR_REF(data.regs, get_register_index(op)) = name;
                }
                
                if (!is_tail)
                {
                    // stash $tc
                    tc_sym = intern("$tc");
                    tc_tmp = fresh_register(&data, tc_sym, REG_REPLACE_TEMP_ONLY);
                    INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                    //  ($call <reg>)
                    INSERT_INSTR(prev, call_instruction(minim_ast(op, NULL)));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;

                    // restore $tc
                    MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                    unreserve_location(&data, tc_tmp);
                }
                else
                {
                    MINIM_CAR(it) = call_instruction(minim_ast(op, NULL));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;
                }


            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$quote")))
            {
                MinimObject *stx, *tc_sym, *tc_tmp;

                stx = MINIM_CADR(val);
                prepare_call(&data, is_tail);
                reserve_register(&data, REG_R0);

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

                if (!is_tail)
                {
                    // stash $tc
                    tc_sym = intern("$tc");
                    tc_tmp = fresh_register(&data, tc_sym, REG_REPLACE_TEMP_ONLY);
                    INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                    //  ($call $rt)
                    INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;

                    // restore $tc
                    MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                    unreserve_location(&data, tc_tmp);
                }
                else
                {
                    MINIM_CAR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;
                }

                // cleanup
                UNRESERVE_REGISTER(data.regs, REG_R0);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$interpret")))
            {
                MinimObject *stx, *tc_sym, *tc_tmp;

                stx = MINIM_CADR(val);
                prepare_call(&data, is_tail);
                reserve_register(&data, REG_R0);

                // ($mov $rt ($addr <quote>))
                INSERT_INSTR(prev, move_instruction(
                    minim_ast(intern(REG_RT_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(intern("interpret"), NULL),
                        minim_null)),
                        NULL)));

                // ($mov $r0 ($syntax <quote-syntax>))
                INSERT_INSTR(prev, move_instruction(
                    minim_ast(intern(REG_R0_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$syntax"), NULL),
                        minim_cons(stx, minim_null)),
                        NULL)));

                if (!is_tail)
                {
                    // stash $tc
                    tc_sym = intern("$tc");
                    tc_tmp = fresh_register(&data, tc_sym, REG_REPLACE_TEMP_ONLY);
                    INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                    //  ($call $rt)
                    INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;

                    // restore $tc
                    MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                    unreserve_location(&data, tc_tmp);
                }
                else
                {
                    MINIM_CAR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;
                }

                // cleanup
                UNRESERVE_REGISTER(data.regs, REG_R0);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$top")))
            {
                // TODO: top-level symbols should be checked before compilation
                MinimObject *reg_or_mem = existing_or_fresh_register(&data, name, REG_REPLACE_TEMP_ONLY);

                // ($mov <reg/mem> ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg_or_mem, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(MINIM_STX_VAL(MINIM_CADR(val)), NULL),
                        minim_null)),
                        NULL));

                minim_symbol_table_add(data.table, MINIM_SYMBOL(name), reg_or_mem);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$load")))
            {
                MinimObject *tc_sym, *tc_tmp;

                prepare_call(&data, is_tail);
                reserve_register(&data, REG_R0);

                // ($mov $r0 ($sym-addr <sym>>
                INSERT_INSTR(prev, move_instruction(
                    minim_ast(intern(REG_R0_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$sym-addr"), NULL),
                        minim_cons(MINIM_CADR(val),
                        minim_null)),
                        NULL)));

                // ($mov $rt ($addr <symbol>))
                INSERT_INSTR(prev, move_instruction(
                    minim_ast(intern(REG_RT_STR), NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$addr"), NULL),
                        minim_cons(minim_ast(intern("get_sym"), NULL),
                        minim_null)),
                        NULL)));
                
                if (!is_tail)
                {
                    // stash $tc
                    tc_sym = intern("$tc");
                    tc_tmp = fresh_register(&data, tc_sym, REG_REPLACE_TEMP_ONLY);
                    INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                    //  ($call $rt)
                    INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;

                    // restore $tc
                    MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                    unreserve_location(&data, tc_tmp);
                }
                else
                {
                    MINIM_CAR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL));
                    minim_symbol_table_add(data.table, MINIM_SYMBOL(name), intern(REG_RT_STR));
                    MINIM_VECTOR_REF(data.regs, REG_RT) = name;
                }

                UNRESERVE_REGISTER(data.regs, REG_R0);
            }
            else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$func")))
            {
                MinimObject *reg, *fname;
                
                fname = MINIM_STX_VAL(MINIM_CADR(val));
                reg = existing_or_fresh_register(&data, name, REG_REPLACE_TEMP_ONLY);
                minim_symbol_table_add(data.table, MINIM_SYMBOL(name), reg);

                // ($mov $rt ($addr <symbol>))
                MINIM_CAR(it) = move_instruction(
                    minim_ast(reg, NULL),
                    minim_ast(
                        minim_cons(minim_ast(intern("$func-addr"), NULL),
                        minim_cons(minim_ast(fname, NULL),
                        minim_null)),
                        NULL));
            }
            else
            {
                THROW(env, minim_error("other form of set", "register allocation"));
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
        }
        else if (minim_eqp(op, intern("$gofalse")))
        {
            MinimObject *label, *arg, *loc;
            
            label = MINIM_STX_VAL(MINIM_CADR(line));
            arg = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            loc = minim_symbol_table_get(data.table, MINIM_SYMBOL(arg));

            // ($jmpz <label> <arg>)
            MINIM_CAR(it) = minim_ast(
                minim_cons(minim_ast(intern("$jmpz"), NULL),
                minim_cons(minim_ast(label, NULL),
                minim_cons(minim_ast(loc, NULL),
                minim_null))),
                NULL);
        }
        else if (minim_eqp(op, intern("$bind")))
        {
            // => ($bind sym val)
            //  intern(sym, val)
            //

            MinimObject *name, *val, *loc, *tc_sym, *tc_tmp;

            // stash values in argument registers
            prepare_call(&data, false);
            reserve_register(&data, REG_R0);
            reserve_register(&data, REG_R1);
            
            name = MINIM_STX_VAL(MINIM_CADR(line));
            val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            loc = minim_symbol_table_get(data.table, MINIM_SYMBOL(val));

            // ($mov $r1 <loc>)
            INSERT_INSTR(prev, move_instruction(
                minim_ast(intern(REG_R1_STR), NULL),
                minim_ast(loc, NULL)));

            // ($mov $r0 ($sym-addr <sym>>
            INSERT_INSTR(prev, move_instruction(
                minim_ast(intern(REG_R0_STR), NULL),
                minim_ast(
                    minim_cons(minim_ast(intern("$sym-addr"), NULL),
                    minim_cons(minim_ast(name, NULL),
                    minim_null)),
                    NULL)));
            
            // ($mov $rt ($addr set_sym))
            INSERT_INSTR(prev, move_instruction(
                minim_ast(intern(REG_RT_STR), NULL),
                minim_ast(
                    minim_cons(minim_ast(intern("$addr"), NULL),
                    minim_cons(minim_ast(intern("set_sym"), NULL),
                    minim_null)),
                    NULL)));

            if (!minim_symbol_table_get(data.tail_syms, MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line)))))
            {
                // stash $tc
                tc_sym = intern("$tc");
                tc_tmp = fresh_register(&data, tc_sym, REG_REPLACE_TEMP_ONLY);
                INSERT_INSTR(prev, move_instruction(minim_ast(tc_tmp, NULL), minim_ast(tc_sym, NULL)));

                //  ($call $rt)
                INSERT_INSTR(prev, call_instruction(minim_ast(intern(REG_RT_STR), NULL)));

                // restore $tc
                MINIM_CAR(it) = move_instruction(minim_ast(tc_sym, NULL), minim_ast(tc_tmp, NULL));
                unreserve_location(&data, tc_tmp);
            }
            else
            {
                MINIM_CAR(it) = call_instruction(minim_ast(intern(REG_RT_STR), NULL));
            }
            
            UNRESERVE_REGISTER(data.regs, REG_RT);
            UNRESERVE_REGISTER(data.regs, REG_R0);
            UNRESERVE_REGISTER(data.regs, REG_R1);
        }
        else if (minim_eqp(op, intern("$return")))
        {
            MinimObject *ref = minim_symbol_table_get(data.table, MINIM_STX_SYMBOL(MINIM_CADR(line)));
            if (!ref)
            {
                THROW(env, minim_error("return symbol ~s not found",
                                       "register allocation",
                                       MINIM_STX_SYMBOL(MINIM_CADR(line))));
            }
            else if (!minim_eqp(ref, intern(REG_RT_STR)))
            {
                INSERT_INSTR(prev, minim_ast(
                    minim_cons(minim_ast(intern("$mov"), NULL),
                    minim_cons(minim_ast(intern(REG_RT_STR), NULL),
                    minim_cons(minim_ast(ref, NULL),
                    minim_null))),
                    NULL));
            }
            
            MINIM_CAR(it) = minim_ast(
                    minim_cons(minim_ast(intern("$ret"), NULL),
                    minim_null),
                    NULL);
        }
        else if (minim_eqp(op, intern("$join")))
        {
            MinimObject *canon, *lhs, *rhs, *lhs_loc, *rhs_loc;

            canon = MINIM_STX_VAL(MINIM_CADR(line));
            lhs = MINIM_STX_VAL(MINIM_CDR(MINIM_CDDR(line)));
            rhs = MINIM_STX_VAL(MINIM_CADR(MINIM_CDDR(line)));

            lhs_loc = join_register(data.joins, data.table, canon);
            rhs_loc = minim_symbol_table_get(data.table, MINIM_SYMBOL(rhs));
            if (!minim_equalp(lhs_loc, rhs_loc))
                THROW(env, minim_error("join arguments are not in the same location", "register allocation"));

            unregister_location(&data, lhs);
            unregister_location(&data, rhs);
            register_location(&data, canon, lhs_loc);

            MINIM_CDR(prev) = MINIM_CDR(it);
            it = prev;
        }
        else if (minim_eqp(op, intern("$label")))
        {
            MinimObject *next_line, *next_op;

            next_line = MINIM_STX_VAL(MINIM_CADR(it));
            next_op = MINIM_STX_VAL(MINIM_CAR(next_line));
            if (minim_eqp(next_op, intern("$join")))
            {
                MinimObject *rhs = MINIM_STX_VAL(MINIM_CADR(MINIM_CDDR(next_line)));
                for (MinimObject *it = MINIM_VECTOR_REF(data.joins, 0); !minim_nullp(it); it = MINIM_CDR(it))
                {
                    if (minim_eqp(MINIM_CAAR(it), rhs))
                    {
                        MinimObject *ref, *loc;
                        ref = minim_symbol_table_get(data.table, MINIM_SYMBOL(MINIM_CAAR(it)));
                        loc = join_register(data.joins, data.table, MINIM_CDAR(it));

                        // move required
                        if (!minim_equalp(ref, loc))
                        {
                            unregister_location(&data, MINIM_CAAR(it));
                            register_location(&data, MINIM_CAAR(it), loc);
                            INSERT_INSTR(prev, move_instruction(minim_ast(loc, NULL), minim_ast(ref, NULL)));
                        }
                    }
                }
            }
        }
        else
        {
            THROW(env, minim_error("unknown pseudo-instruction ~s",
                                   "register allocation",
                                   MINIM_SYMBOL(op)));
        }

        unreserve_last_uses(&data, end_uses);
        prev = it;
    }

    // remove temporary instruction
    func->pseudo = MINIM_CDR(func->pseudo);

    // optimize
    eliminate_unwind(env, func);
    record_memory(env, func);
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
