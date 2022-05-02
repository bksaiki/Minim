#include "../core/minimpriv.h"
#include "compilepriv.h"

static MinimSymbolTable *
join_symbols(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;

    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;

        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$join")))
        {
            minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
            minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))), minim_null);
        }
    }

    return table;
}

static MinimSymbolTable *
return_symbols(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;

    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;

        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$return"))) {
            minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
        }
    }

    return table;
}

static MinimObject *
eval_constant_expr(MinimEnv *env,
                   MinimSymbolTable *table,
                   MinimObject *op,
                   MinimObject *args)
{
    MinimObject **vals;
    MinimObject *it;
    MinimBuiltin fn;
    size_t argc;
    
    argc = minim_list_length(args);
    vals = GC_alloc(argc * sizeof(MinimObject*));
    it = args;
    for (size_t i = 0; !minim_nullp(it); ++i, it = MINIM_CDR(it))
    {
        vals[i] = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CAR(it)));
        if (!vals[i])   return NULL;

        vals[i] = MINIM_STX_VAL(vals[i]);
    }

    fn = MINIM_BUILTIN(op);
    return fn(env, argc, vals);
}

static bool
constant_fold(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    bool changed = false;

    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line = MINIM_STX_VAL(MINIM_CAR(it));
        if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(line)), intern("$set")))
        {
            MinimObject *name, *value;

            name = MINIM_CADR(line);
            value = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            if (MINIM_OBJ_PAIRP(value))
            {
                if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(value)), intern("$quote")))
                {
                    minim_symbol_table_add(table, MINIM_STX_SYMBOL(name), MINIM_CADR(value));
                }
                else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(value)), intern("$top")))
                {
                    MinimObject *ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_CADR(value)));
                    if (ref)    minim_symbol_table_add(table, MINIM_STX_SYMBOL(name), ref);
                }
                else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(value)), intern("$eval")))
                {
                    MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(value)));
                    if (ref && MINIM_OBJ_BUILTINP(ref))
                    {
                        if (MINIM_BUILTIN(ref) == minim_builtin_eq      ||
                            MINIM_BUILTIN(ref) == minim_builtin_eqp     ||
                            MINIM_BUILTIN(ref) == minim_builtin_eqvp    ||
                            MINIM_BUILTIN(ref) == minim_builtin_length  ||
                            MINIM_BUILTIN(ref) == minim_builtin_add     ||
                            MINIM_BUILTIN(ref) == minim_builtin_sub     ||
                            MINIM_BUILTIN(ref) == minim_builtin_mul     ||
                            MINIM_BUILTIN(ref) == minim_builtin_listp   ||
                            MINIM_BUILTIN(ref) == minim_builtin_nullp)
                        {
                            MinimObject *res = eval_constant_expr(env, table, ref, MINIM_CDDR(value));
                            if (res)
                            {
                                MINIM_CAR(MINIM_CDDR(line)) = minim_ast(
                                    minim_cons(minim_ast(intern("$quote"), NULL),
                                    minim_cons(minim_ast(res, MINIM_STX_LOC(value)),
                                    minim_null)),
                                    NULL);
                                changed = true;
                            }
                        }
                    }
                }
            }
        }
    }

    return changed;
}

