#include <string.h>

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"

#define CHECK_REC(proc, x, expr)        if (proc == x) return expr(env, ast, perr)

// ================================ Match table ================================

typedef struct MatchTable
{
    MinimObject **objs;     // associated objects
    size_t *depths;         // pattern depth
    char **syms;            // variable name
    size_t size;            // number of variables
} MatchTable;

static void
gc_mark_match_table(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MatchTable *table = (MatchTable*) ptr;

    mrk(gc, table->objs);
    mrk(gc, table->depths);
    mrk(gc, table->syms);
}

#define GC_alloc_match_table() GC_alloc_opt(sizeof(MatchTable), NULL, gc_mark_match_table)

static void
init_match_table(MatchTable *table)
{
    table->objs = NULL;
    table->depths = NULL;
    table->syms = NULL;
    table->size = 0;
}

#define clear_match_table(table)    init_match_table(table)

static void
copy_minim_table(MatchTable *dest, MatchTable *src)
{
    dest->objs = GC_alloc(src->size * sizeof(MinimObject*));
    dest->depths = GC_alloc(src->size * sizeof(size_t));
    dest->syms = GC_alloc(src->size * sizeof(char*));
    dest->size = src->size;
    for (size_t i = 0; i < src->size; ++i)
    {   // shallow copies are okay since we only access one copy at a time
        dest->objs[i] = src->objs[i];
        dest->depths = src->depths;
        dest->syms = src->syms;
    }
}

static void
match_table_add(MatchTable *table, char *sym, size_t depth, MinimObject *obj)
{
    ++table->size;
    table->objs = GC_realloc(table->objs, table->size * sizeof(MinimObject*));
    table->depths = GC_realloc(table->depths, table->size * sizeof(size_t));
    table->syms = GC_realloc(table->syms, table->size * sizeof(char*));

    table->objs[table->size - 1] = obj;
    table->depths[table->size - 1] = depth;
    table->syms[table->size - 1] = sym;
}

typedef struct SymbolList
{
    char **syms;        // names
    size_t size;
} SymbolList;

static void
init_symbol_list(SymbolList *table, size_t size)
{
    table->syms = GC_alloc(size * sizeof(char*));
    table->size = size;
}

static bool
symbol_list_contains(SymbolList *list, const char *sym)
{
    for (size_t i = 0; i < list->size; ++i)
    {
        if (strcmp(list->syms[i], sym) == 0)
            return true;
    }

    return false;
}

// ================================ Transform ================================

static bool
is_match_pattern(MinimObject *match)
{
    return (match && MINIM_CAR(match) &&
            MINIM_CDR(match) && MINIM_CADR(match) &&
            MINIM_AST(MINIM_CADR(match))->type == SYNTAX_NODE_DATUM &&
            strcmp(MINIM_AST(MINIM_CADR(match))->sym, "...") == 0);
}

static bool
is_replace_pattern(SyntaxNode *replace, size_t idx)
{
    return ((replace->type == SYNTAX_NODE_LIST || replace->type == SYNTAX_NODE_VECTOR) &&
            idx + 1 < replace->childc &&
            replace->children[idx + 1]->sym &&
            strcmp(replace->children[idx + 1]->sym, "...") == 0);
}

