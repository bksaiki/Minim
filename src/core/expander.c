#include "minimpriv.h"

#define EXPAND_REC(ref, fn, exp, env, stx)    if (MINIM_SYNTAX(ref) == (fn)) { return exp(env, stx); }

// foward declaration
MinimObject *expand_expr(MinimEnv *env, MinimObject *stx);
MinimObject *expand_definition_level(MinimEnv *env, MinimObject *stx);

static MinimObject *expand_expr_begin(MinimEnv *env, MinimObject *stx)
{
    for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
        MINIM_CAR(it) = expand_expr(env, MINIM_CAR(it));
    return stx;
}

static MinimObject *expand_expr_1arg(MinimEnv *env, MinimObject *stx)
{
    MinimObject *arg = MINIM_STX_CDR(stx);
    MINIM_CAR(arg) = expand_expr(env, MINIM_CAR(arg));
    return stx;
}

static MinimObject *expand_expr_if(MinimEnv *env, MinimObject *stx)
{
    MinimObject *t = MINIM_STX_CDR(stx);
    MINIM_CAR(t) = expand_expr(env, MINIM_CAR(t));
    t = MINIM_CDR(t);
    MINIM_CAR(t) = expand_expr(env, MINIM_CAR(t));
    t = MINIM_CDR(t);
    MINIM_CAR(t) = expand_expr(env, MINIM_CAR(t));
    return stx;
}

static MinimObject *expand_expr_lambda(MinimEnv *env, MinimObject *stx)
{
    for (MinimObject *it = MINIM_CDR(MINIM_STX_CDR(stx)); !minim_nullp(it); it = MINIM_CDR(it))
        MINIM_CAR(it) = expand_definition_level(env, MINIM_CAR(it));
    return stx;
}

static MinimObject *expand_expr_let_values(MinimEnv *env, MinimObject *stx)
{
    MinimObject *body, *bind;

    body = MINIM_STX_CDR(stx);
    for (MinimObject *it = MINIM_STX_VAL(MINIM_CAR(body)); !minim_nullp(it); it = MINIM_CDR(it))
    {
        bind = MINIM_STX_CDR(MINIM_CAR(it));
        MINIM_CAR(bind) = expand_expr(env, MINIM_CAR(bind));
    }

    body = MINIM_CDR(body);
    for (; !minim_nullp(body); body = MINIM_CDR(body))
        MINIM_CAR(body) = expand_expr(env, MINIM_CAR(body));

    return stx;
}

static MinimObject *expand_expr_setb(MinimEnv *env, MinimObject *stx)
{
    MinimObject *val = MINIM_CDR(MINIM_STX_CDR(stx));
    MINIM_CAR(val) = expand_expr_setb(env, MINIM_CAR(val));
    return stx;
}

static MinimObject *expand_expr_syntax_case(MinimEnv *env, MinimObject *stx)
{
    MinimObject *it, *clause;

    it = MINIM_STX_CDR(stx);
    MINIM_CAR(it) = expand_expr(env, MINIM_CAR(it));
    for (it = MINIM_CDDR(it); !minim_nullp(it); it = MINIM_CDR(it))
    {
        clause = MINIM_STX_CDR(MINIM_CAR(it));
        MINIM_CAR(clause) = expand_expr(env, MINIM_CAR(clause));
    }

    return stx;
}

