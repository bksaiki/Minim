#include "../core/minimpriv.h"
#include "compilepriv.h"

static void
eliminate_join(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *line, *trailing;

    trailing = NULL;
    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        line = MINIM_STX_VAL(MINIM_CAR(it));
        if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(line)), intern("$join")))
        {
            minim_symbol_table_add(table,
                                   MINIM_STX_SYMBOL(MINIM_CAR(MINIM_CDDR(line))),
                                   MINIM_CADR(line));
            minim_symbol_table_add(table,
                                   MINIM_STX_SYMBOL(MINIM_CADR(MINIM_CDDR(line))),
                                   MINIM_CADR(line));

            if (trailing == NULL)   func->pseudo = MINIM_CDR(it);
            else                    MINIM_CDR(trailing) = MINIM_CDR(it);
        }
        else
        {
            trailing = it;
        }
    }

    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        line = MINIM_STX_VAL(MINIM_CAR(it));
        for (MinimObject *it2 = MINIM_CDR(line); !minim_nullp(it2); it2 = MINIM_CDR(it2))
        {
            MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CAR(it2)));
            if (ref)    MINIM_CAR(it2) = ref;
        }
    }
}

static bool
constant_fold(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *line;
    bool changed = false;

    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line = MINIM_STX_VAL(MINIM_CAR(it));
        if (!minim_eqp(MINIM_STX_VAL(MINIM_CAR(line)), intern("$set")))
        {
            

        }
    }

    return changed;
}

static bool
constant_propagation(MinimEnv *env, Function *func)
{
    MinimSymbolTable *table;
    MinimObject *line, *trailing;
    bool changed = false;

    trailing = NULL;
    init_minim_symbol_table(&table);
    for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
    {
        line = MINIM_STX_VAL(MINIM_CAR(it));
        if (minim_eqp(MINIM_STX_VAL(MINIM_CAR(line)), intern("$set")) &&
            MINIM_STX_SYMBOLP(MINIM_CADR(line)) &&
            MINIM_STX_SYMBOLP(MINIM_CAR(MINIM_CDDR(line))))
        {
            minim_symbol_table_add(table, MINIM_STX_SYMBOL(MINIM_CADR(line)), MINIM_CAR(MINIM_CDDR(line)));
            if (trailing == NULL)   func->pseudo = MINIM_CDR(it);
            else                    MINIM_CDR(trailing) = MINIM_CDR(it);
            changed = true;
        }
        else
        {
            for (MinimObject *it2 = MINIM_CDR(line); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                MinimObject *ref = minim_symbol_table_get(table, MINIM_STX_SYMBOL(MINIM_CAR(it2)));
                if (ref)    MINIM_CAR(it2) = ref;
            }

            trailing = it;
        }
    }

    return changed;
}

static bool
code_elimination(MinimEnv *env, Function *func)
{
    return false;
}

void function_optimize(MinimEnv *env, Function *func)
{
    size_t iter;
    bool changed;

    // do once
    eliminate_join(env, func);

    // do many
    iter = 1;
    changed = true;
    while (changed)
    {
        changed = false;
        changed |= constant_fold(env, func);
        changed |= constant_propagation(env, func);
        changed |= code_elimination(env, func);

        printf("   optimization iteration: %zu\n", iter);
        iter += 1;
    }
    
    // debugging
    debug_function(env, func);
}