// TODO: what about $set, $bind, $goto
static bool
early_return(MinimEnv *env, Function *func)
{
    MinimSymbolTable *ret_syms, *join_syms, *exit_labels, *modified_syms;
    MinimObject *reverse;
    bool changed;

    ret_syms = return_symbols(env, func);
    init_minim_symbol_table(&join_syms);
    init_minim_symbol_table(&exit_labels);
    init_minim_symbol_table(&modified_syms);
    reverse = minim_list_reverse(func->pseudo);
    changed = false;

    // insert early returns
    for (MinimObject *it = MINIM_CDR(reverse); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
            
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$join")))
        {
            MinimObject *join = MINIM_CADR(line);
            if (minim_symbol_table_get(ret_syms, MINIM_STX_SYMBOL(join)) ||
                minim_symbol_table_get(join_syms, MINIM_STX_SYMBOL(join)))
            {
                MinimObject *ift, *iff;

                ift = MINIM_CAR(MINIM_CDDR(line));
                iff = MINIM_CADR(MINIM_CDDR(line));
                minim_symbol_table_add(join_syms, MINIM_STX_SYMBOL(ift), minim_null);
                minim_symbol_table_add(join_syms, MINIM_STX_SYMBOL(iff), minim_null);
            }
        }
        else if (minim_eqp(op, intern("$label")) || minim_eqp(op, intern("$goto")))
        {
            MinimObject *it2, *next_line, *next_op;

            it2 = it;
            next_line = MINIM_STX_VAL(MINIM_CADR(it2));
            next_op = MINIM_STX_VAL(MINIM_CAR(next_line));
            while (minim_eqp(next_op, intern("$pop-env")))
            {
                it2 = MINIM_CDR(it2);
                next_line = MINIM_STX_VAL(MINIM_CADR(it2));
                next_op = MINIM_STX_VAL(MINIM_CAR(next_line));
            }

            if (minim_eqp(next_op, intern("$set")))
            {
                MinimObject *ref;
                char *name;

                name = MINIM_STX_SYMBOL(MINIM_CADR(next_line));
                ref = minim_symbol_table_get(join_syms, name);
                if (ref)
                {
                    MINIM_CDR(it2) = minim_cons(minim_ast(
                        minim_cons(minim_ast(intern("$return"), NULL),
                        minim_cons(MINIM_CADR(next_line),
                        minim_null)),
                        NULL),
                        MINIM_CDR(it2)
                    );

                    minim_symbol_table_add(modified_syms, name, minim_null);
                    changed = true;
                }

                it = it2;
            }
        }
    }

    func->pseudo = minim_list_reverse(reverse);
    if (modified_syms->size > 0)
    {
        MinimSymbolTable *join_direct;
        MinimObject *trailing;

        trailing = NULL;
        init_minim_symbol_table(&join_direct);
        for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *line, *op;
                
            line = MINIM_STX_VAL(MINIM_CAR(it));
            op = MINIM_STX_VAL(MINIM_CAR(line));
            if (minim_eqp(op, intern("$join")))
            {
                MinimObject *join, *ift, *iff;

                join = MINIM_CADR(line);
                ift = MINIM_CAR(MINIM_CDDR(line));
                iff = MINIM_CADR(MINIM_CDDR(line));
                if (minim_symbol_table_get(ret_syms, MINIM_STX_SYMBOL(join)) ||
                    minim_symbol_table_get(join_syms, MINIM_STX_SYMBOL(join)))
                {
                    if (minim_symbol_table_get(modified_syms, MINIM_STX_SYMBOL(ift)) &&
                        minim_symbol_table_get(modified_syms, MINIM_STX_SYMBOL(iff)))
                    {
                        minim_symbol_table_add(modified_syms, MINIM_STX_SYMBOL(join), minim_null);
                        minim_symbol_table_add(join_direct, MINIM_STX_SYMBOL(join), ift);
                        MINIM_CDR(trailing) = MINIM_CDR(it);
                        it = trailing;
                    }
                    else if (minim_symbol_table_get(modified_syms, MINIM_STX_SYMBOL(ift)))
                    {
                        MINIM_CDR(trailing) = minim_cons(minim_ast(
                            minim_cons(minim_ast(intern("$set"), NULL),
                            minim_cons(join,
                            minim_cons(iff,
                            minim_null))),
                            NULL),
                            MINIM_CDR(it));
                        it = MINIM_CDR(trailing);
                    }
                    else if (minim_symbol_table_get(modified_syms, MINIM_STX_SYMBOL(iff)))
                    {
                        MINIM_CDR(trailing) = minim_cons(minim_ast(
                            minim_cons(minim_ast(intern("$set"), NULL),
                            minim_cons(join,
                            minim_cons(ift,
                            minim_null))),
                            NULL),
                            MINIM_CDR(it));
                        it = MINIM_CDR(trailing);
                    }
                    else
                    {
                        THROW(env, minim_error("not all join symbols registered: ~s ~s",
                                            "optimization [early return]",
                                                MINIM_STX_SYMBOL(ift),
                                                MINIM_STX_SYMBOL(iff)));
                    }
                }
            }
            else
            {
                for (MinimObject *it2 = MINIM_CDR(line); !minim_nullp(it2); it2 = MINIM_CDR(it2))
                {   
                    // TODO: this is gross
                    while (1)
                    {
                        MinimObject *ref = minim_symbol_table_get(join_direct, MINIM_STX_SYMBOL(MINIM_CAR(it2)));
                        if (!ref)   break;
                        MINIM_CAR(it2) = ref;
                    }  
                }
            }

            trailing = it;
        }
    }

    return changed;
}

