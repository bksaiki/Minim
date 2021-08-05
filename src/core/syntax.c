#include <string.h>

#include "../gc/gc.h"
#include "arity.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "syntax.h"
#include "transform.h"

// ================================ Checker ================================

#define CHECK_REC(proc, x, expr)        if (proc == x) return expr(env, ast, perr);
#define MATCH_RET(proc, x, ret)         if (proc == x) return ret;

static bool check_syntax_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **perr);

static bool check_syntax_set(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *sym, *tmp;

    unsyntax_ast(env, ast->children[1], &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        *perr = minim_syntax_error("not an identifier",
                                   ast->children[0]->sym,
                                   ast,
                                   ast->children[1]);
        return false;
    }

    init_minim_object(&tmp, MINIM_OBJ_VOID);
    env_intern_sym(env, MINIM_STRING(sym), tmp);
    return check_syntax_rec(env, ast->children[2], perr);
}

static bool check_syntax_func(MinimEnv *env, SyntaxNode *ast, MinimObject **perr, size_t name_idx)
{
    MinimObject *args, *sym;
    MinimEnv *env2;

    init_env(&env2, env, NULL);
    unsyntax_ast(env, ast->children[name_idx], &args);
    if (MINIM_OBJ_PAIRP(args))
    {
        for (MinimObject *it = args; it && !minim_nullp(it); it = MINIM_CDR(it))
        {
            MinimObject *tmp;

            if (minim_listp(it))
            {
                unsyntax_ast(env, MINIM_AST(MINIM_CAR(it)), &sym);
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    *perr = minim_syntax_error("not an identifier",
                                               ast->children[0]->sym,
                                               ast,
                                               MINIM_AST(MINIM_CAR(it)));
                    return false;
                }

                init_minim_object(&tmp, MINIM_OBJ_VOID);
                env_intern_sym(env2, MINIM_STRING(sym), tmp);
            }
            else // ... arg_n . arg_rest)
            {
                unsyntax_ast(env, MINIM_AST(MINIM_CAR(it)), &sym);
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    *perr = minim_syntax_error("not an identifier",
                                               ast->children[0]->sym,
                                               ast,
                                               MINIM_AST(MINIM_CAR(it)));
                    return false;
                }

                init_minim_object(&tmp, MINIM_OBJ_VOID);
                env_intern_sym(env2, MINIM_STRING(sym), tmp);
                
                unsyntax_ast(env, MINIM_AST(MINIM_CDR(it)), &sym);
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    *perr = minim_syntax_error("not an identifier",
                                               ast->children[0]->sym,
                                               ast,
                                               MINIM_AST(MINIM_CDR(it)));
                    return false;
                }

                init_minim_object(&tmp, MINIM_OBJ_VOID);
                env_intern_sym(env2, MINIM_STRING(sym), tmp);
                break;
            }
        }
    }
    else if (!MINIM_OBJ_SYMBOLP(args))
    {
        *perr = minim_syntax_error("expected argument names for ~s",
                                    ast->children[0]->sym,
                                    ast,
                                    ast->children[name_idx]);
        return false;
    }
    
    for (size_t i = name_idx + 1; i < ast->childc; ++i)
    {
        if (!check_syntax_rec(env2, ast->children[i], perr))
            return false;
    }
    
    return true;
}

static bool check_syntax_def_values(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *ids, *sym, *tmp;

    unsyntax_ast(env, ast->children[1], &ids);
    if (!minim_listp(ids))
    {
        *perr = minim_syntax_error("not a list of identifiers",
                                   "def-values",
                                   ast,
                                   ast->children[1]);
        return false;
    }

    for (size_t i = 0; i < ast->children[1]->childc; ++i)
    {
        unsyntax_ast(env, ast->children[1]->children[i], &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            *perr = minim_syntax_error("not an identifier",
                                       "def-values",
                                       ast,
                                       ast->children[1]->children[i]);
            return false;
        }

        for (size_t j = 0; j < i; ++j)
        {
            if (strcmp(ast->children[1]->children[j]->sym, MINIM_STRING(sym)) == 0)
            {
                *perr = minim_syntax_error("duplicate identifier",
                                           "def-values",
                                           ast,
                                           ast->children[1]->children[i]);
            }
        }
        
        init_minim_object(&tmp, MINIM_OBJ_VOID);
        env_intern_sym(env, MINIM_STRING(sym), tmp);
    }

    return check_syntax_rec(env, ast->children[2], perr);
}

