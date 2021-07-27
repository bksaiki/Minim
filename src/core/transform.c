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
init_match_table(MatchTable *table)
{
    table->objs = NULL;
    table->depths = NULL;
    table->syms = NULL;
    table->size = 0;
}

#define clear_match_table(table)    init_match_table(table)

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

static MinimObject *
match_table_get(MatchTable *table, const char *sym)
{
    for (size_t i = 0; i < table->size; ++i)
    {
        if (strcmp(table->syms[i], sym) == 0)
            return table->objs[i];
    }

    return NULL;
}

static size_t
match_table_get_depth(MatchTable *table, const char *sym)
{
    for (size_t i = 0; i < table->size; ++i)
    {
        if (strcmp(table->syms[i], sym) == 0)
            return table->depths[i];
    }

    return 0;
}

static void
match_table_merge(MatchTable *parent, MatchTable *child)
{
    MinimObject *val;

    for (size_t i = 0; i < child->size; ++i)
    {
        val = match_table_get(child, child->syms[i]);
        match_table_add(parent, child->syms[i], child->depths[i], val);
    }

    clear_match_table(child);
}

static void
match_table_merge_patterns(MatchTable *parent, MatchTable *child)
{
    MinimObject *pair;

    if (child->size == 0)
        return;

    if (parent->size == 0)
    {
        for (size_t i = 0; i < child->size; ++i)
        {
            init_minim_object(&pair, MINIM_OBJ_PAIR, child->objs[i], NULL);
            match_table_add(parent, child->syms[i], child->depths[i], pair);
        }
    }
    else
    {
        for (size_t i = 0; i < child->size; ++i)
        {
            MINIM_TAIL(pair, match_table_get(parent, child->syms[i]));
            init_minim_object(&MINIM_CDR(pair), MINIM_OBJ_PAIR, child->objs[i], NULL);
        }
    }
}

static void
match_table_next_depth(MatchTable *dest, MatchTable *src, size_t idx)
{
    MinimObject *val;
    size_t depth;

    for (size_t i = 0; i < src->size; ++i)
    {
        depth = match_table_get_depth(src, src->syms[i]);
        if (depth > 0)
        {
            val = match_table_get(src, src->syms[i]);
            match_table_add(dest, src->syms[i], depth - 1, minim_list_ref(val, idx));
        }
    }
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

static void
minim_error_add_syntax(MinimError *err, SyntaxNode *in)
{
    Buffer *bf;

    init_buffer(&bf);
    ast_to_buffer(in, bf);
    init_minim_error_desc_table(&err->table, 1);
    minim_error_desc_table_set(err->table, 0, "in", get_buffer(bf));
}

static bool
is_match_pattern(SyntaxNode *parent, size_t idx)
{
    return (idx + 1 < parent->childc &&
            parent->children[idx + 1]->sym &&
            strcmp(parent->children[idx + 1]->sym, "...") == 0);
}

static void
add_null_variables(SyntaxNode *match, MatchTable *table, SymbolList *reserved, size_t pdepth)
{
    if (match->type == SYNTAX_NODE_LIST || match->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < match->childc; ++i)
            add_null_variables(match->children[i], table, reserved, pdepth);
    }
    else if (match->type == SYNTAX_NODE_PAIR)
    {
        add_null_variables(match->children[0], table, reserved, pdepth);
        add_null_variables(match->children[1], table, reserved, pdepth);
    }
    else //  match->type == SYNTAX_NODE_DATUM
    {
        MinimObject *null;

        if (strcmp(match->sym, "_") == 0 ||                 // wildcard
            strcmp(match->sym, ".") == 0 ||                 // dot operator
            strcmp(match->sym, "...") == 0 ||               // ellipse
            symbol_list_contains(reserved, match->sym))     // reserved
            return;

        init_minim_object(&null, MINIM_OBJ_PAIR, NULL, NULL);
        match_table_add(table, match->sym, pdepth, null);
    }
}

