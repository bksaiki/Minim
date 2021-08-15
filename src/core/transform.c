#include <string.h>

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"

// forward declaration
SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast);

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

    return SIZE_MAX;
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
            pair = minim_cons(child->objs[i], minim_null);
            match_table_add(parent, child->syms[i], child->depths[i], pair);
        }
    }
    else
    {
        for (size_t i = 0; i < child->size; ++i)
        {
            MINIM_TAIL(pair, match_table_get(parent, child->syms[i]));
            MINIM_CDR(pair) = minim_cons(child->objs[i], minim_null);
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
            if (idx < minim_list_length(val))
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
        if (strcmp(match->sym, "_") == 0 ||                 // wildcard
            strcmp(match->sym, ".") == 0 ||                 // dot operator
            strcmp(match->sym, "...") == 0 ||               // ellipse
            symbol_list_contains(reserved, match->sym))     // reserved
            return;

        match_table_add(table, match->sym, pdepth, minim_null);
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

        return j == ast->childc;
    }
    else if (match->type == SYNTAX_NODE_PAIR)
    {
        return match_transform(match->children[0], ast->children[0], table, reserved, pdepth) &&
               match_transform(match->children[1], ast->children[1], table, reserved, pdepth);
    }
    else // match->type == SYNTAX_NODE_DATUM
    {
        if (strcmp(match->sym, "_") == 0)       // wildcard
            return true;

        if (strcmp(match->sym, ".") == 0)       // dot operator
            return strcmp(ast->sym, ".") == 0;

        if (symbol_list_contains(reserved, match->sym)) // reserved name
            return ast_equalp(match, ast);

        match_table_add(table, match->sym, pdepth, minim_ast(ast)); // wrap first
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

    // printf("trans: "); print_ast(ast); printf("\n");
    // for (size_t i = 0; i < table->size; ++i)
    // {
    //     printf("[%zu] %s: ", table->depths[i], table->syms[i]);
    //     debug_print_minim_object(table->objs[i], NULL);
    // }

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
        if (val)    // replace
        {
            SyntaxNode *cp;

            copy_syntax_node(&cp, MINIM_AST(val));
            return cp;
        }
    }

    return ast;
}

static SyntaxNode*
transform_loc(MinimEnv *env, MinimObject *trans, SyntaxNode *ast)
{
    MinimObject *body, *res;

    body = minim_ast(ast);
    res = eval_lambda(MINIM_TRANSFORM_PROC(trans), NULL, 1, &body);
    if (!MINIM_OBJ_ASTP(res))
    {
        THROW(minim_syntax_error("expected syntax as a result",
                                 MINIM_TRANSFORM_NAME(trans),
                                 ast,
                                 NULL));
    }

    return MINIM_AST(res);
}

static bool
valid_matchp(SyntaxNode* match, MatchTable *table, SymbolList *reserved, size_t pdepth)
{
    if (match->type == SYNTAX_NODE_LIST || match->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < match->childc; ++i)
        {
            if (is_match_pattern(match, i))
            {
                if (!valid_matchp(match->children[i], table, reserved, pdepth + 1))
                    return false;

                ++i; // skip ellipse
            }
            else
            {
                if (!valid_matchp(match->children[i], table, reserved, pdepth))
                    return false;
            }
        }
    }
    else if (match->type == SYNTAX_NODE_PAIR)
    {
        return valid_matchp(match->children[0], table, reserved, pdepth) &&
               valid_matchp(match->children[1], table, reserved, pdepth);
    }
    else // datum
    {
        if (strcmp(match->sym, "...") == 0)
        {
            THROW(minim_syntax_error("ellipse not allowed here", NULL, match, NULL));
            return false;
        }


        if (!symbol_list_contains(reserved, match->sym) && strcmp(match->sym, "_") != 0)
            match_table_add(table, match->sym, pdepth, minim_null);
    }

    return true;
}

static bool
valid_replacep(SyntaxNode* replace, MatchTable *table, SymbolList *reserved, size_t pdepth)
{
    if (replace->type == SYNTAX_NODE_LIST || replace->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < replace->childc; ++i)
        {
            if (is_match_pattern(replace, i))
            {
                if (!valid_replacep(replace->children[i], table, reserved, pdepth + 1))
                    return false;
                
                ++i;    // skip ellipse
            }
            else
            {
                if (!valid_replacep(replace->children[i], table, reserved, pdepth))
                    return false;
            }
        }
    }
    else if (replace->type == SYNTAX_NODE_PAIR)
    {
        return valid_replacep(replace->children[0], table, reserved, pdepth) &&
               valid_replacep(replace->children[1], table, reserved, pdepth);
    }
    else // datum
    {
        if (!symbol_list_contains(reserved, replace->sym))
        {
            size_t depth = match_table_get_depth(table, replace->sym);

            if (depth != SIZE_MAX)  // in table
            {
                if (pdepth > depth)
                {
                    THROW(minim_syntax_error("missing ellipse in pattern", NULL, replace, NULL));
                    return false;
                }

                if (pdepth < depth)
                {
                    THROW(minim_syntax_error("too many ellipses in pattern", NULL, replace, NULL));
                    return false;
                }
            }
        }
    }

    return true;
}