static bool
match_transform(MinimEnv *env, SyntaxNode *match, SyntaxNode *ast, MatchTable *table,
                SymbolList *reserved, size_t level, size_t pdepth)
{
    MinimObject *match_unbox, *ast_unbox, *it, *it2;
    size_t len_m, len_a, idx;

    // printf("Match: "); print_ast(match); printf("\n");
    // printf("Given: "); print_ast(ast); printf("\n");

    unsyntax_ast(env, match, &match_unbox);
    unsyntax_ast(env, ast, &ast_unbox);
    if (minim_listp(match_unbox) && minim_listp(ast_unbox))
    {
        // handle null cases
        if (minim_nullp(match_unbox) && minim_nullp(ast_unbox))
            return true;

        if (minim_nullp(match_unbox) || minim_nullp(ast_unbox))
            return false;

        idx = 0;
        len_m = minim_list_length(match_unbox);
        len_a = minim_list_length(ast_unbox);
        it = match_unbox;
        it2 = ast_unbox;

        while (it && it2)
        {
            bool advance = true;

            if (idx != 0 || level == 0)
            {
                if (is_match_pattern(it))
                {
                    MinimObject *head, *it3, *name, *val;
                    MinimEnv *env2;

                    // printf("pattern in syntax rule\n");
                    // printf(" idx: %zu\n", idx);
                    // printf(" len_m: %zu\n", len_m);
                    // printf(" len_a: %zu\n", len_a);
                    
                    if (len_m > len_a + 2)    // not enough space for remaining
                        return false;

                    name = MINIM_CAR(it);
                    if (len_m == len_a + 2)
                    {
                        init_minim_object(&head, MINIM_OBJ_PAIR, NULL, NULL);
                    }
                    else
                    {
                        head = NULL;
                        for (size_t j = idx; j < len_a - (len_m - idx - 2); ++j, it2 = MINIM_CDR(it2))
                        {
                            init_env(&env2, NULL, NULL);    // isolated
                            match_transform(env2, MINIM_AST(name), MINIM_AST(MINIM_CAR(it2)),
                                            table, reserved, level + 1, pdepth);
                            val = env_get_sym(env2, MINIM_AST(name)->sym);
                            if (!val)   return false;

                            if (head)
                            {
                                init_minim_object(&MINIM_CDR(it3), MINIM_OBJ_PAIR, val, NULL);
                                it3 = MINIM_CDR(it3);
                            }
                            else
                            {
                                init_minim_object(&head, MINIM_OBJ_PAIR, val, NULL);
                                it3 = head;
                            }
                        }
                    }

                    match_table_add(table, MINIM_AST(name)->sym, 1, head);
                    it = MINIM_CDR(it);
                    advance = false;
                    ++idx;
                }
                else if (!match_transform(env, MINIM_AST(MINIM_CAR(it)), MINIM_AST(MINIM_CAR(it2)),
                                          table, reserved, level + 1, pdepth))
                {
                    return false;
                }
            }

            it = MINIM_CDR(it);
            if (advance) it2 = MINIM_CDR(it2);
            ++idx;
        }

        if (!it2 && is_match_pattern(it))
        {
            MinimObject *null;

            init_minim_object(&null, MINIM_OBJ_PAIR, NULL, NULL);  // wrap first
            match_table_add(table, MINIM_AST(MINIM_CAR(it))->sym, 0, null);
            it = MINIM_CDDR(it);
        }

        return (!it && !it2);   // both reached end
    }
    else if (MINIM_OBJ_VECTORP(match_unbox) && MINIM_OBJ_VECTORP(ast_unbox))
    {
        len_m = MINIM_VECTOR_LEN(match_unbox);
        len_a = MINIM_VECTOR_LEN(ast_unbox);

        if (len_m != len_a)     return false;
        if (len_m == 0)         return true;
        
        for (size_t i = 0; i < len_m; ++i)
        {
            if (!match_transform(env, MINIM_AST(MINIM_VECTOR_ARR(match_unbox)[i]),
                                 MINIM_AST(MINIM_VECTOR_ARR(ast_unbox)[i]),
                                 table, reserved, level + 1, pdepth))
                return false;
        }

    }
    else if (MINIM_OBJ_SYMBOLP(match_unbox))    // intern matching syntax
    {
        if (symbol_list_contains(reserved, MINIM_STRING(match_unbox)))
            return true;    // don't add reserved names

        init_minim_object(&it, MINIM_OBJ_AST, ast);  // wrap first
        match_table_add(table, MINIM_STRING(match_unbox), 0, it);
    }
    else
    {
        return false;
    }

    return true;
}

static SyntaxNode*
apply_transformation(MinimEnv *env, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *val;

    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            if (is_replace_pattern(ast, i))
            {
                val = env_get_sym(env, ast->children[i]->sym);
                if (val && minim_listp(val))
                {
                    size_t len = minim_list_length(val);
                    
                    if (len == 0)   // shrink by two
                    {
                        for (size_t j = i; j + 2 < ast->childc; ++j)
                            ast->children[j] = ast->children[j + 2];

                        ast->childc -= 2;
                        ast->children = GC_realloc(ast->children, ast->childc * sizeof(SyntaxNode*));
                        ++i;
                    }
                    else if (len == 1) // shrink by one
                    {
                        for (size_t j = i; j + 1 < ast->childc; ++j)
                            ast->children[j] = ast->children[j + 1];

                        ast->childc -= 1;
                        ast->children = GC_realloc(ast->children, ast->childc * sizeof(SyntaxNode*));
                        ast->children[i] = MINIM_AST(MINIM_CAR(val));
                        ++i;
                    }
                    else if (len == 2) // nothing
                    {
                        ast->children[i] = MINIM_AST(MINIM_CAR(val));
                        ast->children[i + 1] = MINIM_AST(MINIM_CADR(val));
                        i += 2;
                    }
                    else        // expand
                    {
                        MinimObject *it;
                        size_t new_size, j;

                        ast->children[i] = MINIM_AST(MINIM_CAR(val));
                        ast->children[i + 1] = MINIM_AST(MINIM_CADR(val));

                        it = MINIM_CDDR(val);
                        new_size = ast->childc + len - 2;
                        ast->children = GC_realloc(ast->children, new_size * sizeof(SyntaxNode*));
                        for (j = i + 2; j < ast->childc; ++j)
                        {
                            ast->children[j + new_size] = ast->children[j];
                            ast->children[j] = MINIM_AST(MINIM_CAR(it));
                            it = MINIM_CDR(it);
                        }

                        for (; j < new_size; ++j)
                        {
                            ast->children[j] = MINIM_AST(MINIM_CAR(it));
                            it = MINIM_CDR(it);
                        }
                        
                        ast->childc = new_size;
                        i += len;
                    }
                }
                else
                {
                    *perr = minim_error("not a pattern variable", NULL);
                    return NULL;
                }
            }
            else
            {
                ast->children[i] = apply_transformation(env, ast->children[i], perr);
            }
        }
    }
    else
    {
        val = env_get_sym(env, ast->sym);
        if (val)    return MINIM_AST(val);      // replace
    }

    return ast;
}

