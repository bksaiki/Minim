#include "../core/minimpriv.h"
#include "compilepriv.h"

static void
eliminate_join(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *reverse, *trailing;

    trailing = NULL;
    reverse = minim_list_reverse(func->pseudo);
    init_minim_symbol_table(&table);
    for (MinimObject *it = reverse; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$join")))
        {
            minim_symbol_table_add(table,
                                   MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))),
                                   MINIM_CADR(line));
            minim_symbol_table_add(table,
                                   MINIM_STX_SYMBOL(MINIM_CADR(MINIM_CDDR(line))),
                                   MINIM_CADR(line));
            MINIM_CDR(trailing) = MINIM_CDR(it);
        }
        else if (minim_eqp(op, intern("$set")))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CADR(line)));
            if (ref)    MINIM_CADR(line) = ref;
        }

        trailing = it;
    }


    func->pseudo = minim_list_reverse(reverse);

    // MinimSymbolTable *table;
    // MinimObject *line, *trailing;

    // trailing = NULL;
    // init_minim_symbol_table(&table);
    // for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    // {
    //     line = MINIM_STX_VAL(MINIM_CAR(it));
    //     if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(line)), intern("$join")))
    //     {
    //         minim_symbol_table_add(table,
    //                                MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))),
    //                                MINIM_CADR(line));
    //         minim_symbol_table_add(table,
    //                                MINIM_STX_SYMBOL(MINIM_CADR(MINIM_CDDR(line))),
    //                                MINIM_CADR(line));

    //         if (trailing == NULL)   func->pseudo = MINIM_CDR(it);
    //         else                    MINIM_CDR(trailing) = MINIM_CDR(it);
    //     }
    //     else
    //     {
    //         trailing = it;
    //     }
    // }

    // for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    // {
    //     line = MINIM_STX_VAL(MINIM_CAR(it));
    //     for (MinimObject *it2 = MINIM_CDR(line); !minim_nullp(it2); it2 = MINIM_CDR(it2))
    //     {
    //         MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CAR(it2)));
    //         if (ref)    MINIM_CAR(it2) = ref;
    //     }
    // }
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
        vals[i] = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CAR(args)));
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
                        if (MINIM_BUILTIN(ref) == minim_builtin_length ||
                            MINIM_BUILTIN(ref) == minim_builtin_add ||
                            MINIM_BUILTIN(ref) == minim_builtin_sub ||
                            MINIM_BUILTIN(ref) == minim_builtin_mul)
                        {
                            MinimObject *res = eval_constant_expr(env, table, ref, MINIM_CDDR(value));
                            if (res)
                            {
                                MINIM_CAR(MINIM_CDDR(line)) = minim_ast(res, MINIM_STX_LOC(value));
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

static bool
constant_propagation(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *line;
    bool changed = false;

    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        line = MINIM_STX_VAL(MINIM_CAR(it));
        if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(line)), intern("$set")) &&
            MINIM_STX_SYMBOLP(MINIM_CADR(line)) &&
            MINIM_STX_SYMBOLP(MINIM_CAR(MINIM_CDDR(line))))
        {
            minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), MINIM_CAR(MINIM_CDDR(line)));
            changed = true;
        }
        else
        {
            for (MinimObject *it2 = MINIM_CDR(line); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CAR(it2)));
                if (ref)    MINIM_CAR(it2) = ref;
            }
        }
    }

    return changed;
}

static bool
code_elimination(MinimEnv *env, Function *func)
{
    MinimObject *reverse;
    MinimSymbolTable *table;
    size_t iter;
    bool eliminated;
    
    reverse = minim_list_reverse(func->pseudo);
    iter = 0;

    do {
        MinimObject *trailing;
        
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
                    eliminated = true;
                }
                else
                {
                    val = MINIM_CAR(MINIM_CDDR(line));
                    minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
                    if (MINIM_STX_PAIRP(val) && minim_eqp(MINIM_STX_VAL(MINIM_CAR(val)), intern("$eval")))
                    {
                        for (MinimObject *it2 = MINIM_CDR(val); !minim_nullp(it2); it2 = MINIM_CDR(it2))
                            minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CAR(it2)), minim_null);
                    }
                }
            }
            else if (minim_eqp(op, intern("$join")))
            {
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))), minim_null);
            }
            else if (minim_eqp(op, intern("$bind")) || minim_eqp(op, intern("$gotfalse")))
            {
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))), minim_null);
            }
            else if (minim_eqp(op, intern("$ret")))
            {
                minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), minim_null);
            }

            trailing = it;
        }

        ++iter;
    } while (eliminated);

    func->pseudo = minim_list_reverse(reverse);
    return iter > 1;
}

void function_optimize(MinimEnv *env, Function *func)
{
    size_t iter;
    bool changed;

    // do many
    iter = 1;
    changed = true;
    while (changed)
    {
        changed = false;

        printf("   optimization iteration: %zu\n", iter);
        changed |= constant_fold(env, func);
        changed |= constant_propagation(env, func);
        changed |= code_elimination(env, func);
        iter += 1;
    }

    // do at the end
    eliminate_join(env, func);
    
    // debugging
    debug_function(env, func);
}