static bool
constant_propagation(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table, *joins;
    bool changed = false;

    init_minim_symbol_table(&table);
    joins = join_symbols(env, func);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line = MINIM_STX_VAL(MINIM_CAR(it));
        if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(line)), intern("$set")))
        {
            MinimObject *val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
            if (MINIM_OBJ_SYMBOLP(val))
            {
                char *name = MINIM_STX_SYMBOL(MINIM_CADR(line));
                if (!minim_symbol_table_get(joins, name))
                    minim_symbol_table_add(table, name, MINIM_CAR(MINIM_CDDR(line)));
            }
        }
        else
        {
            for (MinimObject *it2 = MINIM_CDR(line); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CAR(it2)));
                if (ref)
                {
                    MINIM_CAR(it2) = ref;
                    changed = true;
                }
            }
        }
    }

    return changed;
}

static bool
eliminate_dead_code(MinimEnv *env, Function *func)
{
    MinimSymbolTable *labels, *unreachable_jmp;
    MinimObject *reverse, *trailing;
    size_t iter;
    bool changed, eliminated;
    
    reverse = minim_list_reverse(func->pseudo);
    iter = 0;

    // iteratively scan back to front for unused variables
    do {
        MinimSymbolTable *table;
        
        eliminated = false;
        trailing = NULL;
        init_minim_symbol_table(&table);
        for (MinimObject *it = reverse; !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *line, *op;
            
            line = MINIM_STX_VAL(MINIM_CAR(it));
            op = MINIM_STX_VAL(MINIM_CAR(line));
            if (minim_eqp(op, intern("$set")))
            {
                MinimObject *name, *val;
                
                name = MINIM_CADR(line);
                if (!minim_symbol_table_get(table, MINIM_STX_SYMBOL(name)))
                {
                    MINIM_CDR(trailing) = MINIM_CDR(it);
                    it = trailing;
                    eliminated = true;
                }
                else
                {
                    minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
                    val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
                    if (MINIM_OBJ_SYMBOLP(val))
                    {
                        minim_symbol_table_add(table, MINIM_SYMBOL(val), minim_null);
                    }
                    else if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$eval")))
                    {
                        // always add result
                        minim_symbol_table_add(table, MINIM_SYMBOL(name), minim_null);
                        for (MinimObject *it2 = MINIM_CDR(val); !minim_nullp(it2); it2 = MINIM_CDR(it2))
                            minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CAR(it2)), minim_null);
                    }
                }
            }
            else if (minim_eqp(op, intern("$arg")))
            {
                MinimObject *name = MINIM_CADR(line);
                if (!minim_symbol_table_get(table, MINIM_STX_SYMBOL(name)))
                {
                    MINIM_CDR(trailing) = MINIM_CDR(it);
                    it = trailing;
                    eliminated = true;
                }   
            }
            else if (minim_eqp(op, intern("$join")))
            {
                MinimObject *ift, *iff;
                
                ift = MINIM_CAR(MINIM_CDDR(line));
                iff = MINIM_CADR(MINIM_CDDR(line));
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(ift), minim_null);
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(iff), minim_null);
            }
            else if (minim_eqp(op, intern("$bind")) || minim_eqp(op, intern("$gofalse")))
            {
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))), minim_null);
            }
            else if (minim_eqp(op, intern("$return")))
            {
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
            }

            trailing = it;
        }

        ++iter;
    } while (eliminated);

    func->pseudo = minim_list_reverse(reverse);
    init_minim_symbol_table(&labels);
    init_minim_symbol_table(&unreachable_jmp);
    changed = iter > 1;
    trailing = NULL;

    // scan once forward to remove old labels
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
            
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));

        if (minim_eqp(op, intern("$goto")) || minim_eqp(op, intern("$gofalse")))
        {
            minim_symbol_table_add(labels, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
        }
        else if (minim_eqp(op, intern("$label")))
        {
            if (!minim_symbol_table_get(labels, MINIM_STX_SYMBOL(MINIM_CADR(line))))
            {
                MINIM_CDR(trailing) = MINIM_CDR(it);
                it = trailing;
                changed = true;
            }
        }
        else if (minim_eqp(op, intern("$return")) && !minim_nullp(MINIM_CDR(it)))
        {
            MinimObject *next_line, *next_op;

            next_line = MINIM_STX_VAL(MINIM_CADR(it));
            next_op = MINIM_STX_VAL(MINIM_CAR(next_line));
            while (!minim_eqp(next_op, intern("$label")))
            {
                if (minim_eqp(next_op, intern("$goto")))
                    minim_symbol_table_add(unreachable_jmp, MINIM_STX_SYMBOL(MINIM_CADR(next_line)), minim_null);

                MINIM_CDR(it) = MINIM_CDDR(it);
                changed = true;

                if (minim_nullp(MINIM_CDR(it)))
                    break;

                next_line = MINIM_STX_VAL(MINIM_CADR(it));
                next_op = MINIM_STX_VAL(MINIM_CAR(next_line));
            }
        }

        trailing = it;
    }

    return changed;
}