static bool check_syntax_lambda(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    return check_syntax_func(env, ast, perr, 1);
}

static bool check_syntax_begin(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    for (size_t i = 0; i < ast->childc; ++i)
    {
        if (!check_syntax_rec(env, ast->children[i], perr))
            return false;
    }

    return true;
}

static bool check_syntax_if(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    return check_syntax_rec(env, ast->children[0], perr) &&
           check_syntax_rec(env, ast->children[1], perr) &&
           check_syntax_rec(env, ast->children[2], perr);
}

static bool check_syntax_case(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *branch, *match;
    SyntaxNode *datum;

    for (size_t i = 2; i < ast->childc; ++i)
    {
        unsyntax_ast(env, ast->children[i], &branch);
        if (!minim_listp(branch) || minim_list_length(branch) < 2)
        {
            *perr = minim_syntax_error("bad clause", "case", ast, ast->children[i]);
            return false;
        }

        // match
        datum = MINIM_AST(MINIM_CAR(branch));
        if (i + 1 != ast->childc && datum->sym && strcmp(datum->sym, "else") == 0)
        {
            *perr = minim_syntax_error("bad clause", "case", ast, ast->children[i]);
            return false;
        }
        
        // datums
        unsyntax_ast(env, datum, &match);
        if (!minim_listp(match))
        {
            *perr = minim_syntax_error("bad match datum", "case", ast, ast->children[i]);
            return false;
        }

        // vals
        for (MinimObject *it = MINIM_CDR(branch); it; it = MINIM_CDR(it))
        {
            if (!check_syntax_rec(env, MINIM_DATA(MINIM_CAR(it)), perr))
                return false;
        }
    }

    return true;
}

static bool check_syntax_let_values(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *bindings, *sym;
    MinimEnv *env2;
    size_t base;

    base = 1;
    init_env(&env2, env, NULL);
    unsyntax_ast(env, ast->children[base], &sym);
    if (MINIM_OBJ_SYMBOLP(sym))
    {
        MinimObject *tmp;

        init_minim_object(&tmp, MINIM_OBJ_VOID);
        env_intern_sym(env2, MINIM_STRING(sym), tmp);
        ++base;
    }

    unsyntax_ast(env, ast->children[base], &bindings);
    if (!minim_listp(bindings))
    {
        *perr = minim_syntax_error("expected a list of bindings", "let", ast, ast->children[base]);
        return false;
    }

    // early exit: (let () ...)
    if (minim_nullp(bindings))
        return check_syntax_rec(env, ast->children[base + 1], perr);

    for (MinimObject *it = bindings; it; it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym, *tmp, *ids;
        SyntaxNode *stx;
        MinimEnv *env3;

        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &bind);
        if (!minim_listp(bind) || minim_list_length(bind) != 2)
        {
            *perr = minim_syntax_error("bad binding", "let-values", ast, MINIM_AST(MINIM_CAR(it)));
            return false;
        }

        // list of identifier
        stx = MINIM_AST(MINIM_CAR(bind));
        unsyntax_ast(env, stx, &ids);
        if (!minim_listp(ids))
        {
            *perr = minim_syntax_error("not a list of identifiers",
                                       "let-values",
                                       ast,
                                       stx);
            return false;
        }

        for (size_t i = 0; i < stx->childc; ++i)
        {
            unsyntax_ast(env, stx->children[i], &sym);
            if (!MINIM_OBJ_SYMBOLP(sym))
            {
                *perr = minim_syntax_error("not an identifier",
                                           MINIM_STRING(sym),
                                           ast,
                                           stx->children[i]);
                return false;
            }

            for (size_t j = 0; j < i; ++j)
            {
                if (strcmp(stx->children[j]->sym, MINIM_STRING(sym)) == 0)
                {
                    *perr = minim_syntax_error("duplicate identifier",
                                               MINIM_STRING(sym),
                                               ast,
                                               stx->children[i]);
                }
            }
            
            init_minim_object(&tmp, MINIM_OBJ_VOID);
            env_intern_sym(env, MINIM_STRING(sym), tmp);
        }

        init_env(&env3, env, NULL);
        if (!check_syntax_rec(env3, MINIM_AST(MINIM_CADR(bind)), perr))
            return false;

        init_minim_object(&tmp, MINIM_OBJ_VOID);
        env_intern_sym(env2, MINIM_STRING(sym), tmp);
    }

    if (base + 1 == ast->childc)
    {
        *perr = minim_syntax_error("missing body", "let", ast, NULL);
        return false;
    }

    for (size_t i = base + 1; i < ast->childc; ++i)
    {
        if (!check_syntax_rec(env2, ast->children[base + 1], perr))
            return false;
    }
    
    return true;
}

