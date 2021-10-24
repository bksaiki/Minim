#include "../core/builtin.h"
#include "../core/error.h"
#include "../core/string.h"

#include "compile.h"
#include "compilepriv.h"

static void compile_datum(MinimEnv *env,
                          SyntaxNode *stx,
                          Buffer *bf,
                          SymbolTable *tab)
{
    if (is_rational(stx->sym))
    {
        writef_buffer(bf, "exactnum ~s\n", stx->sym);
    }
    else if (is_float(stx->sym))
    {
        writef_buffer(bf, "inexactnum ~s\n", stx->sym);
    }
    // else if (is_char(stx->sym))
    // {
    //     return minim_char(stx->sym[2]);
    // }
    // else if (is_str(stx->sym))
    // {
    //     size_t len = strlen(stx->sym) - 1;
    //     char *tmp = GC_alloc_atomic(len * sizeof(char));

    //     strncpy(tmp, &stx->sym[1], len - 1);
    //     tmp[len - 1] = '\0';
    //     return minim_string(tmp);
    // }
    else
    {
        printf("compiler: unknown %s\n", stx->children[0]->sym);
        THROW(env, minim_error("panic", NULL));
    }
}

static void compile_expr(MinimEnv *env,
                         SyntaxNode *stx,
                         Buffer *bf,
                         SymbolTable *tab)
{
    if (stx->type == SYNTAX_NODE_LIST)
    {
        printf("compiling func: ");
        print_ast(stx);
        printf("\n");

        MinimObject *op = env_get_sym(env, stx->children[0]->sym);
        if (!op)
        {
            printf("compiler: unknown %s\n", stx->children[0]->sym);
            THROW(env, minim_error("panic", NULL));
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            printf("compiler: unimplemented\n");
            THROW(env, minim_error("panic", NULL));
        }
        else if (MINIM_OBJ_BUILTINP(op))
        {
            printf("compiling builtin: %s\n", stx->children[0]->sym);
            for (size_t i = 1; i < stx->childc; ++i)
            {
                compile_expr(env, stx->children[i], bf, tab);
                writes_buffer(bf, "push\n");
            }

            writef_buffer(bf, "lookup ~s\n", stx->children[0]->sym);
            writes_buffer(bf, "apply\n");
        }
        else
        {
            printf("compiling closure: %s\n", stx->children[0]->sym);
            printf("compiler: unimplemented\n");
            THROW(env, minim_error("panic", NULL));
        }
    }
    else if (stx->type == SYNTAX_NODE_DATUM)
    {
        printf("compiling datum: %s\n", stx->sym);
        compile_datum(env, stx, bf, tab);
    }
    else
    {
        printf("compiler: bad things happened!!\n");
        THROW(env, minim_error("panic", NULL));
    }
}

static void compile_top(MinimEnv *env,
                        SyntaxNode *stx,
                        Buffer *bf,
                        SymbolTable *tab)
{
    if (stx->type == SYNTAX_NODE_LIST &&  // syntax or builtin
        stx->childc > 0)
    {
        MinimObject *op;
        
        op = env_get_sym(env, stx->children[0]->sym);
        if (!op)
        {
            printf("compiler: unknown %s\n", stx->children[0]->sym);
            THROW(env, minim_error("panic", NULL));
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            printf("compiling syntax: %s\n", stx->children[0]->sym);
            if (MINIM_SYNTAX(op) == minim_builtin_def_values)
            {
                if (stx->children[1]->childc == 0)
                {

                }
                else if (stx->children[1]->childc == 1)
                {
                    compile_expr(env, stx->children[2], bf, tab);
                    tab->tl.vars.vars[tab->tl.vars.set].name = stx->children[1]->children[0]->sym;
                    ++tab->tl.vars.set;
                    writef_buffer(bf, "bind ~s\n", stx->children[1]->children[0]->sym);
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
        }
    }
    else if (stx->type == SYNTAX_NODE_DATUM)
    {
        compile_expr(env, stx, bf, tab);
    }
    else
    {
        printf("compiler: bad things happened!!\n");
        THROW(env, minim_error("panic", NULL));
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
    for (size_t i = 0; i < module->exprc; ++i)
        compile_top(env, module->exprs[i], bf, &tab);

    // debugging
    writec_buffer(bf, '\n');
    for (size_t i = 0; i < tab.tl.vars.count; ++i)
        writef_buffer(bf, "@global:name ~s\n", tab.tl.vars.vars[i].name);

#endif
    return bf;
}