static SyntaxNode *
replace_syntax(MinimEnv *env, SyntaxNode *ast, MatchTable *table)
{
    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        if (ast->type == SYNTAX_NODE_LIST &&
            ast->children[0]->sym &&
            strcmp(ast->children[0]->sym, "syntax") == 0)
        {
            ast->children[1] = apply_transformation(table, ast->children[1]);
            ast->children[1] = transform_syntax(env, ast->children[1]);
        }
        else
        {
            for (size_t i = 0; i < ast->childc; ++i)
                ast->children[i] = replace_syntax(env, ast->children[i], table);
        }
    }
    else if (ast->type == SYNTAX_NODE_PAIR)
    {
        ast->children[0] = replace_syntax(env, ast->children[0], table);
        ast->children[1] = replace_syntax(env, ast->children[1], table);
    }
    else
    {
        MinimObject *obj;

        obj = match_table_get(table, ast->sym);
        if (obj) THROW(minim_error("variable cannot be used outside of template", ast->sym));
    }

    return ast;
}

// ================================ Public ================================

SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast)
{
    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        MinimObject *op;

        if (ast->childc == 0)
            return ast;

        if (ast->children[0]->sym)
        {
            op = env_get_sym(env, ast->children[0]->sym);
            if (op)
            {
                if (MINIM_SYNTAX(op) == minim_builtin_syntax)
                    return ast;

                if (MINIM_OBJ_TRANSFORMP(op))
                    ast = transform_loc(env, op, ast);
            }
        }

        for (size_t i = 0; i < ast->childc; ++i)
            ast->children[i] = transform_syntax(env, ast->children[i]);
    }
    else if (ast->type == SYNTAX_NODE_DATUM)
    {
        MinimObject *val;

        if (ast->sym)
        {
            val = env_get_sym(env, ast->sym);
            if (val && MINIM_OBJ_TRANSFORMP(val))
                ast = transform_loc(env, val, ast);
        }
    }

    return ast;
}

void check_transform(SyntaxNode *match, SyntaxNode *replace, MinimObject *reserved)
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
    valid_matchp(match, &table, &reserved_lst, 0);
    valid_replacep(replace, &table, &reserved_lst, 0);
}

// ================================ Builtins ================================

MinimObject *minim_builtin_def_syntaxes(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val, *trans;

    eval_ast_no_check(env, MINIM_AST(args[1]), &val);
    if (!MINIM_OBJ_VALUESP(val))
    {
        if (MINIM_AST(args[0])->childc != 1)
            THROW(minim_values_arity_error("def-syntaxes", MINIM_AST(args[0])->childc,
                                            1, MINIM_AST(args[0])));
        
        if (!MINIM_OBJ_CLOSUREP(val))
            THROW(minim_syntax_error("expected a procedure of 1 argument",
                                      "def-syntaxes",
                                      MINIM_AST(args[1]),
                                      NULL));

        trans = minim_transform(MINIM_AST(args[0])->children[0]->sym, MINIM_CLOSURE(val));
        env_intern_sym(env, MINIM_AST(args[0])->children[0]->sym, trans);
    }
    else
    {
        if (MINIM_VALUES_SIZE(val) != MINIM_AST(args[0])->childc)
            THROW(minim_values_arity_error("def-syntaxes",
                                            MINIM_AST(args[0])->childc,
                                            MINIM_VALUES_SIZE(val),
                                            MINIM_AST(args[0])));

        for (size_t i = 0; i < MINIM_AST(args[0])->childc; ++i)
        {
            if (!MINIM_OBJ_CLOSUREP(MINIM_VALUES_REF(val, i)))
                THROW(minim_syntax_error("expected a procedure of 1 argument",
                                          "def-syntaxes",
                                          MINIM_AST(args[1]),
                                          NULL));

            trans = minim_transform(MINIM_AST(args[0])->children[i]->sym, MINIM_CLOSURE(MINIM_VALUES_REF(val, i)));
            env_intern_sym(env, MINIM_AST(args[0])->children[i]->sym, trans);
        }
    }

    return minim_void;
}

MinimObject *minim_builtin_syntax_case(MinimEnv *env, size_t argc, MinimObject **args)
{
    MatchTable table;
    SymbolList reserved;
    MinimObject *ast0;

    eval_ast_no_check(env, MINIM_AST(args[0]), &ast0);
    if (!ast0)
        THROW(minim_error("unknown identifier", MINIM_AST(args[0])->sym));

    if (!MINIM_OBJ_ASTP(ast0))
        THROW(minim_argument_error("syntax?", "syntax-case", 0, ast0));
    
    init_symbol_list(&reserved, MINIM_AST(args[1])->childc);
    for (size_t i = 0; i < MINIM_AST(args[1])->childc; ++i)
        reserved.syms[i] = MINIM_AST(args[1])->children[i]->sym;

    for (size_t i = 2; i < argc; ++i)
    {
        SyntaxNode *rule, *replace;
        MinimObject *res;

        init_match_table(&table);
        rule = MINIM_AST(args[i]);
        if (match_transform(rule->children[0], MINIM_AST(ast0), &table, &reserved, 0))
        {
            copy_syntax_node(&replace, rule->children[1]);
            replace = replace_syntax(env, replace, &table);
            eval_ast_no_check(env, replace, &res);
            return res;
        }
    }

    THROW(minim_syntax_error("bad syntax", "?", MINIM_AST(ast0), NULL));
    return NULL;
}
