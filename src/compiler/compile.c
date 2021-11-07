#include "../core/builtin.h"
#include "../core/error.h"
#include "../core/string.h"
#include "../core/print.h"

#include "compile.h"
#include "compilepriv.h"

static void compile_datum(MinimEnv *env,
                          MinimObject *stx,
                          Buffer *bf,
                          SymbolTable *tab,
                          bool top)
{
    PrintParams pp;
    MinimObject *datum = MINIM_STX_VAL(stx);

    set_default_print_params(&pp);
    if (MINIM_OBJ_EXACTP(datum))
    {
        writes_buffer(bf, "exactnum ");
        print_to_buffer(bf, datum, env, &pp);
        writec_buffer(bf, 'c');
    }
    else if (MINIM_OBJ_INEXACTP(datum))
    {
        writes_buffer(bf, "inexactnum ");
        print_to_buffer(bf, datum, env, &pp);
        writec_buffer(bf, 'c');
    }
    else if (MINIM_OBJ_CHARP(datum))
    {
        writef_buffer(bf, "char ~u\n", (unsigned long) MINIM_CHAR(datum));
    }
    else if (MINIM_OBJ_STRINGP(datum))
    {
        writef_buffer(bf, "string ~s\n", MINIM_STRING(datum));
    }
    else if (MINIM_OBJ_SYMBOLP(datum))
    {
        if (top)    writef_buffer(bf, "lookup ~s\n", MINIM_SYMBOL(datum));
        else        writef_buffer(bf, "local-lookup ~s\n", MINIM_SYMBOL(datum));
    }
    else
    {
        printf("compiler: datum not supported\n");
        THROW(env, minim_error("panic", NULL));
    }
}

static void compile_expr(MinimEnv *env,
                         MinimObject *stx,
                         Buffer *bf,
                         SymbolTable *tab)
{
    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *op = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (!op)
        {
            printf("compiler: unknown %s\n", MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
            THROW(env, minim_error("panic", NULL));
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            printf("compiler: unimplemented\n");
            THROW(env, minim_error("panic", NULL));
        }
        else if (MINIM_OBJ_BUILTINP(op))
        {
            for (MinimObject *it = MINIM_STX_VAL(stx); !MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
            {
                compile_expr(env, MINIM_CAR(it), bf, tab);
                writes_buffer(bf, "push\n");
            }

            writef_buffer(bf, "lookup ~s\n", MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
            writes_buffer(bf, "apply\n");
        }
        else
        {
            printf("compiler: unimplemented\n");
            THROW(env, minim_error("panic", NULL));
        }
    }
    else
    {
        compile_datum(env, stx, bf, tab, false);
    }
}

static void compile_top(MinimEnv *env,
                        MinimObject *stx,
                        Buffer *bf,
                        SymbolTable *tab)
{
    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *op;
        
        op = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
        if (!op)
        {
            printf("compiler: unknown %s\n", MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
            THROW(env, minim_error("panic", NULL));
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            if (MINIM_SYNTAX(op) == minim_builtin_def_values)
            {
                size_t bindc = syntax_list_len(MINIM_STX_CADR(stx));
                if (bindc == 1)
                {
                    char *name = MINIM_STX_SYMBOL(MINIM_STX_CAR(MINIM_STX_CADR(stx)));
                    compile_expr(env, MINIM_CADR(MINIM_STX_CDR(stx)), bf, tab);
                    tab->tl.vars.vars[tab->tl.vars.set].name = name;
                    ++tab->tl.vars.set;
                    writef_buffer(bf, "bind ~s\n", name);
                }
                else
                {
                    printf("compiler: unimplemented\n");
                    THROW(env, minim_error("panic", NULL));
                }
            }
            else
            {
                printf("compiler: unimplemented\n");
                THROW(env, minim_error("panic", NULL));
            }
        }
        else
        {
            compile_expr(env, stx, bf, tab);
            writes_buffer(bf, "write\n");
        }
    }
    else
    {
        compile_datum(env, stx, bf, tab, true);
        writes_buffer(bf, "write\n");
    }
}

// ================================ Public ================================

Buffer *compile_module(MinimEnv *env, MinimModule *module)
{
#if defined(MINIM_LINUX)
    SymbolTable tab;
#endif
    Buffer *bf;

    init_buffer(&bf);
#if defined(MINIM_LINUX)    // only enabled for linux
    // set defaults
    tab.tl.vars.count = top_level_def_count(env, module);
    tab.tl.vars.vars = malloc(tab.tl.vars.count * sizeof(VarEntry));
    tab.tl.vars.set = 0;

    // emit top
    writef_buffer(bf, "global ~u\n", tab.tl.vars.count);
    writec_buffer(bf, '\n');

    // compile
    writes_buffer(bf, "@main\n");
    for (size_t i = 0; i < module->exprc; ++i)
        compile_top(env, module->exprs[i], bf, &tab);

    // debugging
    writec_buffer(bf, '\n');
    for (size_t i = 0; i < tab.tl.vars.count; ++i)
        writef_buffer(bf, "@global:~u ~s\n", i, tab.tl.vars.vars[i].name);

#endif
    return bf;
}
