#include <string.h>

#include "../gc/gc.h"
#include "arity.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "syntax.h"
#include "transform.h"

// =============================== Syntax Location ==========================

SyntaxLoc *init_syntax_loc(MinimObject *src,
                           size_t row,
                           size_t col,
                           size_t pos,
                           size_t span)
{
    SyntaxLoc *loc = GC_alloc(sizeof(SyntaxLoc));
    loc->src = src;
    loc->row = row;
    loc->col = col;
    loc->pos = pos;
    loc->span = span;
    return loc;
}

size_t syntax_list_len(MinimObject *stx)
{
    size_t len = 0;

    if (MINIM_STX_PAIRP(stx))
        stx = MINIM_STX_VAL(stx);

    while (MINIM_OBJ_PAIRP(stx))
    {
        ++len;
        stx = MINIM_CDR(stx);
        if (MINIM_STX_PAIRP(stx))
            stx = MINIM_STX_VAL(stx);
    }
    
    return len;
}

size_t syntax_proper_list_len(MinimObject *stx)
{
    size_t len = 0;

    if (MINIM_STX_PAIRP(stx))
        stx = MINIM_STX_VAL(stx);

    while (MINIM_OBJ_PAIRP(stx))
    {
        ++len;
        stx = MINIM_CDR(stx);
        if (MINIM_STX_PAIRP(stx))
            stx = MINIM_STX_VAL(stx);
    }
    
    return (minim_nullp(stx) ? len : SIZE_MAX);
}

// ================================ Checker ================================

#define CHECK_REC(proc, x, expr)        if (proc == x) expr(env, ast);

static void check_syntax_rec(MinimEnv *env, MinimObject *ast);
static MinimObject *constant_fold_rec(MinimEnv *env, MinimObject *ast);

static void check_syntax_set(MinimEnv *env, MinimObject *ast)
{
    MinimObject *sym;

    sym = unsyntax_ast(env, ast->children[1]);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        THROW(env, minim_syntax_error("not an identifier",
                                 ast->children[0]->sym,
                                 ast,
                                 ast->children[1]));
    }

    env_intern_sym(env, MINIM_STRING(sym), minim_void);
    check_syntax_rec(env, ast->children[2]);
}

static void check_syntax_func(MinimEnv *env, MinimObject *ast, size_t name_idx)
{
    MinimObject *args, *sym;
    MinimEnv *env2;

    init_env(&env2, env, NULL);
    args = unsyntax_ast(env, ast->children[name_idx]);
    if (MINIM_OBJ_PAIRP(args))
    {
        for (MinimObject *it = args; it && !minim_nullp(it); it = MINIM_CDR(it))
        {
            if (minim_nullp(MINIM_CDR(it)) || MINIM_OBJ_PAIRP(MINIM_CDR(it)))
            {
                sym = unsyntax_ast(env, MINIM_STX_VAL(MINIM_CAR(it)));
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                             ast->children[0]->sym,
                                             ast,
                                             MINIM_STX_VAL(MINIM_CAR(it))));
                }

                env_intern_sym(env2, MINIM_STRING(sym), minim_void);
            }
            else // ... arg_n . arg_rest)
            {
                sym = unsyntax_ast(env, MINIM_STX_VAL(MINIM_CAR(it)));
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                             ast->children[0]->sym,
                                             ast,
                                             MINIM_STX_VAL(MINIM_CAR(it))));
                }

                env_intern_sym(env2, MINIM_STRING(sym), minim_void);       
                sym = unsyntax_ast(env, MINIM_STX_VAL(MINIM_CDR(it)));
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    THROW(env, minim_syntax_error("not an identifier",
                                             ast->children[0]->sym,
                                             ast,
                                             MINIM_STX_VAL(MINIM_CDR(it))));
                }

                env_intern_sym(env2, MINIM_STRING(sym), minim_void);
                break;
            }
        }
    }
    else if (!MINIM_OBJ_SYMBOLP(args) && !minim_nullp(args))
    {
        THROW(env, minim_syntax_error("expected argument names",
                                 ast->children[0]->sym,
                                 ast,
                                 ast->children[name_idx]));
    }
    
    for (size_t i = name_idx + 1; i < ast->childc; ++i)
        check_syntax_rec(env2, ast->children[i]);
}

