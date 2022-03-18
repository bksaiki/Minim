#include "minimpriv.h"

//
//  Builtins
//

MinimObject *minim_builtin_if(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *cond;
    uint8_t prev_flags;
    
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    cond = eval_ast_no_check(env, args[0]);
    env->flags = prev_flags;
    return eval_ast_no_check(env, (coerce_into_bool(cond) ? args[1] : args[2]));
}

MinimObject *minim_builtin_let_values(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *it;
    MinimEnv *env2;
    size_t bindc;
    uint8_t prev_flags;
    
    // setup up new scope
    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    env2 = init_env(env);

    bindc = syntax_list_len(args[0]);
    it = MINIM_STX_VAL(args[0]);

    // Bind names and values
    for (size_t i = 0; i < bindc; ++i)
    {   
        MinimObject *binding, *names, *val;
        size_t namec;

        binding = MINIM_CAR(it);
        names = MINIM_STX_CAR(binding);
        namec = syntax_list_len(names);
        val = eval_ast_no_check(env, MINIM_STX_CADR(binding));

        if (!MINIM_OBJ_VALUESP(val))
        {
            if (namec != 1)
                THROW(env, minim_values_arity_error("let-values", namec, 1, names));
            
            env_intern_sym(env2, MINIM_STX_SYMBOL(MINIM_STX_CAR(names)), val);
            if (MINIM_OBJ_CLOSUREP(val))
            {
                env_intern_sym(MINIM_CLOSURE(val)->env,
                               MINIM_STX_SYMBOL(MINIM_STX_CAR(names)),
                               val);
            }
        }
        else
        {
            MinimObject *it2;

            if (MINIM_VALUES_SIZE(val) != namec)
                THROW(env, minim_values_arity_error("let-values", namec, MINIM_VALUES_SIZE(val), names));

            it2 = MINIM_STX_VAL(names);
            for (size_t i = 0; i < namec; ++i)
            {
                env_intern_sym(env2, MINIM_STX_SYMBOL(MINIM_CAR(it2)), MINIM_VALUES_REF(val, i));
                if (MINIM_OBJ_CLOSUREP(MINIM_VALUES_REF(val, i)))
                {
                    env_intern_sym(MINIM_CLOSURE(MINIM_VALUES_REF(val, i))->env,
                                   MINIM_STX_SYMBOL(MINIM_CAR(it2)),
                                   MINIM_VALUES_REF(val, i));
                }
            
                it2 = MINIM_CDR(it2);
            }
        }

        it = MINIM_CDR(it);
    }

    // Evaluate body
    env->flags = prev_flags; 
    return minim_builtin_begin(env2, argc - 1, &args[1]);
}

MinimObject *minim_builtin_begin(MinimEnv *env, size_t argc, MinimObject **args)
{
    uint8_t prev_flags;

    if (argc == 0)
        return minim_void;

    prev_flags = env->flags;
    env->flags &= ~MINIM_ENV_TAIL_CALLABLE;
    for (size_t i = 0; i < argc - 1; ++i)
        eval_ast_no_check(env, args[i]);

    env->flags = prev_flags;
    return eval_ast_no_check(env, args[argc - 1]);
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

    proc = eval_ast_no_check(env, args[0]);
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
