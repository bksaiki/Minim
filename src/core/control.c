#include <setjmp.h>
#include <string.h>

#include "../gc/gc.h"
#include "arity.h"
#include "ast.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "jmp.h"
#include "lambda.h"
#include "list.h"
#include "tail_call.h"

//
//  Builtins
//

MinimObject *minim_builtin_if(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *cond;

    cond = eval_ast_no_check(env, MINIM_AST(args[0]));
    return eval_ast_no_check(env, (coerce_into_bool(cond) ? MINIM_AST(args[1]) : MINIM_AST(args[2])));
}

MinimObject *minim_builtin_let_values(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val;
    SyntaxNode *binding, *names;
    MinimEnv *env2;
    uint8_t prev_flags;
    
    // Bind names and values
    init_env(&env2, env, NULL);
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    for (size_t i = 0; i < MINIM_AST(args[0])->childc; ++i)
    {
        binding = MINIM_AST(args[0])->children[i];
        val = eval_ast_no_check(env, binding->children[1]);
        names = binding->children[0];
        if (!MINIM_OBJ_VALUESP(val))
        {
            if (names->childc != 1)
                THROW(env, minim_values_arity_error("let-values", names->childc, 1, names));
            
            env_intern_sym(env2, names->children[0]->sym, val);
            if (MINIM_OBJ_CLOSUREP(val))
                env_intern_sym(MINIM_CLOSURE(val)->env, names->children[0]->sym, val);
        }
        else
        {
            if (MINIM_VALUES_SIZE(val) != names->childc)
                THROW(env, minim_values_arity_error("let-values", names->childc, MINIM_VALUES_SIZE(val), names));

            for (size_t i = 0; i < names->childc; ++i)
            {
                env_intern_sym(env2, names->children[i]->sym, MINIM_VALUES_REF(val, i));
                if (MINIM_OBJ_CLOSUREP(MINIM_VALUES_REF(val, i)))
                    env_intern_sym(MINIM_CLOSURE(MINIM_VALUES_REF(val, i))->env,
                                   names->children[0]->sym,
                                   MINIM_VALUES_REF(val, i));
            }
        }
    }

    // Evaluate body
    env->flags = prev_flags; 
    return minim_builtin_begin(env2, argc - 1, &args[1]);
}

MinimObject *minim_builtin_letstar_values(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val;
    SyntaxNode *binding, *names;
    MinimEnv *env2;
    
    // Bind names and values
    init_env(&env2, env, NULL);
    for (size_t i = 0; i < MINIM_AST(args[0])->childc; ++i)
    {
        binding = MINIM_AST(args[0])->children[i];
        names = binding->children[0];
        val = eval_ast_no_check(env2, binding->children[1]);
        if (!MINIM_OBJ_VALUESP(val))
        {
            if (names->childc != 1)
                THROW(env, minim_values_arity_error("let*-values", names->childc, 1, names));
            
            env_intern_sym(env2, names->children[0]->sym, val);
            if (MINIM_OBJ_CLOSUREP(val))
                env_intern_sym(MINIM_CLOSURE(val)->env, names->children[0]->sym, val);
        }
        else
        {
            if (MINIM_VALUES_SIZE(val) != names->childc)
                THROW(env, minim_values_arity_error("let*-values", names->childc, MINIM_VALUES_SIZE(val), names));

            for (size_t i = 0; i < names->childc; ++i)
            {
                env_intern_sym(env2, names->children[i]->sym, MINIM_VALUES_REF(val, i));
                if (MINIM_OBJ_CLOSUREP(MINIM_VALUES_REF(val, i)))
                    env_intern_sym(MINIM_CLOSURE(MINIM_VALUES_REF(val, i))->env,
                                   names->children[0]->sym,
                                   MINIM_VALUES_REF(val, i));
            }
        }
    }

    // Evaluate body
    return minim_builtin_begin(env2, argc - 1, &args[1]);
}

MinimObject *minim_builtin_begin(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val;

    if (argc == 0)
        return minim_void;

    for (size_t i = 0; i < argc; ++i)
        val = eval_ast_no_check(env, MINIM_AST(args[i]));
    return val;
}

MinimObject *minim_builtin_values(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_values(argc, args);
}

MinimObject *minim_builtin_callcc(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimArity arity;
    MinimObject *proc, *cont;
    jmp_buf *jmp;

    proc = eval_ast_no_check(env, MINIM_AST(args[0]));
    if (!MINIM_OBJ_CLOSUREP(proc))
        THROW(env, minim_argument_error("procedure of 1 argument", "call/cc", 0, proc));

    minim_get_lambda_arity(MINIM_CLOSURE(proc), &arity);
    if (arity.low != 1 || arity.high != 1)
        THROW(env, minim_argument_error("procedure of 1 argument", "call/cc", 0, proc));

    jmp = GC_alloc_atomic(sizeof(jmp_buf));
    cont = minim_jmp(jmp, NULL);
    return (setjmp(*jmp) == 0) ?
           eval_lambda(MINIM_CLOSURE(proc), env, 1, &cont) :
           MINIM_JUMP_VAL(cont);
}

MinimObject *minim_builtin_exit(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (minim_exit_handler != NULL)     // no return
        minim_long_jump(minim_exit_handler, env, argc, args);

    printf("exit handler not set up\n");
    return NULL;    // panic
}