static bool check_syntax_delay(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    return check_syntax_rec(env, ast->children[1], perr);
}

static bool check_syntax_def_syntax(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *sym, *rules, *tmp;
    MinimEnv *env2;

    unsyntax_ast(env, ast->children[1], &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        *perr = minim_syntax_error("not a identifier", "def-syntax", ast, ast->children[1]);
        return false;
    }

    init_env(&env2, env, NULL);
    init_minim_object(&tmp, MINIM_OBJ_VOID);
    env_intern_sym(env2, MINIM_STRING(sym), tmp);
    
    unsyntax_ast(env, ast->children[2], &rules);
    if (!minim_listp(rules) || minim_list_length(rules) == 0)
    {
        *perr = minim_syntax_error("expected a transform", "def-syntax", ast, ast->children[2]);
        return false;
    }

    if (strcmp(MINIM_AST(MINIM_CAR(rules))->sym, "lambda") != 0)
    {
        *perr = minim_syntax_error("expected a transform", "def-syntax", ast, ast->children[2]);
        return false;
    }

    if (MINIM_AST(MINIM_CADR(rules))->type != SYNTAX_NODE_LIST ||
        MINIM_AST(MINIM_CADR(rules))->childc != 1)
    {
        *perr = minim_syntax_error("takes a procedure of 1 argument",
                                    "def-syntax",
                                    ast,
                                    ast->children[2]);
        return false;
    }

    return true;
}

static bool check_syntax_syntax_case(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *reserved, *sym;

    // extract reserved symbols
    unsyntax_ast(env, ast->children[2], &reserved);
    if (!minim_listp(reserved))
    {
        *perr = minim_syntax_error("expected a list of symbols", "syntax-rules", ast, ast->children[2]);
        return false;
    }
    
    // check reserved list is all symbols
    for (MinimObject *it = reserved; it && MINIM_CAR(it); it = MINIM_CDR(it))
    {
        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            *perr = minim_syntax_error("expected a list of symbols", "syntax-rules", ast, ast->children[2]);
            return false;
        }
    }

    // check each match expression
    // [(_ arg ...) anything]
    for (size_t i = 3; i < ast->childc; ++i)
    {
        MinimObject *rule;

        unsyntax_ast(env, ast->children[i], &rule);
        if (!minim_listp(rule) || minim_list_length(rule) != 2)
        {
            *perr = minim_syntax_error("bad rule", "syntax-rules", ast, ast->children[i]);
            return false;
        }

        if (!valid_transformp(MINIM_AST(MINIM_CAR(rule)),
                             MINIM_AST(MINIM_CADR(rule)),
                             reserved,
                             perr))
        {
            return false;
        }
    }

    return true;
}

static bool check_syntax_import(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *sym;

    for (size_t i = 0; i < ast->childc; ++i)
    {
        unsyntax_ast(env, ast->children[i], &sym);
        if (!MINIM_OBJ_SYMBOLP(sym) && !MINIM_OBJ_STRINGP(sym))
        {
            *perr = minim_syntax_error("import must be a symbol or string",
                                       "%import",
                                       ast,
                                       ast->children[i]);
            return false;
        }
    }

    return true;
}