static void check_syntax_def_values(MinimEnv *env, MinimObject *ast)
{
    MinimObject *ids, *sym;

    ids = unsyntax_ast(env, ast->children[1]);
    if (!minim_listp(ids))
    {
        THROW(env, minim_syntax_error("not a list of identifiers",
                                 "def-values",
                                 ast,
                                 ast->children[1]));
    }

    for (size_t i = 0; i < ast->children[1]->childc; ++i)
    {
        sym = unsyntax_ast(env, ast->children[1]->children[i]);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            THROW(env, minim_syntax_error("not an identifier",
                                     "def-values",
                                     ast,
                                     ast->children[1]->children[i]));
        }

        for (size_t j = 0; j < i; ++j)
        {
            if (strcmp(ast->children[1]->children[j]->sym, MINIM_STRING(sym)) == 0)
            {
                THROW(env, minim_syntax_error("duplicate identifier",
                                         "def-values",
                                         ast,
                                         ast->children[1]->children[i]));
            }
        }
        
        env_intern_sym(env, MINIM_STRING(sym), minim_void);
    }

    check_syntax_rec(env, ast->children[2]);
}

static void check_syntax_lambda(MinimEnv *env, MinimObject *ast)
{
    check_syntax_func(env, ast, 1);
}

static void check_syntax_begin(MinimEnv *env, MinimObject *ast)
{
    for (size_t i = 0; i < ast->childc; ++i)
        check_syntax_rec(env, ast->children[i]);
}

static void check_syntax_if(MinimEnv *env, MinimObject *ast)
{
    check_syntax_rec(env, ast->children[0]);
    check_syntax_rec(env, ast->children[1]);
    check_syntax_rec(env, ast->children[2]);
}

// TODO: (let-values ([x ...]) ...) still passes
static void check_syntax_let_values(MinimEnv *env, MinimObject *ast)
{
    MinimObject *bindings, *sym;
    MinimEnv *env2;
    size_t base;

    base = 1;
    init_env(&env2, env, NULL);
    sym = unsyntax_ast(env, ast->children[base]);
    if (MINIM_OBJ_SYMBOLP(sym))
    {
        env_intern_sym(env2, MINIM_STRING(sym), minim_void);
        ++base;
    }

    bindings = unsyntax_ast(env, ast->children[base]);
    if (!minim_listp(bindings))
        THROW(env, minim_syntax_error("expected a list of bindings", "let", ast, ast->children[base]));

    // early exit: (let () ...)
    if (minim_nullp(bindings))
        check_syntax_rec(env, ast->children[base + 1]);

    for (MinimObject *it = bindings; !minim_nullp(it); it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym, *ids;
        MinimObject *stx;
        MinimEnv *env3;

        bind = unsyntax_ast(env, MINIM_STX_VAL(MINIM_CAR(it)));
        if (!minim_listp(bind) || minim_list_length(bind) != 2)
        {
            THROW(env, minim_syntax_error("bad binding",
                                          "let-values",
                                          ast,
                                          MINIM_STX_VAL(MINIM_CAR(it))));
        }

        // list of identifier
        stx = MINIM_STX_VAL(MINIM_CAR(bind));
        ids = unsyntax_ast(env, stx);
        if (!minim_listp(ids))
        {
            THROW(env, minim_syntax_error("not a list of identifiers",
                                          "let-values",
                                          ast,
                                          stx));
        }

        for (size_t i = 0; i < stx->childc; ++i)
        {
            sym = unsyntax_ast(env, stx->children[i]);
            if (!MINIM_OBJ_SYMBOLP(sym))
            {
                THROW(env, minim_syntax_error("not an identifier",
                                              "let-values",
                                              ast,
                                              stx->children[i]));
            }

            for (size_t j = 0; j < i; ++j)
            {
                if (strcmp(stx->children[j]->sym, MINIM_STRING(sym)) == 0)
                {
                    THROW(env, minim_syntax_error("duplicate identifier",
                                                  "let-values",
                                                  ast,
                                                  stx->children[i]));
                }
            }

            env_intern_sym(env, MINIM_STRING(sym), minim_void);
        }

        init_env(&env3, env, NULL);
        check_syntax_rec(env3, MINIM_STX_VAL(MINIM_CADR(bind)));
    }

    if (base + 1 == ast->childc)
        THROW(env, minim_syntax_error("missing body", "let", ast, NULL));

    for (size_t i = base + 1; i < ast->childc; ++i)
        check_syntax_rec(env2, ast->children[base + 1]);
}

