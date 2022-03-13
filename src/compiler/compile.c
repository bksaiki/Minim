#include "../core/minimpriv.h"

#include "compile.h"
#include "compilepriv.h"

#define COMPILE_REC(ref, fn, exp, env, stx, comp, func) \
  if (MINIM_SYNTAX(ref) == (fn)) { return exp(env, stx, comp, func); }

#define CURR_FUNC(comp)     ((comp)->funcs[(comp)->func_count - 1])

static void
debug_function(MinimEnv *env, Function *func)
{
    printf("  function %s:\n", func->name);
    if (func->pseudo) {
        for (MinimObject *it = func->pseudo; !minim_nullp(it); it = MINIM_CDR(it))
        {
            printf("   ");
            print_syntax_to_port(MINIM_CAR(it), stdout);
            printf("\n");
        }
    }
}

static MinimObject *
compile_expr(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler,
             Function *func);

static MinimObject *
compile_begin(MinimEnv *env,
              MinimObject *stx,
              Compiler *compiler,
              Function *func)
{
    if (minim_nullp(MINIM_STX_CDR(stx)))
    {
        MinimObject *instr, *sym;

        sym = minim_symbol(gensym_unique("t"));
        instr = minim_ast(
            minim_cons(minim_ast(intern("$set"), NULL),
            minim_cons(minim_ast(sym, NULL),
            minim_cons(minim_ast(intern("$void"), NULL),
            minim_null))),
            NULL
        );

        function_add_line(func, instr);
        return sym;
    }
    else
    {
        MinimObject *t = minim_null;    // stop compiler warnings
        for (MinimObject *it2 = MINIM_STX_CDR(stx); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            t = compile_expr(env, MINIM_CAR(it2), compiler, func);
        return t;
    }
}

static MinimObject *
compile_if(MinimEnv *env,
           MinimObject *stx,
           Compiler *compiler,
           Function *func)
{
    MinimObject *cond, *ift, *iff,
                *label1, *label2, *sym;

    // seek to syntax
    cond = MINIM_STX_CDR(stx);
    ift = MINIM_CDR(MINIM_STX_CDR(stx));
    iff = MINIM_CDDR(MINIM_STX_CDR(stx));

    // generate labels
    label1 = minim_symbol(gensym_unique("L"));
    label2 = minim_symbol(gensym_unique("L"));


    // begin compilation
    cond = compile_expr(env, MINIM_CAR(cond), compiler, func);
    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$goif"), NULL),
        minim_cons(minim_ast(label1, NULL),
        minim_cons(minim_ast(cond, NULL),
        minim_cons(minim_ast(intern("$false"), NULL),
        minim_null)))),
        NULL));

    ift = compile_expr(env, MINIM_CAR(ift), compiler, func);
    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$goto"), NULL),
        minim_cons(minim_ast(label2, NULL),
        minim_null)),
        NULL));

    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$label"), NULL),
        minim_cons(minim_ast(label1, NULL),
        minim_null)),
        NULL));

    iff = compile_expr(env, MINIM_CAR(iff), compiler, func);
    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$label"), NULL),
        minim_cons(minim_ast(label2, NULL),
        minim_null)),
        NULL));

    sym = minim_symbol(gensym_unique("t"));
    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$join"), NULL),
        minim_cons(minim_ast(sym, NULL),
        minim_cons(minim_ast(ift, NULL),
        minim_cons(minim_ast(iff, NULL),
        minim_null)))),
        NULL));

    return sym;
}

static void
compile_def_values(MinimEnv *env,
                   MinimObject *stx,
                   Compiler *compiler,
                   Function *func)
{
    MinimObject *body = MINIM_CADR(MINIM_STX_CDR(stx));
    compile_expr(env, body, compiler, func);
}

static MinimObject *
compile_top(MinimEnv *env,
            MinimObject *stx,
            Compiler *compiler,
            Function *func)
{
    MinimObject *instr, *sym;

    sym = minim_symbol(gensym_unique("t"));
    instr = minim_ast(
        minim_cons(minim_ast(intern("$set"), NULL),
        minim_cons(minim_ast(sym, NULL),
        minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$top"), NULL),
            minim_cons(MINIM_STX_CDR(stx),
            minim_null)),
            NULL),
        minim_null))),
        NULL
    );

    function_add_line(func, instr);
    return sym;
}