static bool check_syntax_export(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *export;

    for (size_t i = 0; i < ast->childc; ++i)
    {
        if (ast->children[i]->type == SYNTAX_NODE_LIST)
        {
            MinimObject *attrib;

            if (ast->children[i]->childc != 2)
            {
                *perr = minim_syntax_error("not a valid export form",
                                           "%export",
                                           ast,
                                           ast->children[i]);
                return false;
            }

            unsyntax_ast(env, ast->children[i]->children[0], &attrib);
            if (!MINIM_OBJ_SYMBOLP(attrib) ||
                strcmp(MINIM_STRING(attrib), "all"))
            {
                *perr = minim_syntax_error("not a valid export form",
                                           "%export",
                                           ast,
                                           ast->children[i]);
            }

            if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            {
                *perr = minim_syntax_error("export must be a symbol or string",
                                           "%export",
                                           ast,
                                           ast->children[i]);
                return false;
            }

            unsyntax_ast(env, ast->children[i]->children[1], &export);
            if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            {
                *perr = minim_syntax_error("export must be a symbol or string",
                                           "%export",
                                           ast,
                                           ast->children[i]->children[1]);
                return false;
            }
        }
        else
        {
            unsyntax_ast(env, ast->children[i], &export);
            if (!MINIM_OBJ_SYMBOLP(export) && !MINIM_OBJ_STRINGP(export))
            {
                *perr = minim_syntax_error("export must be a symbol or string",
                                           "%export",
                                           ast,
                                           ast->children[i]);
                return false;
            }
        }
    }

    return true;
}

static bool check_syntax_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *op;

    if (ast->type != SYNTAX_NODE_LIST)
        return true;

    if (ast->children[0]->sym)
    {
        op = env_get_sym(env, ast->children[0]->sym);
        if (!op)
        {
            *perr = minim_syntax_error("unknown identifier", ast->children[0]->sym, ast, NULL);
            return false;
        }
        else if (MINIM_OBJ_SYNTAXP(op))
        {
            void *proc = MINIM_DATA(op);

            if (!minim_check_syntax_arity(proc, ast->childc - 1, env))
            {
                *perr = minim_syntax_error("bad syntax", ast->children[0]->sym, ast, NULL);
                return false;
            }

            CHECK_REC(proc, minim_builtin_begin, check_syntax_begin);
            CHECK_REC(proc, minim_builtin_setb, check_syntax_set);
            CHECK_REC(proc, minim_builtin_def_values, check_syntax_def_values);
            CHECK_REC(proc, minim_builtin_if, check_syntax_if);
            CHECK_REC(proc, minim_builtin_case, check_syntax_case);
            CHECK_REC(proc, minim_builtin_let_values, check_syntax_let_values);
            CHECK_REC(proc, minim_builtin_letstar_values, check_syntax_let_values);
            CHECK_REC(proc, minim_builtin_lambda, check_syntax_lambda);
            CHECK_REC(proc, minim_builtin_delay, check_syntax_delay);

            CHECK_REC(proc, minim_builtin_def_syntax, check_syntax_def_syntax);
            CHECK_REC(proc, minim_builtin_syntax_case, check_syntax_syntax_case);

            CHECK_REC(proc, minim_builtin_import, check_syntax_import);
            CHECK_REC(proc, minim_builtin_export, check_syntax_export);

            MATCH_RET(proc, minim_builtin_quote, true);
            MATCH_RET(proc, minim_builtin_quasiquote, true);
            MATCH_RET(proc, minim_builtin_unquote, true);
            MATCH_RET(proc, minim_builtin_syntax, true);
        }
        else if (MINIM_OBJ_FUNCP(op))
        {
            for (size_t i = 1; i < ast->childc; ++i)
            {
                if (!check_syntax_rec(env, ast->children[i], perr))
                    return false;
            }
        }
    }
    else
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            if (!check_syntax_rec(env, ast->children[i], perr))
                return false;
        }
    }

    return true;
}

// ================================ Syntax Conversions ================================

