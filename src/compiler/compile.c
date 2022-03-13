#include "../core/minimpriv.h"

#include "compile.h"
#include "compilepriv.h"

#define COMPILE_REC(ref, fn, exp, env, stx, comp) \
  if (MINIM_SYNTAX(ref) == (fn)) { return exp(env, stx, comp); }

static void
compile_expr(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler);

static void
compile_def_values(MinimEnv *env,
                   MinimObject *stx,
                   Compiler *compiler)
{
    MinimObject *body = MINIM_CADR(MINIM_STX_CDR(stx));
    compile_expr(env, body, compiler);
}

static void
compile_lambda(MinimEnv *env,
               MinimObject *stx,
               Compiler *compiler)
{
    MinimObject *args, *body;
    Function *func;
    
    args = MINIM_STX_CADR(stx);
    body = MINIM_CADR(MINIM_STX_CDR(stx));
    
    func = GC_alloc(sizeof(Function));

    compiler_add_function(compiler, func);

    compile_expr(env, body, compiler);
    printf("  (get <unnamed func>)\n");
}

static void
compile_expr(MinimEnv *env,
             MinimObject *stx,
             Compiler *compiler)
{
    printf(" expr: ");
    print_syntax_to_port(stx, stdout);
    printf("\n");

    if (MINIM_STX_PAIRP(stx))
    {
        // assuming (<ident> <thing> ...)
   
        // variable reference
        if (minim_eqvp(MINIM_STX_VAL(MINIM_STX_CAR(stx)), intern("%top")))
        {
            compile_expr(env, MINIM_STX_CADR(stx), compiler);
            return;
        }

        MinimObject *ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (ref && MINIM_OBJ_SYNTAXP(ref))        // syntax
        {
            // EXPAND_REC(ref, minim_builtin_begin, expand_expr_begin, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_callcc, expand_expr_1arg, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_delay, expand_expr_1arg, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_if, expand_expr_if, env, stx, analysis);
            COMPILE_REC(ref, minim_builtin_lambda, compile_lambda, env, stx, compiler);
            // EXPAND_REC(ref, minim_builtin_let_values, expand_expr_let_values, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_setb, expand_expr_setb, env, stx, analysis);
            // EXPAND_REC(ref, minim_builtin_syntax_case, expand_expr_syntax_case, env, stx, analysis);

            // minim_builtin_quote
            // minim_builtin_quasiquote
            // minim_builtin_unquote
            // minim_builtin_syntax
            // minim_builtin_template
        }
        else
        {
            compile_expr(env, MINIM_STX_CAR(stx), compiler);
            for (MinimObject *it = MINIM_STX_CDR(stx); !minim_nullp(it); it = MINIM_CDR(it))
                compile_expr(env, MINIM_CAR(it), compiler);
            
            printf("  (eval <func> <args> ...)\n");
        }
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        printf("  (get %s)\n", MINIM_STX_SYMBOL(stx));
    }
    else
    {
        printf("  (quote %s)\n", MINIM_STX_SYMBOL(stx));
    }
}

static void
compile_definition_level(MinimEnv *env,
                         MinimObject *stx,
                         Compiler *compiler)
{
    MinimObject *car, *ref;

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
        ref = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (ref && MINIM_OBJ_SYNTAXP(ref))
        {
            if (MINIM_SYNTAX(ref) == minim_builtin_def_values ||
                MINIM_SYNTAX(ref) == minim_builtin_def_syntaxes)
            {
                compile_def_values(env, stx, compiler);
                return;
            }
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
// only enabled for linux
#if defined(MINIM_LINUX)
    Compiler compiler;

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

    compile_top_level(env, module->body, &compiler);
#endif
}
