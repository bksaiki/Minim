#include "../core/minimpriv.h"

#include "compile.h"
#include "compilepriv.h"

#define COMPILE_REC(ref, fn, exp, env, stx, comp) \
  if (MINIM_SYNTAX(ref) == (fn)) { return exp(env, stx, comp); }

static MinimObject *
compile_expr(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler);

static MinimObject *
compile_begin(MinimEnv *env,
              MinimObject *stx,
              Compiler *compiler)
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

        function_add_line(compiler->curr_func, instr);
        return sym;
    }
    else
    {
        MinimObject *t = minim_null;    // stop compiler warnings
        for (MinimObject *it2 = MINIM_STX_CDR(stx); !minim_nullp(it2); it2 = MINIM_CDR(it2))
            t = compile_expr(env, MINIM_CAR(it2), compiler);
        return t;
    }
}

static MinimObject *
compile_if(MinimEnv *env,
           MinimObject *stx,
           Compiler *compiler)
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
    cond = compile_expr(env, MINIM_CAR(cond), compiler);
    function_add_line(compiler->curr_func, minim_ast(
        minim_cons(minim_ast(intern("$gofalse"), NULL),
        minim_cons(minim_ast(label1, NULL),
        minim_cons(minim_ast(cond, NULL),
        minim_null))),
        NULL));

    ift = compile_expr(env, MINIM_CAR(ift), compiler);
    function_add_line(compiler->curr_func, minim_ast(
        minim_cons(minim_ast(intern("$goto"), NULL),
        minim_cons(minim_ast(label2, NULL),
        minim_null)),
        NULL));

    function_add_line(compiler->curr_func, minim_ast(
        minim_cons(minim_ast(intern("$label"), NULL),
        minim_cons(minim_ast(label1, NULL),
        minim_null)),
        NULL));

    iff = compile_expr(env, MINIM_CAR(iff), compiler);
    function_add_line(compiler->curr_func, minim_ast(
        minim_cons(minim_ast(intern("$label"), NULL),
        minim_cons(minim_ast(label2, NULL),
        minim_null)),
        NULL));

    sym = minim_symbol(gensym_unique("t"));
    function_add_line(compiler->curr_func, minim_ast(
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
                   Compiler *compiler)
{
    MinimObject *body;
    body = MINIM_CADR(MINIM_STX_CDR(stx));
    compile_expr(env, body, compiler);
}

static MinimObject *
compile_top(MinimEnv *env,
            MinimObject *stx,
            Compiler *compiler)
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

    function_add_line(compiler->curr_func, instr);
    return sym;
}

static MinimObject *
compile_load(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler)
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

    function_add_line(compiler->curr_func, instr);
    return sym;
}

static MinimObject *
compile_interpret(MinimEnv *env,
                  MinimObject *stx,
                  Compiler *compiler)
{
    MinimObject *instr, *sym;

    sym = minim_symbol(gensym_unique("t"));
    instr = minim_ast(
        minim_cons(minim_ast(intern("$set"), NULL),
        minim_cons(minim_ast(sym, NULL),
        minim_cons(minim_ast(
            minim_cons(minim_ast(intern("$interpret"), NULL),
            minim_cons(stx, minim_null)),
            NULL),
        minim_null))),
        NULL
    );

    function_add_line(compiler->curr_func, instr);
    return sym;
}

static MinimObject *
compile_quote(MinimEnv *env,
              MinimObject *stx,
              Compiler *compiler)
              
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

    function_add_line(compiler->curr_func, instr);
    return sym;
}