// static void
// eliminate_join(MinimEnv *env, Function *func)
// {
//     MinimSymbolTable *table;
//     MinimObject *reverse, *trailing;

//     trailing = NULL;
//     reverse = minim_list_reverse(func->pseudo);
//     init_minim_symbol_table(&table);
//     for (MinimObject *it = reverse; !minim_nullp(it); it = MINIM_CDR(it))
//     {
//         MinimObject *line, *op;
        
//         line = MINIM_STX_VAL(MINIM_CAR(it));
//         op = MINIM_STX_VAL(MINIM_CAR(line));
//         if (minim_eqp(op, intern("$join")))
//         {
//             MinimObject *replace, *ref;
            
//             replace = MINIM_CADR(line);
//             ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(replace));
//             while (ref)
//             {
//                 replace = ref;
//                 ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(replace));
//             }

//             minim_symbol_table_add(table,
//                                    MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))),
//                                    replace);

//             minim_symbol_table_add(table,
//                                    MINIM_STX_SYMBOL(MINIM_CADR(MINIM_CDDR(line))),
//                                    replace);

//             MINIM_CDR(trailing) = MINIM_CDR(it);
//             it = trailing;
//         }
//         else if (minim_eqp(op, intern("$set")))
//         {
//             MinimObject *name, *val, *ref;
            
//             name = MINIM_CADR(line);
//             ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(name));
//             if (ref)
//             {
//                 MINIM_CADR(line) = ref;
//             }
//             else
//             {
//                 val = MINIM_STX_VAL(MINIM_CAR(MINIM_CDDR(line)));
//                 if (MINIM_OBJ_PAIRP(val) && minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$eval")))
//                 {
//                     for (MinimObject *it2 = MINIM_CDR(val); !minim_nullp(it2); it2 = MINIM_CDR(it2))
//                     {
//                         name = MINIM_CAR(it2);
//                         ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(name));
//                         if (ref)    MINIM_CAR(it2) = ref;
//                     }
//                 }
//             }
//         }
//         else if (minim_eqp(op, intern("$arg")))
//         {
//             MinimObject *ref, *name;