static SyntaxNode*
transform_loc(MinimEnv *env, MinimObject *trans, SyntaxNode *ast, MinimObject **perr)
{
    MinimObject *reserved_lst, *rule, *sym, *it;
    MatchTable table;
    SymbolList reserved;
    size_t rlen;

    unsyntax_ast(env, MINIM_TRANSFORM_AST(trans)->children[1], &reserved_lst);
    rlen = minim_list_length(reserved_lst);
    it = reserved_lst;

    init_match_table(&table);
    init_symbol_list(&reserved, rlen);
    for (size_t i = 0; i < rlen; ++i, it = MINIM_CDR(it))
    {
        unsyntax_ast(env, MINIM_AST(MINIM_CAR(it)), &sym);
        reserved.syms[i] = MINIM_STRING(sym);
    }

    for (size_t i = 2; i < MINIM_TRANSFORM_AST(trans)->childc; ++i)
    {
        SyntaxNode *match, *replace;
        MinimEnv *env2;

        init_env(&env2, NULL, NULL);  // isolated environment
        unsyntax_ast(env, MINIM_TRANSFORM_AST(trans)->children[i], &rule);

        match = MINIM_AST(MINIM_CAR(rule));
        copy_syntax_node(&replace, MINIM_AST(MINIM_CADR(rule)));

        // printf("match: "); print_ast(match); printf("\n");
        // printf("replace: "); print_ast(replace); printf("\n");

        if (match_transform(env2, match, ast, &table, &reserved, 0, 0))
        {
            ast = apply_transformation(env2, replace, perr);
            return (*perr) ? NULL : ast;
        }
    }

    *perr = minim_error("no matching syntax rule", MINIM_TRANSFORM_NAME(trans));
    return ast;
}

SyntaxNode* transform_syntax_rec(MinimEnv *env, SyntaxNode* ast, MinimObject **perr)
{
    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        MinimObject *op;

        if (ast->childc == 0)
        return ast;

        for (size_t i = 0; i < ast->childc; ++i)
        {
            ast->children[i] = transform_syntax_rec(env, ast->children[i], perr);
            if (*perr)   return ast;
        }

        if (ast->children[0]->sym)
        {
            op = env_get_sym(env, ast->children[0]->sym);
            if (op && MINIM_OBJ_TRANSFORMP(op))
            {
                MinimEnv *env2;

                init_env(&env2, NULL, NULL);
                ast = transform_loc(env2, op, ast, perr);
                if (*perr)   return ast;
            }
        }
    }

    return ast;
}

SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast, MinimObject **perr)
{
    *perr = NULL;   // NULL signals success 
    return transform_syntax_rec(env, ast, perr);
}

// ================================ Builtins ================================

MinimObject *minim_builtin_def_syntax(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *trans, *sym;

    unsyntax_ast(env, MINIM_AST(args[0]), &sym);
    init_minim_object(&trans, MINIM_OBJ_TRANSFORM, MINIM_STRING(sym), MINIM_AST(args[1]));
    env_intern_sym(env, MINIM_STRING(sym), trans);

    init_minim_object(&res, MINIM_OBJ_VOID);
    return res;
}

MinimObject *minim_builtin_syntax_rules(MinimEnv *env, MinimObject **args, size_t argc)
{
    return minim_error("should not execute here", "syntax-rules");
}
