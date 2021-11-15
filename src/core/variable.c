#include <string.h>

#include "../common/path.h"
#include "../gc/gc.h"
#include "builtin.h"
#include "eval.h"
#include "error.h"
#include "lambda.h"
#include "number.h"

MinimObject *minim_builtin_def_values(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val;
    size_t bindc;

    bindc = syntax_list_len(args[0]);
    val = eval_ast_no_check(env, args[1]);
    if (bindc == 1)
    {
        if (MINIM_OBJ_VALUESP(val))
        {
            THROW(env, minim_values_arity_error("def-values",
                                                bindc,
                                                MINIM_VALUES_SIZE(val),
                                                args[0]));
        }

        env_intern_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(args[0])), val);
    }
    else
    {
        MinimObject *it;

        if (MINIM_VALUES_SIZE(val) != bindc)
        {
            THROW(env, minim_values_arity_error("def-values",
                                                bindc,
                                                MINIM_VALUES_SIZE(val),
                                                args[0]));
        }

        it = MINIM_STX_VAL(args[0]);
        for (size_t i = 0; i < bindc; ++i)
        {
            env_intern_sym(env, MINIM_STX_SYMBOL(MINIM_CAR(it)), MINIM_VALUES_REF(val, i));
            it = MINIM_CDR(it);
        }
    }

    return minim_void;
}

MinimObject *minim_builtin_quote(MinimEnv *env, size_t argc, MinimObject **args)
{
    return unsyntax_ast_rec(env, args[0]);
}

MinimObject *minim_builtin_quasiquote(MinimEnv *env, size_t argc, MinimObject **args)
{
    return unsyntax_ast_rec2(env, args[0]);
}

MinimObject *minim_builtin_unquote(MinimEnv *env, size_t argc, MinimObject **args)
{
    return eval_ast_no_check(env, args[0]);
}

MinimObject *minim_builtin_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val;

    val = eval_ast_no_check(env, args[1]);
    if (env_get_sym(env, MINIM_STX_SYMBOL(args[0])))
    {
        env_set_sym(env, MINIM_STX_SYMBOL(args[0]), val);
        return minim_void;
    }
    else
    {
        Buffer *bf;
        PrintParams pp;

        init_buffer(&bf);
        set_default_print_params(&pp);
        pp.quote = true;
        print_to_buffer(bf, MINIM_STX_VAL(args[0]), env, &pp);
        THROW(env, minim_error("not a variable", bf->data));
        return NULL;    // prevent compiler error
    }
}

MinimObject *minim_builtin_symbolp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_SYMBOLP(args[0]));
}

MinimObject *minim_builtin_equalp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_equalp(args[0], args[1]));
}

MinimObject *minim_builtin_eqvp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_eqvp(args[0], args[1]));
}

MinimObject *minim_builtin_eqp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_eqp(args[0], args[1]));
}

MinimObject *minim_builtin_version(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_string(MINIM_VERSION_STR);
}

MinimObject *minim_builtin_void(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_void;
}

MinimObject *minim_builtin_symbol_count(MinimEnv *env, size_t argc, MinimObject **args)
{
    return int_to_minim_number(env_symbol_count(env));
}

MinimObject *minim_builtin_dump_symbols(MinimEnv *env, size_t argc, MinimObject **args)
{
    env_dump_symbols(env);
    return minim_void;
}

MinimObject *minim_builtin_identifier_equalp(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *a, *b;
    
    a = eval_ast_no_check(env, args[0]);
    if (!MINIM_STX_SYMBOLP(a))
        THROW(env, minim_argument_error("identifier?", "identifier=?", 0, args[0]));

    b = eval_ast_no_check(env, args[1]);
    if (!MINIM_STX_SYMBOLP(b))
        THROW(env, minim_argument_error("identifier?", "identifier=?", 0, args[1]));

    return to_bool(minim_eqp(MINIM_STX_VAL(a), MINIM_STX_VAL(b)));
}