static SyntaxNode *datum_to_syntax(MinimObject *obj, MinimObject **perr)
{
    SyntaxNode *node;
    Buffer *bf;

    if (MINIM_OBJ_ASTP(obj))
    {
        return MINIM_AST(obj);
    }
    else if (MINIM_OBJ_PAIRP(obj))
    {
        if (MINIM_CAR(obj) && MINIM_CDR(obj) && !MINIM_OBJ_PAIRP(MINIM_CDR(obj)))   // true pair
        {
            init_syntax_node(&node, SYNTAX_NODE_PAIR);
            node->children = GC_realloc(node->children, 2 * sizeof(SyntaxNode*));
            node->childc = 2;

            node->children[0] = datum_to_syntax(MINIM_CAR(obj), perr);
            if (!node->children[0])     return NULL;

            node->children[1] = datum_to_syntax(MINIM_CDR(obj), perr);
            if (!node->children[1])     return NULL;
        }
        else
        {
            init_syntax_node(&node, SYNTAX_NODE_LIST);
            node->children = NULL;
            node->childc = 0;

            if (MINIM_CAR(obj))
            {
                for (MinimObject *it = obj; it; it = MINIM_CDR(it))
                {
                    if (MINIM_CDR(it) && !MINIM_OBJ_PAIRP(MINIM_CDR(it)))
                    {
                        node->childc += 3;
                        node->children = GC_realloc(node->children, node->childc * sizeof(SyntaxNode*));
                        node->children[node->childc - 3] = datum_to_syntax(MINIM_CAR(it), perr);
                        if (!node->children[node->childc - 3])      return NULL;

                        init_syntax_node(&node->children[node->childc - 2], SYNTAX_NODE_DATUM);
                        node->children[node->childc - 2]->sym = ".";

                        node->children[node->childc - 1] = datum_to_syntax(MINIM_CDR(it), perr);
                        if (!node->children[node->childc - 1])      return NULL;

                        break;
                    }
                    else
                    {
                        ++node->childc;
                        node->children = GC_realloc(node->children, node->childc * sizeof(SyntaxNode*));
                        node->children[node->childc - 1] = datum_to_syntax(MINIM_CAR(it), perr);
                        if (!node->children[node->childc - 1])      return NULL;
                    }
                }
            }
        }
    }
    else if (MINIM_OBJ_VECTORP(obj))
    {
        init_syntax_node(&node, SYNTAX_NODE_VECTOR);
        node->children = GC_alloc(MINIM_VECTOR_LEN(obj) * sizeof(SyntaxNode*));
        node->childc = MINIM_VECTOR_LEN(obj);
        for (size_t i = 0; i < node->childc; ++i)
        {
            node->children[i] = datum_to_syntax(MINIM_VECTOR_ARR(obj)[i], perr);
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

        set_default_print_params(&pp);
        init_buffer(&bf);
        print_to_buffer(bf, obj, NULL, &pp);

        *perr = minim_error("can't convert to syntax", NULL);
        init_minim_error_desc_table(&MINIM_ERROR(*perr)->table, 1);
        minim_error_desc_table_set(MINIM_ERROR(*perr)->table, 0, "at", get_buffer(bf));

        node = NULL;
    }

    return node;
}

// ================================ Public ================================

bool check_syntax(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    return check_syntax_rec(env, ast, perr);
}

// ================================ Builtins ================================

MinimObject *minim_builtin_syntax(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_AST, MINIM_AST(args[0]));
    return res;
}

MinimObject *minim_builtin_syntaxp(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_ASTP(args[0]));
    return res;
}

MinimObject *minim_builtin_unwrap(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_ASTP(args[0]))
        return minim_argument_error("syntax?", "unwrap", 0, args[0]);

    unsyntax_ast(env, MINIM_AST(args[0]), &res);
    return res;
}

MinimObject *minim_builtin_to_syntax(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    SyntaxNode *stx;

    stx = datum_to_syntax(args[0], &res);
    if (!stx)   return res;

    init_minim_object(&res, MINIM_OBJ_AST, stx);
    return res;
}