static bool
match_transform(SyntaxNode *match, SyntaxNode *ast, MatchTable *table,
                SymbolList *reserved, size_t pdepth)
{
    if (match->type == SYNTAX_NODE_LIST || match->type == SYNTAX_NODE_VECTOR)
    {
        size_t i, j;

        i = 0; j = 0;
        for (; i < match->childc; ++i)
        {
            if (j > ast->childc)    // match too long
                return false;

            if (j == ast->childc)   // match possibly problematic
            {
                if (is_match_pattern(match, i))
                {
                    add_null_variables(match->children[i], table, reserved, pdepth + 1);
                    return true;
                }
                else
                {
                    return false;
                }
            }

            if (is_match_pattern(match, i))
            {
                MatchTable table2;

                if (match->childc > ast->childc + 2)    // not enough space for on ast
                    return false;

                init_match_table(&table2);
                for (; j < ast->childc - (match->childc - i - 2); ++j)
                {
                    MatchTable table3;

                    init_match_table(&table3);
                    if (!match_transform(match->children[i], ast->children[j], &table3, reserved, pdepth + 1))
                        return false;

                    match_table_merge_patterns(&table2, &table3);
                }

                match_table_merge(table, &table2);
                ++i;    // skip ellipse
            }
            else if (!match_transform(match->children[i], ast->children[j], table, reserved, pdepth))
            {
                return false;
            }
            else
            {
                ++j;
            }
        }
    }
    else if (match->type == SYNTAX_NODE_PAIR)
    {
        return match_transform(match->children[0], ast->children[0], table, reserved, pdepth) &&
               match_transform(match->children[1], ast->children[1], table, reserved, pdepth);
    }
    else // match->type == SYNTAX_NODE_DATUM
    {
        MinimObject *val;

        if (strcmp(match->sym, "_") == 0)       // wildcard
            return true;

        if (strcmp(match->sym, ".") == 0)       // dot operator
            return strcmp(ast->sym, ".") == 0;

        if (symbol_list_contains(reserved, match->sym))
            return true;    // don't add reserved names

        init_minim_object(&val, MINIM_OBJ_AST, ast);  // wrap first
        match_table_add(table, match->sym, pdepth, val);
    }

    return true;
}

static size_t
pattern_length(MatchTable *table, SyntaxNode *ast)
{
    MinimObject *val;
    size_t ret;

    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            ret = pattern_length(table, ast->children[i]);
            if (ret != SIZE_MAX) return ret;
        }
    }
    else
    {
        val = match_table_get(table, ast->sym);
        return (val) ? minim_list_length(val) : SIZE_MAX;
    }

    return SIZE_MAX;
}

static SyntaxNode*
apply_transformation(MatchTable *table, SyntaxNode *ast)
{
    MinimObject *val;

    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            if (is_match_pattern(ast, i))
            {
                MatchTable table2;
                SyntaxNode *sub;
                size_t len;

                init_match_table(&table2);
                len = pattern_length(table, ast->children[i]);
                if (len == SIZE_MAX)
                {
                    printf("apply transforms: bad things happened\n");
                }

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
                    for (size_t j = i + 1; j + 1 < ast->childc; ++j)
                        ast->children[j] = ast->children[j + 1];

                    ast->childc -= 1;
                    ast->children = GC_realloc(ast->children, ast->childc * sizeof(SyntaxNode*));

                    match_table_next_depth(&table2, table, 0);
                    ast->children[i] = apply_transformation(&table2, ast->children[i]);

                    ++i;
                }
                else if (len == 2) // nothing
                {
                    copy_syntax_node(&sub, ast->children[i]);
                    match_table_next_depth(&table2, table, 0);
                    ast->children[i] = apply_transformation(&table2, ast->children[i]);
                    
                    clear_match_table(&table2);
                    match_table_next_depth(&table2, table, 1);
                    ast->children[i + 1] = apply_transformation(&table2, sub);

                    i += 2;
                }
                else        // expand
                {
                    SyntaxNode *tmp;
                    size_t new_size, j;

                    copy_syntax_node(&sub, ast->children[i]);
                    match_table_next_depth(&table2, table, 0);
                    ast->children[i] = apply_transformation(&table2, ast->children[i]);
                    
                    clear_match_table(&table2);
                    copy_syntax_node(&tmp, sub);
                    match_table_next_depth(&table2, table, 1);
                    ast->children[i + 1] = apply_transformation(&table2, tmp);

                    new_size = ast->childc + len - 2;
                    ast->children = GC_realloc(ast->children, new_size * sizeof(SyntaxNode*));
                    for (j = i + 2; j < ast->childc; ++j)
                    {
                        clear_match_table(&table2);
                        copy_syntax_node(&tmp, sub);
                        match_table_next_depth(&table2, table, j - i);
                        ast->children[j + new_size] = ast->children[j];
                        ast->children[j] = apply_transformation(&table2, tmp);
                    }

                    for (; j < new_size; ++j)
                    {
                        clear_match_table(&table2);
                        copy_syntax_node(&tmp, sub);
                        match_table_next_depth(&table2, table, j - i);
                        ast->children[j] = apply_transformation(&table2, tmp);
                    }
                    
                    ast->childc = new_size;
                    i += len;
                }
            }
            else
            {
                ast->children[i] = apply_transformation(table, ast->children[i]);
            }
        }
    }
    else if (ast->type == SYNTAX_NODE_PAIR)
    {
        ast->children[0] = apply_transformation(table, ast->children[0]);
        ast->children[1] = apply_transformation(table, ast->children[1]);
    }
    else
    {
        val = match_table_get(table, ast->sym);
        if (val)    return MINIM_AST(val);      // replace
    }

    return ast;
}