static MinimObject *
compile_let_values(MinimEnv *env,
                   MinimObject *stx,
                   Compiler *compiler)
{
    MinimSymbolTable *old_table;
    MinimObject *ids, *body, *ret;

    ids = MINIM_STX_VAL(MINIM_CAR(MINIM_STX_CDR(stx)));
    body = MINIM_CDR(MINIM_STX_CDR(stx));

    // save old table
    old_table = compiler->table;

    // push a new environment in the compiled code
    function_add_line(compiler->curr_func, minim_ast(
        minim_cons(minim_ast(intern("$push-env"), NULL),
        minim_null),
        NULL));

    // compile the values
    init_minim_symbol_table(&compiler->table);
    for (MinimObject *it = ids; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *sym, *var, *val;
        size_t idx;
        
        var = MINIM_STX_CAR(MINIM_CAR(it));
        val = MINIM_STX_CDR(MINIM_CAR(it));
        val = compile_expr(env, MINIM_CAR(val), compiler);

        if (syntax_proper_list_len(var) == 1)
        {
            var = MINIM_STX_VAL(var);
            sym = minim_symbol(gensym_unique("t"));

            // assign value to temporary
            function_add_line(compiler->curr_func, minim_ast(
                minim_cons(minim_ast(intern("$set"), NULL),
                minim_cons(minim_ast(sym, NULL),
                minim_cons(minim_ast(val, NULL),
                minim_null))),
                NULL));

            // bind temporary to symbol
            minim_symbol_table_add(compiler->table, MINIM_STX_SYMBOL(var), sym);
            function_add_line(compiler->curr_func, minim_ast(
                minim_cons(minim_ast(intern("$bind"), NULL),
                minim_cons(MINIM_CAR(var),
                minim_cons(minim_ast(sym, NULL),
                minim_null))),
                NULL));
        }
        else
        {
            var = MINIM_STX_VAL(var);
            idx = 0;
            while (!minim_nullp(var))
            {
                // assign value to temporary
                sym = minim_symbol(gensym_unique("t"));
                function_add_line(compiler->curr_func, minim_ast(
                    minim_cons(minim_ast(intern("$set"), NULL),
                    minim_cons(minim_ast(sym, NULL),
                    minim_cons(minim_ast(
                        minim_cons(minim_ast(intern("$values"), NULL),
                        minim_cons(minim_ast(val, NULL),
                        minim_cons(minim_ast(uint_to_minim_number(idx), NULL),
                        minim_null))),
                        NULL),
                    minim_null))),
                    NULL));

                // bind temporary to symbol
                minim_symbol_table_add(compiler->table, MINIM_STX_SYMBOL(var), sym);
                function_add_line(compiler->curr_func, minim_ast(
                    minim_cons(minim_ast(intern("$bind"), NULL),
                    minim_cons(MINIM_CAR(var),
                    minim_cons(minim_ast(sym, NULL),
                    minim_null))),
                    NULL));

                var = MINIM_CDR(var);
                ++idx;
            }
        }
    }

    // compile the body expressions
    ret = minim_null;   // (avoid compilation errors)
    for (MinimObject *it = body; !minim_nullp(it); it = MINIM_CDR(it))
        ret = compile_expr(env, MINIM_CAR(it), compiler);

    // restore and return
    compiler->table = old_table;
    return ret;
}

static MinimObject *
compile_lambda(MinimEnv *env,
               MinimObject *stx,
               Compiler *compiler)
{
    MinimObject *args, *body, *ret;
    Function *old_func, *func;
    MinimSymbolTable *old_table;
    
    args = MINIM_STX_CADR(stx);
    body = MINIM_CADR(MINIM_STX_CDR(stx));
    
    init_function(&func);
    compiler_add_function(compiler, func);

    func->name = gensym_unique("f");
    if (syntax_proper_list_len(args) != SIZE_MAX)
    {
        func->variary = false;
        func->argc = minim_list_length(MINIM_STX_VAL(args));
    }

    // save old values
    old_func = compiler->curr_func;
    old_table = compiler->table;
    
    // compile the arguments
    init_minim_symbol_table(&compiler->table);
    if (!minim_nullp(args))
    {
        MinimObject *it = MINIM_STX_VAL(args);
        size_t i = 0;

        while (MINIM_OBJ_PAIRP(it) && !minim_nullp(it))
        {
            MinimObject *sym = minim_symbol(gensym_unique("a"));
            function_add_line(func, minim_ast(
                minim_cons(minim_ast(intern("$arg"), NULL),
                minim_cons(minim_ast(sym, NULL),
                minim_cons(minim_ast(uint_to_minim_number(i), NULL),
                minim_null))),
                NULL));

                minim_symbol_table_add(compiler->table, MINIM_STX_SYMBOL(MINIM_CAR(it)), sym);
             it = MINIM_CDR(it);
             ++i;
        }

        if (!minim_nullp(it)) {
            MinimObject *sym = minim_symbol(gensym_unique("a"));
            function_add_line(func, minim_ast(
                minim_cons(minim_ast(intern("$arg"), NULL),
                minim_cons(minim_ast(sym, NULL),
                minim_cons(minim_ast(uint_to_minim_number(i), NULL),
                minim_null))),
                NULL));

            minim_symbol_table_add(compiler->table, MINIM_STX_SYMBOL(it), sym);
        }

    }

    // compile the body
    compiler->curr_func = func;
    ret = compile_expr(env, body, compiler);

    // restore and add return
    compiler->curr_func = old_func;
    compiler->table = old_table;
    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$ret"), NULL),
        minim_cons(ret,
        minim_null)),
        NULL));


    debug_function(env, func);
    return minim_symbol(func->name);
}