static void check_syntax_1arg(MinimEnv *env, MinimObject *ast)
{
    check_syntax_rec(env, ast->children[1]);
}

static void check_syntax_def_syntaxes(MinimEnv *env, MinimObject *ast)
{
    MinimObject *ids, *sym;

    ids = unsyntax_ast(env, ast->children[1]);
    if (!minim_listp(ids))
    {
        THROW(env, minim_syntax_error("not a list of identifiers",
                                 "def-syntaxes",
                                 ast,
                                 ast->children[1]));
    }

    for (size_t i = 0; i < ast->children[1]->childc; ++i)
    {
        sym = unsyntax_ast(env, ast->children[1]->children[i]);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            THROW(env, minim_syntax_error("not an identifier",
                                     "def-syntaxes",
                                     ast,
                                     ast->children[1]->children[i]));
        }

        for (size_t j = 0; j < i; ++j)
        {
            if (strcmp(ast->children[1]->children[j]->sym, MINIM_STRING(sym)) == 0)
            {
                THROW(env, minim_syntax_error("duplicate identifier",
                                         "def-syntaxes",
                                         ast,
                                         ast->children[1]->children[i]));
            }
        }
        
        env_intern_sym(env, MINIM_STRING(sym), minim_void);
    }

    check_syntax_rec(env, ast->children[2]);
}

static void check_syntax_syntax_case(MinimEnv *env, MinimObject *ast)
{
    MinimObject *reserved, *sym;

    // extract reserved symbols
    reserved = unsyntax_ast(env, ast->children[2]);
    if (!minim_listp(reserved))
        THROW(env, minim_syntax_error("expected a list of symbols", "syntax-rules", ast, ast->children[2]));
    
    // check reserved list is all symbols
    for (MinimObject *it = reserved; !minim_nullp(it); it = MINIM_CDR(it))
    {
        sym = unsyntax_ast(env, MINIM_STX_VAL(MINIM_CAR(it)));
        if (!MINIM_OBJ_SYMBOLP(sym))
            THROW(env, minim_syntax_error("expected a list of symbols", "syntax-rules", ast, ast->children[2]));
    }

    // check each match expression
    // [(_ arg ...) anything]
    for (size_t i = 3; i < ast->childc; ++i)
    {
        MinimObject *rule;

        rule = unsyntax_ast(env, ast->children[i]);
        if (!minim_listp(rule) || minim_list_length(rule) != 2)
            THROW(env, minim_syntax_error("bad rule", "syntax-rules", ast, ast->children[i]));

        check_transform(env,
                        MINIM_STX_VAL(MINIM_CAR(rule)),
                        MINIM_STX_VAL(MINIM_CADR(rule)),
                        reserved);
    }
}

