#include <string.h>

#include "../common/read.h"
#include "../gc/gc.h"
#include "ast.h"
#include "builtin.h"
#include "eval.h"
#include "error.h"
#include "lambda.h"
#include "number.h"

MinimObject *minim_builtin_def(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *sym, *val;

    unsyntax_ast(env, args[0]->u.ptrs.p1, &sym);
    if (argc == 2)
    {
        eval_ast_no_check(env, args[1]->u.ptrs.p1, &val);
    }
    else
    {
        SyntaxNode *ast = args[0]->u.ptrs.p1;
        MinimLambda *lam;
        
        val = minim_builtin_lambda(env, &args[1], argc - 1);
        
        lam = val->u.ptrs.p1;
        copy_syntax_loc(&lam->loc, ast->loc);
    }
    
    if (!MINIM_OBJ_THROWNP(val))
    {
        env_intern_sym(env, sym->u.str.str, val);
        init_minim_object(&res, MINIM_OBJ_VOID);
    }
    else
    {
        res = val;
    }

    return res;
}

MinimObject *minim_builtin_quote(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    unsyntax_ast_rec(env, args[0]->u.ptrs.p1, &res);
    return res;
}

MinimObject *minim_builtin_setb(MinimEnv *env, MinimObject **args, size_t argc)
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

MinimObject *minim_builtin_symbolp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_SYMBOLP(args[0]));   
    return res;
}

MinimObject *minim_builtin_equalp(MinimEnv *env, MinimObject **args, size_t argc)
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

MinimObject *minim_builtin_version(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    char *str;

    str = GC_alloc_atomic((strlen(MINIM_VERSION_STR) + 1) * sizeof(char));
    strcpy(str, MINIM_VERSION_STR);
    init_minim_object(&res, MINIM_OBJ_STRING, str);

    return res;
}

MinimObject *minim_builtin_symbol_count(MinimEnv *env, MinimObject **args, size_t argc)
{
    return int_to_minim_number(env->table->size);
}

MinimObject *minim_builtin_exit(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_EXIT, 0);
    return res;
}
