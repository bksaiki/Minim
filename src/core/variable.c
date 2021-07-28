#include <string.h>

#include "../common/read.h"
#include "../gc/gc.h"
#include "ast.h"
#include "builtin.h"
#include "eval.h"
#include "error.h"
#include "lambda.h"
#include "number.h"

MinimObject *minim_builtin_def(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *sym, *val;
    MinimLambda *lam;
    SyntaxNode *ast;

    unsyntax_ast(env, args[0]->u.ptrs.p1, &sym);
    if (argc == 2)
    {
        eval_ast_no_check(env, MINIM_DATA(args[1]), &val);
        
    }
    else
    {
        ast = MINIM_DATA(args[0]);
        val = minim_builtin_lambda(env, argc - 1, &args[1]);

        lam = MINIM_DATA(val);
        copy_syntax_loc(&lam->loc, ast->loc);
        env_intern_sym(lam->env, MINIM_STRING(sym), val);
    }
    
    if (!MINIM_OBJ_THROWNP(val))
    {
        init_minim_object(&res, MINIM_OBJ_VOID);
        env_intern_sym(env, MINIM_STRING(sym), val);
    }
    else
    {
        res = val;
    }

    return res;
}

MinimObject *minim_builtin_quote(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    unsyntax_ast_rec(env, MINIM_DATA(args[0]), &res);
    return res;
}

MinimObject *minim_builtin_quasiquote(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    unsyntax_ast_rec2(env, MINIM_DATA(args[0]), &res);
    return res;
}

MinimObject *minim_builtin_unquote(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    eval_ast_no_check(env, MINIM_DATA(args[0]), &res);
    return res;
}

MinimObject *minim_builtin_setb(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *sym, *val;

    unsyntax_ast(env, args[0]->u.ptrs.p1, &sym);
    eval_ast_no_check(env, args[1]->u.ptrs.p1, &val);
    if (MINIM_OBJ_THROWNP(val))
        return val;

    if (env_set_sym(env, MINIM_STRING(sym), val))
    {
        env_set_sym(env, sym->u.str.str, val);
        init_minim_object(&res, MINIM_OBJ_VOID);
    }
    else
    {
        Buffer *bf;
        PrintParams pp;

        init_buffer(&bf);
        set_default_print_params(&pp);
        pp.quote = true;
        print_to_buffer(bf, sym, env, &pp);
        res = minim_error("not a variable", bf->data);
    }

    return res;
}

MinimObject *minim_builtin_symbolp(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_SYMBOLP(args[0]));   
    return res;
}

MinimObject *minim_builtin_equalp(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    for (size_t i = 1; i < argc; ++i)
    {
        if (!minim_equalp(args[i - 1], args[i]))
        {
            init_minim_object(&res, MINIM_OBJ_BOOL, 0);
            return res;
        }
    }

    init_minim_object(&res, MINIM_OBJ_BOOL, 1);
    return res;
}

MinimObject *minim_builtin_version(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    char *str;

    str = GC_alloc_atomic((strlen(MINIM_VERSION_STR) + 1) * sizeof(char));
    strcpy(str, MINIM_VERSION_STR);
    init_minim_object(&res, MINIM_OBJ_STRING, str);

    return res;
}

MinimObject *minim_builtin_void(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}


MinimObject *minim_builtin_symbol_count(MinimEnv *env, size_t argc, MinimObject **args)
{
    return int_to_minim_number(env->table->size);
}

MinimObject *minim_builtin_exit(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_EXIT, 0);
    return res;
}
