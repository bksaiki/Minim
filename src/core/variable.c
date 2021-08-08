#include <string.h>

#include "../common/path.h"
#include "../gc/gc.h"
#include "ast.h"
#include "builtin.h"
#include "eval.h"
#include "error.h"
#include "lambda.h"
#include "number.h"

MinimObject *minim_builtin_def(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *sym, *val;
    MinimLambda *lam;
    SyntaxNode *ast;

    unsyntax_ast(env, MINIM_AST(args[0]), &sym);
    if (argc == 2)
    {
        eval_ast_no_check(env, MINIM_AST(args[1]), &val);
        
    }
    else
    {
        ast = MINIM_AST(args[0]);
        val = minim_builtin_lambda(env, argc - 1, &args[1]);

        lam = MINIM_CLOSURE(val);
        copy_syntax_loc(&lam->loc, ast->loc);
        env_intern_sym(lam->env, MINIM_STRING(sym), val);
    }
    
    if (MINIM_OBJ_THROWNP(val))
        return val;

    env_intern_sym(env, MINIM_STRING(sym), val);
    return minim_void();
}

MinimObject *minim_builtin_def_values(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *val;

    eval_ast_no_check(env, MINIM_AST(args[1]), &val);
    if (MINIM_OBJ_THROWNP(val))
        return val;
    
    if (!MINIM_OBJ_VALUESP(val))
    {
        if (MINIM_AST(args[0])->childc != 1)
        {
            res = minim_values_arity_error("def-values", MINIM_AST(args[0])->childc,
                                           1, MINIM_AST(args[0]));
            return res;
        } 
        
        env_intern_sym(env, MINIM_AST(args[0])->children[0]->sym, val);
        if (MINIM_OBJ_CLOSUREP(val))
            env_intern_sym(MINIM_CLOSURE(val)->env,
                           MINIM_AST(args[0])->children[0]->sym,
                           val);
    }
    else
    {
        if (MINIM_VALUES_SIZE(val) != MINIM_AST(args[0])->childc)
        {
            res = minim_values_arity_error("def-values",
                                           MINIM_AST(args[0])->childc,
                                           MINIM_VALUES_SIZE(val),
                                           MINIM_AST(args[0]));
            return res;
        }

        for (size_t i = 0; i < MINIM_AST(args[0])->childc; ++i)
        {
            env_intern_sym(env, MINIM_AST(args[0])->children[i]->sym, MINIM_VALUES(val)[i]);
            if (MINIM_OBJ_CLOSUREP(MINIM_VALUES(val)[i]))
                env_intern_sym(MINIM_CLOSURE(MINIM_VALUES(val)[i])->env,
                               MINIM_AST(args[0])->children[i]->sym,
                               val);
        }
    }

    return minim_void();
}

MinimObject *minim_builtin_quote(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    unsyntax_ast_rec(env, MINIM_AST(args[0]), &res);
    return res;
}

MinimObject *minim_builtin_quasiquote(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    unsyntax_ast_rec2(env, MINIM_AST(args[0]), &res);
    return res;
}

MinimObject *minim_builtin_unquote(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    eval_ast_no_check(env, MINIM_AST(args[0]), &res);
    return res;
}

MinimObject *minim_builtin_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *sym, *val;

    unsyntax_ast(env, MINIM_AST(args[0]), &sym);
    eval_ast_no_check(env, MINIM_AST(args[1]), &val);
    if (MINIM_OBJ_THROWNP(val))
        return val;

    if (env_set_sym(env, MINIM_STRING(sym), val))
    {
        env_set_sym(env, MINIM_STRING(sym), val);
        return minim_void();
    }
    else
    {
        Buffer *bf;
        PrintParams pp;

        init_buffer(&bf);
        set_default_print_params(&pp);
        pp.quote = true;
        print_to_buffer(bf, sym, env, &pp);
        return minim_error("not a variable", bf->data);
    }
}

MinimObject *minim_builtin_symbolp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_bool(MINIM_OBJ_SYMBOLP(args[0]));
}

MinimObject *minim_builtin_equalp(MinimEnv *env, size_t argc, MinimObject **args)
{
    for (size_t i = 1; i < argc; ++i)
    {
        if (!minim_equalp(args[i - 1], args[i]))
            return minim_bool(0);
    }

    return minim_bool(1);
}

MinimObject *minim_builtin_version(MinimEnv *env, size_t argc, MinimObject **args)
{
    char *str = GC_alloc_atomic((strlen(MINIM_VERSION_STR) + 1) * sizeof(char));
    strcpy(str, MINIM_VERSION_STR);
    return minim_string(str);
}

MinimObject *minim_builtin_void(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_void();
}


MinimObject *minim_builtin_symbol_count(MinimEnv *env, size_t argc, MinimObject **args)
{
    return int_to_minim_number(env_symbol_count(env));
}

static MinimEnv *env_for_print = NULL;

static void print_symbol_entry(const char *sym, MinimObject *obj)
{
    PrintParams pp;

    set_default_print_params(&pp);
    printf("(%s . ", sym);
    print_minim_object(obj, env_for_print, &pp);
    printf(")\n");
}

static void env_dump_symbols(MinimEnv *env)
{
    for (MinimEnv *it = env; it; it = it->parent)
    {
        env_for_print = it;
        minim_symbol_table_for_each(it->table, print_symbol_entry);
    }
}

MinimObject *minim_builtin_dump_symbols(MinimEnv *env, size_t argc, MinimObject **args)
{
    env_dump_symbols(env);
    return minim_void();
}

MinimObject *minim_builtin_exit(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_exit(0);
}