static void check_syntax_import(MinimEnv *env, MinimObject *ast)
{
    MinimObject *sym;

    for (size_t i = 0; i < ast->childc; ++i)
    {
        sym = unsyntax_ast(env, ast->children[i]);
        if (!MINIM_OBJ_SYMBOLP(sym) && !MINIM_OBJ_STRINGP(sym))
        {
            THROW(env, minim_syntax_error("import must be a symbol or string",
                                       "%import",
                                       ast,
                                       ast->children[i]));
        }
    }
}

static void check_syntax_export(MinimEnv *env, MinimObject *ast)
{
    MinimObject *export;
    
    for (size_t i = 0; i < ast->childc; ++i)
    {
        if (ast->children[i]->type == SYNTAX_NODE_LIST)
        {
            MinimObject *attrib;

            if (ast->children[i]->childc != 2)
            {
                THROW(env, minim_syntax_error("not a valid export form",
                                           "%export",
                                           ast,
                                           ast->children[i]));
            }

            attrib = unsyntax_ast(env, ast->children[i]->children[0]);
            if (!MINIM_OBJ_SYMBOLP(attrib) ||
                strcmp(MINIM_STRING(attrib), "all"))
            {
                THROW(env, minim_syntax_error("not a valid export form",
                                           "%export",
                                           ast,
                                           ast->children[i]));
            }

            if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            {
                THROW(env, minim_syntax_error("export must be a symbol or string",
                                           "%export",
                                           ast,
                                           ast->children[i]));
            }

            export = unsyntax_ast(env, ast->children[i]->children[1]);
            if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            {
                THROW(env, minim_syntax_error("export must be a symbol or string",
                                           "%export",
                                           ast,
                                           ast->children[i]->children[1]));
            }
        }
        else
        {
            export = unsyntax_ast(env, ast->children[i]);
            if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            {
                THROW(env, minim_syntax_error("export must be a symbol or string",
                                           "%export",
                                           ast,
                                           ast->children[i]));
            }
        }
    }
}

static void check_syntax_rec(MinimEnv *env, MinimObject *ast)
{
    MinimObject *op;

    if (ast->type != SYNTAX_NODE_LIST)  // early exit
        return;

    if (ast->children[0]->sym)
    {
        op = env_get_sym(env, ast->children[0]->sym);
        if (!op)
            THROW(env, minim_syntax_error("unknown identifier", ast->children[0]->sym, ast, NULL));

        if (MINIM_OBJ_SYNTAXP(op))
        {
            MinimBuiltin proc = MINIM_SYNTAX(op);

            if (!minim_check_syntax_arity(proc, ast->childc - 1, env))
                THROW(env, minim_syntax_error("bad syntax", ast->children[0]->sym, ast, NULL));

            CHECK_REC(proc, minim_builtin_begin, check_syntax_begin);
            CHECK_REC(proc, minim_builtin_setb, check_syntax_set);
            CHECK_REC(proc, minim_builtin_def_values, check_syntax_def_values);
            CHECK_REC(proc, minim_builtin_if, check_syntax_if);
            CHECK_REC(proc, minim_builtin_let_values, check_syntax_let_values);
            CHECK_REC(proc, minim_builtin_letstar_values, check_syntax_let_values);
            CHECK_REC(proc, minim_builtin_lambda, check_syntax_lambda);
            CHECK_REC(proc, minim_builtin_delay, check_syntax_1arg);
            CHECK_REC(proc, minim_builtin_callcc, check_syntax_1arg)

            CHECK_REC(proc, minim_builtin_def_syntaxes, check_syntax_def_syntaxes);
            CHECK_REC(proc, minim_builtin_syntax_case, check_syntax_syntax_case);

            CHECK_REC(proc, minim_builtin_import, check_syntax_import);
            CHECK_REC(proc, minim_builtin_export, check_syntax_export);

            // minim_builtin_quote
            // minim_builtin_quasiquote
            // minim_builtin_unquote
            // minim_builtin_syntax
            // minim_builtin_template
        }
        else if (MINIM_OBJ_FUNCP(op))
        {
            for (size_t i = 1; i < ast->childc; ++i)
                check_syntax_rec(env, ast->children[i]);
        }
    }
    else
    {
        for (size_t i = 0; i < ast->childc; ++i)
            check_syntax_rec(env, ast->children[i]);
    }
}

