#include <string.h>
#include "arity.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "syntax.h"
#include "transform.h"

#define CHECK_REC(proc, x, expr)        if (proc == x) return expr(env, ast, perr)

static bool check_syntax_rec(MinimEnv *env, SyntaxNode *ast, MinimObject **perr);

static bool check_syntax_set(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *sym;

    unsyntax_ast(env, ast->children[1], &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
        return false;
    }

    return check_syntax_rec(env, ast->children[2], perr);
}

static bool check_syntax_func(MinimEnv *env, SyntaxNode *ast, MinimObject **perr, size_t name_idx)
{
    MinimObject *args, *sym;

    unsyntax_ast(env, ast->children[name_idx], &args);
    if (MINIM_OBJ_PAIRP(args))
    {
        for (MinimObject *it = args; it && !minim_nullp(it); it = MINIM_CDR(it))
        {
            if (minim_listp(it))
            {
                unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &sym);
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
                    return false;
                }
            }
            else // ... arg_n . arg_rest)
            {
                unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &sym);
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
                    return false;
                }
                
                unsyntax_ast(env, MINIM_DATA(MINIM_CDR(it)), &sym);
                if (!MINIM_OBJ_SYMBOLP(sym))
                {
                    *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
                    return false;
                }

                break;
            }
        }
    }
    else if (!MINIM_OBJ_SYMBOLP(args))
    {
        *perr = minim_error("expected argument names for ~s", ast->children[0]->sym,
                            ((name_idx == 2) ? ast->children[1]->sym : "unnamed lambda"));
        return false;
    }
    
    for (size_t i = name_idx + 1; i < ast->childc; ++i)
    {
        if (!check_syntax_rec(env, ast->children[i], perr))
            return false;
    }
    
    return true;
}

static bool check_syntax_def(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *sym;

    if (ast->childc == 3)
        return check_syntax_set(env, ast, perr);

    unsyntax_ast(env, ast->children[1], &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
        return false;
    }

    return check_syntax_func(env, ast, perr, 2);
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

static bool check_syntax_cond(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *branch;
    SyntaxNode *cond;

    for (size_t i = 1; i < ast->childc; ++i)
    {
        unsyntax_ast(env, ast->children[i], &branch);
        if (!minim_listp(branch) || minim_list_length(branch) < 2)
        {
            *perr = minim_error("([<cond> <exprs> ...] ...)", "cond");
            return false;
        }

        // condition
        cond = MINIM_DATA(MINIM_CAR(branch));
        if (i + 1 != ast->childc && cond->sym && strcmp(cond->sym, "else") == 0)
            return minim_error("else clause must be last", "cond");
        
        for (MinimObject *it = MINIM_CDR(branch); it; it = MINIM_CDR(it))
        {
            if (!check_syntax_rec(env, MINIM_DATA(MINIM_CAR(it)), perr))
                return false;
        }
    }

    return true;
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
            *perr = minim_error("([(<datum> ...) <exprs> ...] ...)", "case");
            return false;
        }

        // match
        datum = MINIM_DATA(MINIM_CAR(branch));
        if (i + 2 != ast->childc && datum->sym && strcmp(datum->sym, "else") == 0)
            return minim_error("else clause must be last", "case");
        
        // datums
        unsyntax_ast(env, datum, &match);
        if (!minim_listp(match))
        {
            *perr = minim_error("([(<datum> ...) <exprs> ...] ...)", "case");
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

static bool check_syntax_for(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *bindings;

    unsyntax_ast(env, ast->children[1], &bindings);
    if (!minim_listp(bindings))
    {
        *perr = minim_error("expected a list of bindings", ast->children[0]->sym);
        return false;
    }

    // early exit: (for () ...)
    if (minim_nullp(bindings))
        return check_syntax_rec(env, ast->children[2], perr);

    for (MinimObject *it = bindings; it; it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym;
        SyntaxNode *syn;
        bool body;

        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &bind);
        if (!minim_listp(bind) || minim_list_length(bind) != 2)
        {
            *perr = minim_error("([<symbol> <value>] ...)", ast->children[0]->sym);
            return false;
        }

        // identifier
        syn = MINIM_DATA(MINIM_CAR(bind));
        unsyntax_ast(env, syn, &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
            return false;
        }

        syn = MINIM_DATA(MINIM_CADR(bind));
        body = check_syntax_rec(env, syn, perr);
        if (!body)  return false;
    }

    return check_syntax_rec(env, ast->children[2], perr);
}

static bool check_syntax_let(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *bindings, *sym;
    size_t base = 1;

    if (ast->childc == 4)
    {
        unsyntax_ast(env, ast->children[1], &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
            return false;
        }

        ++base;
    }

    unsyntax_ast(env, ast->children[base], &bindings);
    if (!minim_listp(bindings))
    {
        *perr = minim_error("expected a list of bindings", ast->children[0]->sym);
        return false;
    }

    // early exit: (let () ...)
    if (minim_nullp(bindings))
        return check_syntax_rec(env, ast->children[base + 1], perr);

    for (MinimObject *it = bindings; it; it = MINIM_CDR(it))
    {
        MinimObject *bind, *sym;
        SyntaxNode *syn;
        bool body;

        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &bind);
        if (!minim_listp(bind) || minim_list_length(bind) != 2)
        {
            *perr = minim_error("([<symbol> <value>] ...)", ast->children[0]->sym);
            return false;
        }

        // identifier
        syn = MINIM_DATA(MINIM_CAR(bind));
        unsyntax_ast(env, syn, &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
            return false;
        }

        syn = MINIM_DATA(MINIM_CADR(bind));
        body = check_syntax_rec(env, syn, perr);
        if (!body)  return false;
    }

    return check_syntax_rec(env, ast->children[base + 1], perr);
}

