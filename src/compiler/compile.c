#include "../core/minimpriv.h"

#include "compile.h"
#include "compilepriv.h"
#include "jit.h"

#define COMPILE_REC(ref, fn, exp, env, stx, comp) \
  if (MINIM_SYNTAX(ref) == (fn)) { return exp(env, stx, comp); }

static MinimObject *
translate_expr(MinimEnv *env,
               MinimObject *stx,
               Compiler *compiler);

static MinimObject *
translate_begin(MinimEnv *env,
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
            t = translate_expr(env, MINIM_CAR(it2), compiler);
        return t;
    }
}

static MinimObject *
translate_if(MinimEnv *env,
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
    cond = translate_expr(env, MINIM_CAR(cond), compiler);
    function_add_line(compiler->curr_func, minim_ast(
        minim_cons(minim_ast(intern("$gofalse"), NULL),
        minim_cons(minim_ast(label1, NULL),
        minim_cons(minim_ast(cond, NULL),
        minim_null))),
        NULL));

    ift = translate_expr(env, MINIM_CAR(ift), compiler);
    if (is_argument_location(ift))
    {
        sym = minim_symbol(gensym_unique("t"));
        function_add_line(compiler->curr_func, minim_ast(
            minim_cons(minim_ast(intern("$set"), NULL),
            minim_cons(minim_ast(sym, NULL),
            minim_cons(minim_ast(ift, NULL),
            minim_null))),
            NULL));
        ift = sym;
    }

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

    iff = translate_expr(env, MINIM_CAR(iff), compiler);
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
translate_def_values(MinimEnv *env,
                     MinimObject *stx,
                     Compiler *compiler)
{
    MinimObject *body;
    body = MINIM_CADR(MINIM_STX_CDR(stx));
    translate_expr(env, body, compiler);
}

static MinimObject *
translate_top(MinimEnv *env,
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
translate_load(MinimEnv *env,
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
translate_interpret(MinimEnv *env,
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
translate_quote(MinimEnv *env,
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
translate_let_values(MinimEnv *env,
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
        val = translate_expr(env, MINIM_CAR(val), compiler);

        if (syntax_proper_list_len(var) == 1)
        {
            var = MINIM_CAR(MINIM_STX_VAL(var));
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
                minim_cons(minim_ast(MINIM_CAR(var), NULL),
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
                minim_symbol_table_add(compiler->table, MINIM_STX_SYMBOL(MINIM_CAR(var)), sym);
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
        ret = translate_expr(env, MINIM_CAR(it), compiler);

    // restore the old environment
    function_add_line(compiler->curr_func, minim_ast(
        minim_cons(minim_ast(intern("$pop-env"), NULL),
        minim_null),
        NULL));

    // restore and return
    compiler->table = old_table;
    return ret;
}

static void
add_function_header(Function *func)
{
    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$push-env"), NULL),
        minim_null),
        NULL));
}

static void
add_function_footer(Function *func, MinimObject *ret_sym)
{
    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$pop-env"), NULL),
        minim_null),
        NULL));

    function_add_line(func, minim_ast(
        minim_cons(minim_ast(intern("$return"), NULL),
        minim_cons(minim_ast(ret_sym, NULL),
        minim_null)),
        NULL));
}

static MinimObject *
translate_lambda(MinimEnv *env,
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
        func->argc = minim_list_length(MINIM_STX_VAL(args));

    // save old values
    old_func = compiler->curr_func;
    old_table = compiler->table;

    // push a new environment in the compiled code
    add_function_header(func);
    
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
            MinimObject *sym;
            char *name;
            
            sym = minim_symbol(gensym_unique("a"));
            function_add_line(func, minim_ast(
                minim_cons(minim_ast(intern("$arg"), NULL),
                minim_cons(minim_ast(sym, NULL),
                minim_cons(minim_ast(uint_to_minim_number(i), NULL),
                minim_null))),
                NULL));
            
            name = (MINIM_OBJ_ASTP(it) ? MINIM_STX_SYMBOL(it) : MINIM_SYMBOL(it));
            minim_symbol_table_add(compiler->table, name, sym);
        }

    }

    // compile the body
    compiler->curr_func = func;
    ret = translate_expr(env, body, compiler);
    compiler->curr_func = old_func;
    compiler->table = old_table;

    // pop environment and return
    add_function_footer(func, ret);
    func->ret_sym = ret;

    // debug_function(env, func);
    return minim_cons(minim_ast(intern("$func"), NULL),
        minim_cons(minim_ast(minim_symbol(func->name), NULL),
        minim_null));
}