// ================================ Constant Folder ================================

#define FOLD_REC(proc, x, expr)        if (proc == x) return expr(env, ast);

static MinimObject *fold_datum(MinimEnv *env, MinimObject *ast, MinimObject *obj)
{
    if (minim_nullp(obj))
    {
        MinimObject *node;
        Buffer *bf;

        init_buffer(&bf);
        writes_buffer(bf, "null");
        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = get_buffer(bf);
        return minim_cons(node, obj);
    }
    else if (minim_voidp(obj))
    {
        return minim_cons(ast, obj);
    }
    else if (minim_specialp(obj))
    {
        return minim_cons(datum_to_syntax(env, obj), obj);
    }
    else if (MINIM_OBJ_FUNCP(obj) || MINIM_OBJ_VALUESP(obj))
    {
        return minim_cons(ast, NULL);
    }
    else if (MINIM_OBJ_STRINGP(obj))
    {
        return minim_cons(datum_to_syntax(env, obj), obj);
    }
    else
    {
        return minim_cons(ast, obj);
        
    }
}

static MinimObject *fold_syntax_def_values(MinimEnv *env, MinimObject *ast)
{
    ast->children[2] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[2]));
    return minim_cons(ast, NULL);
}

static MinimObject *fold_syntax_let_values(MinimEnv *env, MinimObject *ast)
{
    MinimObject *bindings = ast->children[1];
    for (size_t i = 0; i < bindings->childc; ++i)
    {
        MinimObject *bind = bindings->children[i];
        bind->children[1] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, bind->children[1]));
    }

    ast->children[2] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[2]));
    return minim_cons(ast, NULL);
}

static MinimObject *fold_syntax_lambda(MinimEnv *env, MinimObject *ast)
{
    for (size_t i = 2; i < ast->childc; ++i)
        ast->children[i] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[i]));

    return minim_cons(ast, NULL);
}

static MinimObject *fold_syntax_begin(MinimEnv *env, MinimObject *ast)
{
    for (size_t i = 1; i < ast->childc; ++i)
        ast->children[i] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[i]));

    return minim_cons(ast, NULL);
}

static MinimObject *fold_syntax_if(MinimEnv *env, MinimObject *ast)
{
    ast->children[1] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[1]));
    ast->children[2] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[2]));
    ast->children[3] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[3]));
    return minim_cons(ast, NULL);
}

static MinimObject *fold_syntax_1arg(MinimEnv *env, MinimObject *ast)
{
    ast->children[1] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[1]));
    return minim_cons(ast, NULL);
}

static MinimObject *fold_syntax_quote(MinimEnv *env, MinimObject *ast)
{
    MinimObject *obj;
    
    obj = unsyntax_ast_rec(env, ast->children[1]);
    return fold_datum(env, ast, obj);
}