static MinimObject *
compile_load(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler,
             Function *func)
{
    MinimObject *instr, *sym;

    sym = minim_symbol(gensym_unique("t"));
    instr = minim_ast(
        minim_cons(minim_ast(intern("$set"), NULL),
        minim_cons(minim_ast(sym, NULL),
        minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$load"), NULL),
            minim_cons(stx,
            minim_null)),
            NULL),
        minim_null))),
        NULL
    );

    function_add_line(func, instr);
    return sym;
}

static MinimObject *
compile_quote(MinimEnv *env,
              MinimObject *stx,
              Compiler *compiler,
              Function *func)
              
{
    MinimObject *instr, *sym;

    sym = minim_symbol(gensym_unique("t"));
    instr = minim_ast(
        minim_cons(minim_ast(intern("$set"), NULL),
        minim_cons(minim_ast(sym, NULL),
        minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$quote"), NULL),
            minim_cons(MINIM_STX_CADR(stx),
            minim_null)),
            NULL),
        minim_null))),
        NULL
    );

    function_add_line(func, instr);
    return sym;
}

static MinimObject *
compile_lambda(MinimEnv *env,
               MinimObject *stx,
               Compiler *compiler,
               Function *func)
{
    MinimObject *args, *body, *ret, *instr;
    Function *func2;
    
    args = MINIM_STX_CADR(stx);
    body = MINIM_CADR(MINIM_STX_CDR(stx));
    
    init_function(&func2);
    compiler_add_function(compiler, func2);

    func2->name = gensym_unique("f");
    if (!MINIM_STX_SYMBOLP(args) && syntax_proper_list_len(MINIM_STX_VAL(args)) != SIZE_MAX)
    {
        func2->variary = false;
        func2->argc = minim_list_length(MINIM_STX_VAL(args));
    }

    ret = compile_expr(env, body, compiler, func2);
    instr = minim_ast(
        minim_cons(minim_ast(intern("$ret"), NULL),
        minim_cons(ret,
        minim_null)),
        NULL);
    function_add_line(func2, instr);

    debug_function(env, func2);
    return minim_symbol(func2->name);
}

static MinimObject *
compile_expr(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler,
             Function *func)
{
    if (MINIM_STX_PAIRP(stx))
    {
        // assuming (<ident> <thing> ...)
   
        // variable reference
        if (minim_eqvp(MINIM_STX_VAL(MINIM_STX_CAR(stx)), intern("%top")))
            return compile_top(env, stx, compiler, func);

        MinimObject *ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (ref && MINIM_OBJ_SYNTAXP(ref))        // syntax
        {
            COMPILE_REC(ref, minim_builtin_begin, compile_begin, env, stx, compiler, func);
            // EXPAND_REC(ref, minim_builtin_callcc, expand_expr_1arg, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_delay, expand_expr_1arg, env, stx, analysis);
            COMPILE_REC(ref, minim_builtin_if, compile_if, env, stx, compiler, func);
            COMPILE_REC(ref, minim_builtin_lambda, compile_lambda, env, stx, compiler, func);
            // EXPAND_REC(ref, minim_builtin_let_values, expand_expr_let_values, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_setb, expand_expr_setb, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_syntax_case, expand_expr_syntax_case, env, stx, analysis);

            COMPILE_REC(ref, minim_builtin_quote, compile_quote, env, stx, compiler, func);

            // minim_builtin_quasiquote
            // minim_builtin_unquote
            // minim_builtin_syntax
            // minim_builtin_template

            printf("Unsupported syntax for compilation ");
            print_syntax_to_port(stx, stdout);
            printf("\n");

            return minim_void;
        }
        else
        {
            MinimObject *it, *instr, *sym;

            sym = minim_symbol(gensym_unique("t"));
            it = minim_cons(minim_ast(intern("$eval"), NULL), minim_null);
            instr = minim_ast(
                minim_cons(minim_ast(intern("$set"), NULL),
                minim_cons(minim_ast(sym, NULL),
                minim_cons(minim_ast(it, NULL),
                minim_null))),
                NULL
            );

            for (MinimObject *it2 = MINIM_STX_VAL(stx); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            {
                MinimObject *t = compile_expr(env, MINIM_CAR(it2), compiler, func);
                MINIM_CDR(it) = minim_cons(minim_ast(t, NULL), minim_null);
                it = MINIM_CDR(it);
            }

            function_add_line(func, instr);
            return sym;
        }
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        return compile_load(env, stx, compiler, func);
    }
    else
    {
        printf("Unsupported syntax for compilation ");
        print_syntax_to_port(stx, stdout);
        printf("\n");

        return minim_void;
    }
}

static void
compile_definition_level(MinimEnv *env,
                         MinimObject *stx,
                         Compiler *compiler,
                         Function *func)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
    {
        compile_expr(env, stx, compiler, func);
        return;
    }

    if (MINIM_STX_NULLP(stx))
        return;

    // (<ident> <thing> ...)
    car = MINIM_STX_CAR(stx);
    if (MINIM_STX_SYMBOLP(car))
    {
        if (minim_eqp(MINIM_STX_VAL(car), intern("%import")) ||
            minim_eqp(MINIM_STX_VAL(car), intern("def-syntaxes")))
            return;

        if (minim_eqp(MINIM_STX_VAL(car), intern("def-values")))
        {
            compile_def_values(env, stx, compiler, func);
            return;
        }
    }

    compile_expr(env, stx, compiler, func);
}