static bool check_syntax_delay(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    return check_syntax_rec(env, ast->children[1], perr);
}

// (syntax-rules (reserved ...)
//  [(_ elem _) syntax]
//  ...)
static bool check_syntax_syntax_rules(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *reserved, *sym;

    // extract reserved symbols
    unsyntax_ast(env, ast->children[1], &reserved);
    if (!minim_listp(reserved))
    {
        *perr = minim_error("expected a list of reserved symbols", NULL);
        return false;
    }
    
    // check reserved list is all symbols
    for (MinimObject *it = reserved; it && MINIM_CAR(it); it = MINIM_CDR(it))
    {
        unsyntax_ast(env, MINIM_DATA(MINIM_CAR(it)), &sym);
        if (!MINIM_OBJ_SYMBOLP(sym))
        {
            *perr = minim_error("expected a list of reserved symbols", NULL);
            return false;
        }
    }

    // check each match expression
    // [(_ arg ...) anything]
    for (size_t i = 2; i < ast->childc; ++i)
    {
        MinimObject *rule, *match;

        unsyntax_ast(env, ast->children[i], &rule);
        if (!minim_listp(rule) || minim_list_length(rule) != 2)
        {
            *perr = minim_error("expected a rule [match replace]", NULL);
            return false;
        }

        unsyntax_ast(env, MINIM_AST(MINIM_CAR(rule)), &match);
        if (!minim_listp(match) || minim_list_length(match) < 1)
        {
            *perr = minim_error("match expression must be (_ args ...)", NULL);
            return false;
        }

        if (!check_syntax_rec(env, MINIM_AST(MINIM_CAR(rule)), perr) ||
            !check_syntax_rec(env, MINIM_AST(MINIM_CADR(rule)), perr))
            return false;

        if (!valid_transformp(MINIM_AST(MINIM_CAR(rule)),
                              MINIM_AST(MINIM_CADR(rule)),
                              reserved,
                              perr))
            return false;
    }

    return true;
}

static bool check_syntax_def_syntax(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *sym, *rules;

    unsyntax_ast(env, ast->children[1], &sym);
    if (!MINIM_OBJ_SYMBOLP(sym))
    {
        *perr = minim_error("identifier should be symbol", ast->children[0]->sym);
        return false;
    }

    unsyntax_ast(env, ast->children[2], &rules);
    if (!minim_listp(rules) || minim_list_length(rules) == 0)
    {
        *perr = minim_error("expected a transform: (def-syntax ~s (syntax-rules ...)", NULL, MINIM_STRING(sym));
        return false;
    }

    if (strcmp(MINIM_AST(MINIM_CAR(rules))->sym, "syntax-rules") != 0)
    {
        *perr = minim_error("expected a transform: (def-syntax ~s (syntax-rules ...)", NULL, MINIM_STRING(sym));
        return false;
    }

    if (!check_syntax_syntax_rules(env, ast->children[2], perr))
    {
        MINIM_ERROR(*perr)->where = MINIM_STRING(sym);
        return false;
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
        if (op && MINIM_OBJ_SYNTAXP(op))
        {
            void *proc = MINIM_DATA(op);

            if (!minim_check_syntax_arity(proc, ast->childc - 1, env))
            {
                *perr = minim_error("bad syntax", ast->children[0]->sym);
                return false;
            }

            CHECK_REC(proc, minim_builtin_begin, check_syntax_begin);
            CHECK_REC(proc, minim_builtin_setb, check_syntax_set);
            CHECK_REC(proc, minim_builtin_def, check_syntax_def);
            CHECK_REC(proc, minim_builtin_if, check_syntax_begin);
            CHECK_REC(proc, minim_builtin_cond, check_syntax_cond);
            CHECK_REC(proc, minim_builtin_case, check_syntax_case);
            CHECK_REC(proc, minim_builtin_let, check_syntax_let);
            CHECK_REC(proc, minim_builtin_letstar, check_syntax_let);
            CHECK_REC(proc, minim_builtin_for, check_syntax_for);
            CHECK_REC(proc, minim_builtin_for_list, check_syntax_for);
            CHECK_REC(proc, minim_builtin_lambda, check_syntax_lambda);
            CHECK_REC(proc, minim_builtin_delay, check_syntax_delay);
            // CHECK_REC(proc, minim_builtin_quote, true);
            // CHECK_REC(proc, minim_builtin_quasiquote, true);
            // CHECK_REC(proc, minim_builtin_unquote, true);
            CHECK_REC(proc, minim_builtin_def_syntax, check_syntax_def_syntax);
            CHECK_REC(proc, minim_builtin_def_syntax, check_syntax_syntax_rules);
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

// Exported

bool check_syntax(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimEnv *env2;

    init_env(&env2, env, NULL);
    return check_syntax_rec(env2, ast, perr);
}