static MinimObject *constant_fold_rec(MinimEnv *env, MinimObject *ast)
{
    if (ast->type != SYNTAX_NODE_LIST)
    {
        MinimObject *obj = eval_ast_terminal(env, ast);
        return (obj == NULL) ? minim_cons(ast, NULL) : fold_datum(env, ast, obj);
    }

    if (ast->children[0]->sym)
    {
        MinimObject *op;
        MinimBuiltin proc;
        
        op = env_get_sym(env, ast->children[0]->sym);
        if (!op || MINIM_OBJ_CLOSUREP(op))
        {
            for (size_t i = 1; i < ast->childc; ++i)
                ast->children[i] = (MinimObject*) MINIM_CAR(constant_fold_rec(env, ast->children[i]));

            return minim_cons(ast, NULL);
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            proc = MINIM_SYNTAX(op);

            FOLD_REC(proc, minim_builtin_setb, fold_syntax_def_values);
            FOLD_REC(proc, minim_builtin_def_values, fold_syntax_def_values);
            FOLD_REC(proc, minim_builtin_let_values, fold_syntax_let_values);
            FOLD_REC(proc, minim_builtin_letstar_values, fold_syntax_let_values);
            FOLD_REC(proc, minim_builtin_def_syntaxes, fold_syntax_def_values);
            FOLD_REC(proc, minim_builtin_lambda, fold_syntax_lambda);
            
            FOLD_REC(proc, minim_builtin_begin, fold_syntax_begin);
            FOLD_REC(proc, minim_builtin_if, fold_syntax_if);
            FOLD_REC(proc, minim_builtin_delay, fold_syntax_1arg);
            FOLD_REC(proc, minim_builtin_callcc, fold_syntax_1arg);
            FOLD_REC(proc, minim_builtin_quote, fold_syntax_quote);


            // minim_builtin_import
            // minim_builtin_export
            // minim_building_syntax_case
            // minim_builtin_quasiquote
            // minim_builtin_unquote
            // minim_builtin_syntax
            // minim_builtin_template

            return minim_cons(ast, NULL);
        }
        else if (MINIM_OBJ_BUILTINP(op))
        {
            MinimObject *arg, **args;
            size_t argc;
            bool foldp;

            foldp = true;
            argc = ast->childc - 1;
            proc = MINIM_BUILTIN(op);
            args = GC_alloc(argc * sizeof(MinimObject*));
            for (size_t i = 1; i < ast->childc; ++i)
            {
                arg = constant_fold_rec(env, ast->children[i]);
                ast->children[i] = (MinimObject*) MINIM_CAR(arg);

                if (!MINIM_CDR(arg))
                    foldp = false;
                else if (foldp)
                    args[i - 1] = MINIM_CDR(arg);
            }
            
            if (foldp && builtin_foldablep(proc))
            {
                arg = proc(env, argc, args);
                return fold_datum(env, ast, arg);
            }
            else
            {
                return minim_cons(ast, NULL);
            }
        }
        else
        {
            return minim_cons(ast, NULL);
        }
    }
    else
    {
        MinimObject *arg;

        for (size_t i = 0; i < ast->childc; ++i)
        {
            arg = constant_fold_rec(env, ast->children[i]);
            ast->children[i] = (MinimObject*) MINIM_CAR(arg);
        }

        return minim_cons(ast, NULL);
    }
}

// ================================ Syntax Conversions ================================