static MinimObject *
translate_expr(MinimEnv *env,
               MinimObject *stx,
               Compiler *compiler)
{
    if (MINIM_STX_PAIRP(stx))
    {
        // assuming (<ident> <thing> ...)
   
        // variable reference
        if (minim_eqvp(MINIM_STX_VAL(MINIM_STX_CAR(stx)), intern("%top")))
            return translate_top(env, stx, compiler);

        MinimObject *ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (ref && MINIM_OBJ_SYNTAXP(ref))        // syntax
        {
            if (MINIM_SYNTAX(ref) == minim_builtin_lambda)
            {
                MinimObject *sym, *val;

                sym = minim_symbol(gensym_unique("t"));
                val = translate_lambda(env, stx, compiler);
                function_add_line(compiler->curr_func, minim_ast(
                    minim_cons(minim_ast(intern("$set"), NULL),
                    minim_cons(minim_ast(sym, NULL),
                    minim_cons(minim_ast(val, NULL),
                    minim_null))),
                    NULL
                ));

                return sym;
            }
            else
            {
                COMPILE_REC(ref, minim_builtin_begin, translate_begin, env, stx, compiler);
                COMPILE_REC(ref, minim_builtin_if, translate_if, env, stx, compiler);
                COMPILE_REC(ref, minim_builtin_let_values, translate_let_values, env, stx, compiler);
                COMPILE_REC(ref, minim_builtin_quote, translate_quote, env, stx, compiler);

                // EXPAND_REC(ref, minim_builtin_callcc, expand_expr_1arg, env, stx, analysis);
                // EXPAND_REC(ref, minim_builtin_delay, expand_expr_1arg, env, stx, analysis);
                // EXPAND_REC(ref, minim_builtin_setb, expand_expr_setb, env, stx, analysis);
                // EXPAND_REC(ref, minim_builtin_syntax_case, expand_expr_syntax_case, env, stx, analysis);

                // minim_builtin_quasiquote
                // minim_builtin_unquote
                // minim_builtin_syntax
                // minim_builtin_template

                return translate_interpret(env, stx, compiler);
            } 
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
                MinimObject *t = translate_expr(env, MINIM_CAR(it2), compiler);
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
        return (ref ? ref : translate_load(env, stx, compiler));
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
translate_definition_level(MinimEnv *env,
                           MinimObject *stx,
                           Compiler *compiler)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
    {
        translate_expr(env, stx, compiler);
        return;
    }

    if (MINIM_STX_NULLP(stx))
        return;

    // (<ident> <thing> ...)
    car = MINIM_STX_CAR(stx);
    if (MINIM_STX_SYMBOLP(car))
    {
        if (minim_eqvp(MINIM_STX_VAL(car), intern("%import")) ||
            minim_eqvp(MINIM_STX_VAL(car), intern("def-syntaxes")))
            return;

        if (minim_eqvp(MINIM_STX_VAL(car), intern("def-values")))
        {
            translate_def_values(env, stx, compiler);
            return;
        }
    }

    translate_expr(env, stx, compiler);
}

static void
translate_module_level(MinimEnv *env,
                       MinimObject *stx,
                       Compiler *compiler)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
        translate_expr(env, stx, compiler);

    car = MINIM_STX_CAR(stx);
    if (MINIM_OBJ_ASTP(car))
    {
        car = MINIM_STX_VAL(car);
        if (minim_eqvp(car, intern("%export")))
            return;

        if (minim_eqvp(car, intern("begin")))
        {
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                translate_module_level(env, MINIM_CAR(it), compiler);
            return;
        }
    }
    
    translate_definition_level(env, stx, compiler);
}

static void
translate_top_level(MinimEnv *env,
                    MinimObject *stx,
                    Compiler *compiler)
{
    MinimObject *car;

    if (!MINIM_STX_PAIRP(stx))
    {
        translate_expr(env, stx, compiler);
        return;
    }

    car = MINIM_STX_CAR(stx);
    if (MINIM_OBJ_ASTP(car))
    {
        car = MINIM_STX_VAL(car);
        if (minim_eqvp(car, intern("%module")))
        {
            MinimObject *t = MINIM_CDDR(MINIM_STX_CDR(stx));
            for (t = MINIM_STX_CDR(MINIM_CAR(t)); !minim_nullp(t); t = MINIM_CDR(t))
                translate_module_level(env, MINIM_CAR(t), compiler);
        }
    }
}

static void
repair_top_function(Function *func)
{
    MinimObject *reverse = minim_list_reverse(func->pseudo);
    for (MinimObject *it = MINIM_CDR(reverse); !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *line, *op;
        
        line = MINIM_STX_VAL(MINIM_CAR(it));
        op = MINIM_STX_VAL(MINIM_CAR(line));
        if (minim_eqp(op, intern("$set")))
        {
            MINIM_STX_CADR(MINIM_CAR(reverse)) = MINIM_CADR(line);
            return;
        }
    }
}

// ================================ Public ================================

void compile_module(MinimEnv *env, MinimModule *module)
{
// only enabled for linux
#if defined(MINIM_LINUX)
    Compiler compiler;
    Function *func;
    MinimEnv *env2;

    env2 = init_env(env);
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
    compiler.curr_func = func;

    //
    //  Translate into flat pseudo-assembly
    //

    translate_top_level(env2, module->body, &compiler);
    for (size_t i = 0; i < compiler.func_count; i++) {
        compiler.curr_func = compiler.funcs[i];
    }

    if (environment_variable_existsp("MINIM_LOG"))
        printf(" Compiled %zu functions\n", compiler.func_count);

    if (compiler.func_count == 0)
        return;

    //
    // Optimization passes
    //

    if (environment_variable_existsp("MINIM_LOG"))
        printf(" Optimizing...\n");

    size_t unopt_expr_count = 0, opt_expr_count = 0;
    for (size_t i = 0; i < compiler.func_count; i++) {
        compiler.curr_func = compiler.funcs[i];
        unopt_expr_count += minim_list_length(compiler.curr_func->pseudo);
        // debug_function(env, compiler.curr_func);
        function_optimize(env, compiler.funcs[i]);
        // debug_function(env, compiler.curr_func);
        opt_expr_count += minim_list_length(compiler.curr_func->pseudo);
    }

    if (environment_variable_existsp("MINIM_LOG"))
    {
        double ratio;
        
        if (unopt_expr_count > 0)
            ratio = 100.0 * (1.0 - (((double) opt_expr_count) / ((double) unopt_expr_count)));
        else
            ratio = 0.0;

        printf(" Shrunk %zu -> %zu (%.0f%%)\n", unopt_expr_count, opt_expr_count, ratio);
    }

    //
    //  Register allocation, and memory management
    //

    for (size_t i = 0; i < compiler.func_count; i++) {
        compiler.curr_func = compiler.funcs[i];
        // debug_function(env, compiler.curr_func);
        function_register_allocation(env, compiler.curr_func);
        debug_function(env, compiler.curr_func);
    }

    //
    //  Generate machine code
    //

    // for (size_t i = 0; i < compiler.func_count; i++) {
    //     MinimNativeLambda *closure;
    //     Buffer *code_buf;
    //     void *page;

    //     init_buffer(&code_buf);
    //     compiler.curr_func = compiler.funcs[i];
    //     ASSEMBLE(env, compiler.curr_func, code_buf);

    //     page = alloc_page(code_buf->pos);
    //     memcpy(page, get_buffer(code_buf), code_buf->pos);
    //     make_page_executable(page, code_buf->pos);
    //     compiler.curr_func->code = page;

    //     closure = GC_alloc(sizeof(MinimNativeLambda));
    //     closure->closure = NULL;
    //     closure->fn = page;
    //     closure->size = code_buf->pos;

    //     MinimObject *(*fn)(MinimEnv*, MinimObject*) = closure->fn;
    //     fn(env, uint_to_minim_number(1));
    // }

#endif
}

void compile_expr(MinimEnv *env, MinimObject *stx)
{
// only enabled for linux
#if defined(MINIM_LINUX)
    Compiler compiler;
    Function *func;
    MinimEnv *env2;

    // set up fresh environment
    env2 = init_env(env);

    // create a top-level "function"
    init_function(&func);
    func->name = "top";

    // intialize the compiler
    compiler.funcs = NULL;
    compiler.curr_func = func;
    compiler.func_count = 0;
    init_minim_symbol_table(&compiler.table);

    // translate
    add_function_header(func);
    translate_module_level(env2, stx, &compiler);
    add_function_footer(func, minim_null);
    repair_top_function(func);
    compiler_add_function(&compiler, func);

    for (size_t i = 0; i < compiler.func_count; i++) {
        MinimNativeLambda *closure;
        Buffer *code_buf;
        void *page;

        // alias
        func = compiler.funcs[i];

        // optimize 
        // debug_function(env, func);
        function_optimize(env, func);
        debug_function(env, func);

        // register allocation
        function_register_allocation(env, func);
        debug_function(env, func);

        // assemble
        init_buffer(&code_buf);
        ASSEMBLE(env, func, code_buf);

        // patch
        for (MinimObject *it = func->calls; !minim_nullp(it); it = MINIM_CDR(it)) {
            Function *ref;
            size_t offset;
            
            offset = MINIM_NUMBER_TO_UINT(MINIM_CDAR(it));
            ref = compiler_get_function(&compiler, MINIM_CAAR(it));
            if (ref == NULL)
                THROW(env, minim_error("cannot patch ~a", "compiler", MINIM_SYMBOL(MINIM_CAAR(it))));

            memcpy(&code_buf->data[offset], ref->code, sizeof(uintptr_t));
        }

        for (size_t i = 0; i < code_buf->pos; ++i)
            printf("%.2x ", code_buf->data[i] & 0xff);
        printf("\n");

        // allocate memory page
        page = alloc_page(code_buf->pos);
        memcpy(page, get_buffer(code_buf), code_buf->pos);
        make_page_executable(page, code_buf->pos);
        compiler.curr_func->code = page;

        closure = GC_alloc(sizeof(MinimNativeLambda));
        closure->closure = NULL;
        closure->fn = page;
        closure->size = code_buf->pos;

        if (i + 1 == compiler.func_count)
            env_intern_sym(env, MINIM_SYMBOL(intern("top")), minim_native_closure(closure));
    }
#endif
}