static SyntaxNode*
transform_loc(MinimObject *trans, SyntaxNode *ast, MinimObject **perr)
{
    MatchTable table;
    SymbolList reserved;
    SyntaxNode *transform;

    transform = MINIM_TRANSFORM_AST(trans);
    init_match_table(&table);
    init_symbol_list(&reserved, transform->children[1]->childc);
    for (size_t i = 0; i < transform->children[1]->childc; ++i)
        reserved.syms[i] = transform->children[1]->children[i]->sym;

    for (size_t i = 2; i < transform->childc; ++i)
    {
        SyntaxNode *rule, *replace;

        rule = transform->children[i];
        if (match_transform(rule->children[0], ast, &table, &reserved, 0))
        {
            copy_syntax_node(&replace, rule->children[1]);
            return apply_transformation(&table, replace);
        }
    }

    *perr = minim_error("no matching syntax rule", NULL);
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
                ast = transform_loc(op, ast, perr);
                if (*perr)   return ast;
            }
        }
    }

    return ast;
}

static bool
valid_matchp(SyntaxNode* match, MatchTable *table, SymbolList *reserved, size_t pdepth, MinimObject **perr)
{
    if (match->type == SYNTAX_NODE_LIST || match->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < match->childc; ++i)
        {
            if (is_match_pattern(match, i))
            {
                if (!valid_matchp(match->children[i], table, reserved, pdepth + 1, perr))
                    return false;

                ++i; // skip ellipse
            }
            else
            {
                if (!valid_matchp(match->children[i], table, reserved, pdepth, perr))
                    return false;
            }
        }
    }
    else if (match->type == SYNTAX_NODE_PAIR)
    {
        return valid_matchp(match->children[0], table, reserved, pdepth, perr) &&
               valid_matchp(match->children[1], table, reserved, pdepth, perr);
    }
    else // datum
    {
        if (strcmp(match->sym, "...") == 0)
        {
            *perr = minim_error("ellipse not allowed here", NULL);
            return false;
        }


        if (!symbol_list_contains(reserved, match->sym) && strcmp(match->sym, "_") != 0)
            match_table_add(table, match->sym, pdepth, NULL);
    }

    return true;
}

static bool
valid_replacep(SyntaxNode* replace, MatchTable *table, size_t pdepth, MinimObject **perr)
{
    if (replace->type == SYNTAX_NODE_LIST || replace->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < replace->childc; ++i)
        {
            if (is_match_pattern(replace, i))
            {
                if (!valid_replacep(replace->children[i], table, pdepth + 1, perr))
                    return false;
                
                ++i;    // skip ellipse
            }
            else
            {
                if (!valid_replacep(replace->children[i], table, pdepth, perr))
                    return false;
            }
        }
    }
    else if (replace->type == SYNTAX_NODE_PAIR)
    {
        return valid_replacep(replace->children[0], table, pdepth, perr) &&
               valid_replacep(replace->children[1], table, pdepth, perr);
    }
    else // datum
    {
        size_t depth = match_table_get_depth(table, replace->sym);

        if (depth != SIZE_MAX)  // in table
        {
            if (pdepth > depth)
            {
                *perr = minim_error("missing ellipse in pattern", NULL);
                return false;
            }

            if (pdepth < depth)
            {
                *perr = minim_error("too many ellipses in pattern", NULL);
                return false;
            }
        }
    }

    return true;
}

// ================================ Public ================================

SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast, MinimObject **perr)
{
    *perr = NULL;   // NULL signals success 
    return transform_syntax_rec(env, ast, perr);
}

bool valid_transformp(SyntaxNode *match, SyntaxNode *replace, MinimObject *reserved,  MinimObject **perr)
{
    MatchTable table;
    SymbolList reserved_lst;
    MinimObject *it;
    size_t reservedc;

    reservedc = minim_list_length(reserved);
    init_symbol_list(&reserved_lst, reservedc);
    it = reserved;
    for (size_t i = 0; i < reservedc; ++i, it = MINIM_CDR(it))
        reserved_lst.syms[i] = MINIM_AST(MINIM_CAR(it))->sym;

    init_match_table(&table);
    if (!valid_matchp(match, &table, &reserved_lst, 0, perr))
    {
        minim_error_add_syntax(MINIM_ERROR(*perr), match);
        return false;
    }

    if (!valid_replacep(replace, &table, 0, perr))
    {
        minim_error_add_syntax(MINIM_ERROR(*perr), replace);
        return false;
    }

    return true;
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