MinimObject *datum_to_syntax(MinimEnv *env, MinimObject *obj)
{
    MinimObject *node;
    Buffer *bf;

    if (minim_nullp(obj))
    {
        init_syntax_node(&node, SYNTAX_NODE_LIST);
        node->childc = 0;
        return node;  
    }
    else if (minim_truep(obj))
    {
        init_buffer(&bf);
        writes_buffer(bf, "true");
        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = get_buffer(bf);
    }
    else if (minim_falsep(obj))
    {
        init_buffer(&bf);
        writes_buffer(bf, "false");
        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = get_buffer(bf);
    }
    else if (minim_voidp(obj))
    {
        init_buffer(&bf);
        writes_buffer(bf, "void");
        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = get_buffer(bf);
    }
    else if (MINIM_OBJ_ASTP(obj))
    {
        return MINIM_STX_VAL(obj);
    }
    else if (MINIM_OBJ_PAIRP(obj))
    {
        if (!minim_nullp(MINIM_CDR(obj)) && !MINIM_OBJ_PAIRP(MINIM_CDR(obj)))   // true pair
        {
            init_syntax_node(&node, SYNTAX_NODE_PAIR);
            node->children = GC_realloc(node->children, 2 * sizeof(MinimObject*));
            node->childc = 2;

            node->children[0] = datum_to_syntax(env, MINIM_CAR(obj));
            node->children[1] = datum_to_syntax(env, MINIM_CDR(obj));
        }
        else
        {
            init_syntax_node(&node, SYNTAX_NODE_LIST);
            node->children = NULL;
            node->childc = 0;

            for (MinimObject *it = obj; !minim_nullp(it); it = MINIM_CDR(it))
            {
                if (!minim_nullp(MINIM_CDR(it)) && !MINIM_OBJ_PAIRP(MINIM_CDR(it)))
                {
                    node->childc += 3;
                    node->children = GC_realloc(node->children, node->childc * sizeof(MinimObject*));
                    node->children[node->childc - 3] = datum_to_syntax(env, MINIM_CAR(it));

                    init_syntax_node(&node->children[node->childc - 2], SYNTAX_NODE_DATUM);
                    node->children[node->childc - 2]->sym = ".";

                    node->children[node->childc - 1] = datum_to_syntax(env, MINIM_CDR(it));

                    break;
                }
                else
                {
                    ++node->childc;
                    node->children = GC_realloc(node->children, node->childc * sizeof(MinimObject*));
                    node->children[node->childc - 1] = datum_to_syntax(env, MINIM_CAR(it));
                }
            }
        }
    }
    else if (MINIM_OBJ_VECTORP(obj))
    {
        init_syntax_node(&node, SYNTAX_NODE_VECTOR);
        node->children = GC_alloc(MINIM_VECTOR_LEN(obj) * sizeof(MinimObject*));
        node->childc = MINIM_VECTOR_LEN(obj);
        for (size_t i = 0; i < node->childc; ++i)
        {
            node->children[i] = datum_to_syntax(env, MINIM_VECTOR_REF(obj, i));
            if (!node->children[i])     return NULL;
        }
    }
    else if (MINIM_OBJ_STRINGP(obj))
    {
        init_buffer(&bf);
        writef_buffer(bf, "\"~s\"", MINIM_STRING(obj));
        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = get_buffer(bf);

    }
    else if (MINIM_OBJ_SYMBOLP(obj))
    {
        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = MINIM_STRING(obj);
    }
    else if (MINIM_OBJ_NUMBERP(obj))
    {
        PrintParams pp;

        set_default_print_params(&pp);
        init_buffer(&bf);
        print_to_buffer(bf, obj, NULL, &pp);

        init_syntax_node(&node, SYNTAX_NODE_DATUM);
        node->sym = get_buffer(bf);
    }
    else
    {
        PrintParams pp;
        MinimObject *err;

        set_default_print_params(&pp);
        init_buffer(&bf);
        print_to_buffer(bf, obj, NULL, &pp);

        err = minim_error("can't convert to syntax", NULL);
        init_minim_error_desc_table(&MINIM_ERROR(err)->table, 1);
        minim_error_desc_table_set(MINIM_ERROR(err)->table, 0, "at", get_buffer(bf));
        THROW(env, err);
    }

    return node;
}

// ================================ Public ================================

void check_syntax(MinimEnv *env, MinimObject *ast)
{
    MinimEnv *env2;

    init_env(&env2, env, NULL);
    check_syntax_rec(env2, ast);
}

MinimObject *constant_fold(MinimEnv *env, MinimObject *ast)
{
    MinimObject *folded;

    folded = ((MinimObject*) MINIM_CAR(constant_fold_rec(env, ast)));
    return folded;
}

// ================================ Builtins ================================

MinimObject *minim_builtin_syntax(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *cp;

    copy_syntax_node(&cp, MINIM_STX_VAL(args[0]));
    return minim_ast(cp);
}

MinimObject *minim_builtin_syntaxp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_ASTP(args[0]));
}

MinimObject *minim_builtin_unwrap(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_ASTP(args[0]))
        THROW(env, minim_argument_error("syntax?", "unwrap", 0, args[0]));

    return unsyntax_ast(env, MINIM_STX_VAL(args[0]));
}

MinimObject *minim_builtin_to_syntax(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_ast(datum_to_syntax(env, args[0]));
}