//             name = MINIM_CADR(line);
//             ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(name));
//             if (ref)    MINIM_CADR(line) = ref;
//         }
//         else if (minim_eqp(op, intern("$bind")) || minim_eqp(op, intern("$gofalse")))
//         {
//             MinimObject *ref, *name;

//             name = MINIM_CAR(MINIM_CDDR(line));
//             ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(name));
//             if (ref)    MINIM_CAR(MINIM_CDDR(line)) = ref;
//         }

//         trailing = it;
//     }

//     func->pseudo = minim_list_reverse(reverse);
// }

static bool
consolidate_labels(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *reverse, *trailing, *curr_label;
    bool changed;

    reverse = minim_list_reverse(func->pseudo);
    init_minim_symbol_table(&table);
    trailing = NULL, curr_label = NULL;
    changed = false;

    for (MinimObject *it = reverse; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$label")))
        {
            MinimObject *label = MINIM_CADR(line);
            if (curr_label)
            {
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(label), curr_label);
                MINIM_CDR(trailing) = MINIM_CDR(it);
                it = trailing;
                changed = true;
            }
            else
            {
                curr_label = label;
            }
        }
        else if (minim_eqp(op, intern("$goto")) || minim_eqp(op, intern("$gofalse")))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(line)));
            if (ref)
            {
                MINIM_CADR(line) = ref;
                changed = true;
            }

            curr_label = NULL;
        }
        else
        {
            curr_label = NULL;
        }

        trailing = it;
    }

    func->pseudo = minim_list_reverse(reverse);
    return changed;
}

static bool
eliminate_immediate_goto(MinimEnv *env, Function *func)
{
    MinimObject *trailing = NULL;
    bool changed = false;

    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$goto")) || minim_eqp(op, intern("$gofalse")))
        {
            MinimObject *label, *next_line, *op2;
            
            label = MINIM_CADR(line);
            next_line = MINIM_STX_VAL(MINIM_CADR(it));
            op2 = MINIM_STX_VAL(MINIM_CAR(next_line));
            if (minim_eqp(op2, intern("$label")))
            {
                MinimObject *label2 = MINIM_CADR(next_line);
                if (MINIM_STX_SYMBOL(label) == MINIM_STX_SYMBOL(label2))
                {
                    MINIM_CDR(trailing) = MINIM_CDDR(it);
                    it = trailing;
                    changed = true;
                }
            }
        }

        trailing = it;
    }

    return changed;
}

static bool
eliminate_unwind(MinimEnv *env, Function *func)
{
    MinimObject *reverse, *trailing;
    bool changed;

    reverse = minim_list_reverse(func->pseudo);
    trailing = reverse;
    changed = false;

    for (MinimObject *it = MINIM_CDR(reverse); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$pop-env")))
        {
            MINIM_CDR(trailing) = MINIM_CDR(it);
            it = trailing;
            changed = true;
        }
        else
        {
            break;
        }

        trailing = it;
    }

    func->pseudo = minim_list_reverse(reverse);
    return changed;
}

void function_optimize(MinimEnv *env, Function *func)
{
    size_t iter;
    bool changed;

    // do until no change
    iter = 1;
    changed = true;
    while (changed)
    {
        changed = false;
        changed |= constant_fold(env, func);
        changed |= constant_propagation(env, func);
        changed |= early_return(env, func);
        changed |= eliminate_dead_code(env, func);

        changed |= eliminate_unwind(env, func);
        changed |= eliminate_immediate_goto(env, func);

        // printf("(iter %zu)", iter);
        // debug_function(env, func);
        iter += 1;
    }

    // do once
    consolidate_labels(env, func);
}