MinimObject *expand_expr(MinimEnv *env, MinimObject *stx)
{
    if (MINIM_STX_PAIRP(stx))
    {
        // early exit
        if (MINIM_STX_NULLP(stx))
            return stx;
        
        // (<ident> <thing> ...)
        if (MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
        {
            MinimObject *ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
            if (ref)
            {
                if (MINIM_OBJ_TRANSFORMP(ref))          // transformer
                {
                    return expand_expr(env, transform_loc(env, ref, stx));
                }
                else if (MINIM_OBJ_SYNTAXP(ref))        // syntax
                {
                    EXPAND_REC(ref, minim_builtin_begin, expand_expr_begin, env, stx);
                    EXPAND_REC(ref, minim_builtin_callcc, expand_expr_1arg, env, stx);
                    EXPAND_REC(ref, minim_builtin_delay, expand_expr_1arg, env, stx);
                    EXPAND_REC(ref, minim_builtin_if, expand_expr_if, env, stx);
                    EXPAND_REC(ref, minim_builtin_lambda, expand_expr_lambda, env, stx);
                    EXPAND_REC(ref, minim_builtin_let_values, expand_expr_let_values, env, stx);
                    EXPAND_REC(ref, minim_builtin_letstar_values, expand_expr_let_values, env, stx);
                    EXPAND_REC(ref, minim_builtin_setb, expand_expr_setb, env, stx);
                    EXPAND_REC(ref, minim_builtin_syntax_case, expand_expr_syntax_case, env, stx);

                    // minim_builtin_quote
                    // minim_builtin_quasiquote
                    // minim_builtin_unquote
                    // minim_builtin_syntax
                    // minim_builtin_template
                    return stx;
                }
            }

            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                MINIM_CAR(it) = expand_expr(env, MINIM_CAR(it));
            return stx;
        }
        else
        {
            for (MinimObject *it = MINIM_STX_VAL(stx); !minim_nullp(it); it = MINIM_CDR(it))
                MINIM_CAR(it) = expand_expr(env, MINIM_CAR(it));
            return stx;
        }
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        return stx;
    }
    else
    {
        return stx;
    }
}

MinimObject *expand_definition_level(MinimEnv *env, MinimObject *stx)
{
    MinimObject *car, *ref;

    if (!MINIM_STX_PAIRP(stx))
        return expand_expr(env, stx);

    if (MINIM_STX_NULLP(stx))
        return stx;

    // (<ident> <thing> ...)
    car = MINIM_STX_CAR(stx);
    if (MINIM_STX_SYMBOLP(car))
    {
        if (minim_eqp(MINIM_STX_VAL(car), intern("%import")))
        {
            eval_top_level(env, stx, minim_builtin_import);
            return stx;
        }

        ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (ref)
        {
            if (MINIM_OBJ_TRANSFORMP(ref))          // transformer
            {
                return expand_definition_level(env, transform_loc(env, ref, stx));
            }
            else if (MINIM_OBJ_SYNTAXP(ref) &&
                     (MINIM_SYNTAX(ref) == minim_builtin_def_values ||
                      MINIM_SYNTAX(ref) == minim_builtin_def_syntaxes))
            {
                MinimObject *val = MINIM_CDR(MINIM_STX_CDR(stx));
                MINIM_CAR(val) = expand_expr(env, MINIM_CAR(val));
                return stx;
            }
        }
    }

    return expand_expr(env, stx);
}

MinimObject *expand_module_level(MinimEnv *env, MinimObject *stx)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
        return expand_expr(env, stx);

    car = MINIM_STX_CAR(stx);
    if (MINIM_OBJ_ASTP(car))
    {
        car = MINIM_STX_VAL(car);
        if (minim_eqp(car, intern("%export")))
            return stx;

        if (minim_eqp(car, intern("begin")))
        {
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                MINIM_CAR(it) = expand_module_level(env, MINIM_CAR(it));
        }
    }
    
    return expand_definition_level(env, stx);
}

MinimObject *expand_top_level(MinimEnv *env, MinimObject *stx)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
        return expand_expr(env, stx);

    car = MINIM_STX_CAR(stx);
    if (MINIM_OBJ_ASTP(car))
    {
        car = MINIM_STX_VAL(car);
        if (minim_eqp(car, intern("%module")))
        {
            MinimObject *t = MINIM_CDDR(MINIM_STX_CDR(stx));
            for (t = MINIM_STX_CDR(MINIM_CAR(t)); !minim_nullp(t); t = MINIM_CDR(t))
                MINIM_CAR(t) = expand_module_level(env, MINIM_CAR(t));   
        }
    }

    return stx;
}

void expand_minim_module(MinimEnv *env, MinimModule *module)
{
    module->body = expand_top_level(env, module->body);
}