static void
compile_module_level(MinimEnv *env,
                     MinimObject *stx,
                     Compiler *compiler,
                     Function *func)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
        compile_expr(env, stx, compiler, func);

    car = MINIM_STX_CAR(stx);
    if (MINIM_OBJ_ASTP(car))
    {
        car = MINIM_STX_VAL(car);
        if (minim_eqp(car, intern("%export")))
            return;

        if (minim_eqp(car, intern("begin")))
        {
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                compile_module_level(env, MINIM_CAR(it), compiler, func);
            return;
        }
    }
    
    compile_definition_level(env, stx, compiler, func);
}

static void
compile_top_level(MinimEnv *env,
                  MinimObject *stx,
                  Compiler *compiler,
                  Function *func)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
    {
        compile_expr(env, stx, compiler, func);
        return;
    }

    car = MINIM_STX_CAR(stx);
    if (MINIM_OBJ_ASTP(car))
    {
        car = MINIM_STX_VAL(car);
        if (minim_eqp(car, intern("%module")))
        {
            MinimObject *t = MINIM_CDDR(MINIM_STX_CDR(stx));
            for (t = MINIM_STX_CDR(MINIM_CAR(t)); !minim_nullp(t); t = MINIM_CDR(t))
                compile_module_level(env, MINIM_CAR(t), compiler, func);
        }
    }
}

// ================================ Public ================================

void compile_module(MinimEnv *env, MinimModule *module)
{
    MinimEnv *env2;
    init_env(&env2, env, NULL);

// only enabled for linux
#if defined(MINIM_LINUX)
    Compiler compiler;
    Function *func;

    compiler.funcs = NULL;
    compiler.func_count = 0;

    if (environment_variable_existsp("MINIM_LOG"))
    {
        if (module->name && module->path)
            printf("Compiling: (%s) %s\n", module->name, module->path);
        else if (module->path)
            printf("Compiling: %s\n", module->path);
        else
            printf("Compiling <unnamed>\n");
    }

    // top-level "function"
    init_function(&func);
    func->name = gensym_unique("f");
    func->variary = false;
    // this should maybe just be thrown away
    // compiler_add_function(&compiler, func);

    // translate program into flat pseudo-assembly
    compile_top_level(env2, module->body, &compiler, func);

    // printf("  top-level (%s):\n", func->name);
    // debug_function(env, func);

    if (environment_variable_existsp("MINIM_LOG"))
    {
        if (compiler.func_count > 1)
            printf(" Compiled %zu functions\n", compiler.func_count - 1);
    }
#endif
}