static MinimObject *
compile_expr(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler)
{
    if (MINIM_STX_PAIRP(stx))
    {
        // assuming (<ident> <thing> ...)
   
        // variable reference
        if (minim_eqvp(MINIM_STX_VAL(MINIM_STX_CAR(stx)), intern("%top")))
            return compile_top(env, stx, compiler);

        MinimObject *ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (ref && MINIM_OBJ_SYNTAXP(ref))        // syntax
        {
            COMPILE_REC(ref, minim_builtin_begin, compile_begin, env, stx, compiler);
            // EXPAND_REC(ref, minim_builtin_callcc, expand_expr_1arg, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_delay, expand_expr_1arg, env, stx, analysis);
            COMPILE_REC(ref, minim_builtin_if, compile_if, env, stx, compiler);
            COMPILE_REC(ref, minim_builtin_lambda, compile_lambda, env, stx, compiler);
            COMPILE_REC(ref, minim_builtin_let_values, compile_let_values, env, stx, compiler);
            // EXPAND_REC(ref, minim_builtin_setb, expand_expr_setb, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_syntax_case, expand_expr_syntax_case, env, stx, analysis);

            COMPILE_REC(ref, minim_builtin_quote, compile_quote, env, stx, compiler);

            // minim_builtin_quasiquote
            // minim_builtin_unquote
            // minim_builtin_syntax
            // minim_builtin_template

            return compile_interpret(env, stx, compiler);
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
                MinimObject *t = compile_expr(env, MINIM_CAR(it2), compiler);
                MINIM_CDR(it) = minim_cons(minim_ast(t, NULL), minim_null);
                it = MINIM_CDR(it);
            }

            function_add_line(compiler->curr_func, instr);
            return sym;
        }
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        MinimObject *ref = minim_symbol_table_get(compiler->table, MINIM_STX_SYMBOL(stx));
        return (ref ? ref : compile_load(env, stx, compiler));
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
                         Compiler *compiler)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
    {
        compile_expr(env, stx, compiler);
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
            compile_def_values(env, stx, compiler);
            return;
        }
    }

    compile_expr(env, stx, compiler);
}

static void
compile_module_level(MinimEnv *env,
                     MinimObject *stx,
                     Compiler *compiler)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
        compile_expr(env, stx, compiler);

    car = MINIM_STX_CAR(stx);
    if (MINIM_OBJ_ASTP(car))
    {
        car = MINIM_STX_VAL(car);
        if (minim_eqp(car, intern("%export")))
            return;

        if (minim_eqp(car, intern("begin")))
        {
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                compile_module_level(env, MINIM_CAR(it), compiler);
            return;
        }
    }
    
    compile_definition_level(env, stx, compiler);
}

static void
compile_top_level(MinimEnv *env,
                  MinimObject *stx,
                  Compiler *compiler)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
    {
        compile_expr(env, stx, compiler);
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
                compile_module_level(env, MINIM_CAR(t), compiler);
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

    if (environment_variable_existsp("MINIM_LOG"))
    {
        if (module->name && module->path)
            printf("Compiling: (%s) %s\n", module->name, module->path);
        else if (module->path)
            printf("Compiling: %s\n", module->path);
        else
            printf("Compiling <unnamed>\n");
    }

    // intialize the compiler
    compiler.funcs = NULL;
    compiler.curr_func = NULL;
    compiler.func_count = 0;
    init_minim_symbol_table(&compiler.table);

    // create a top-level "function"
    init_function(&func);
    func->name = (module->path ? module->path : module->name);
    func->variary = false;
    compiler.curr_func = func;

    // translate program into flat pseudo-assembly
    compile_top_level(env2, module->body, &compiler);
    // debug_function(env, func);

    if (environment_variable_existsp("MINIM_LOG"))
        printf(" Compiled %zu functions\n", compiler.func_count);

    // Optimization passes

    if (environment_variable_existsp("MINIM_LOG"))
        printf(" Optimizing...\n");

    size_t unopt_expr_count = 0, opt_expr_count = 0;
    for (size_t i = 0; i < compiler.func_count; i++) {
        compiler.curr_func = compiler.funcs[i];
        unopt_expr_count += minim_list_length(compiler.curr_func->pseudo);
        function_optimize(env, compiler.funcs[i]);
        opt_expr_count += minim_list_length(compiler.curr_func->pseudo);
    }

    if (environment_variable_existsp("MINIM_LOG"))
        printf(" Shrunk %zu -> %zu\n", unopt_expr_count, opt_expr_count);

#endif
}
