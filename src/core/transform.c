#include <string.h>

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "syntax.h"

#define transform_type(x)   (MINIM_OBJ_CLOSUREP(x) ? MINIM_TRANSFORM_MACRO : MINIM_TRANSFORM_UNKNOWN)

// forward declaration
SyntaxNode* transform_syntax(MinimEnv *env, SyntaxNode* ast);

// ================================ string classification ================================

// TODO: this is copied from src/core/eval.c

static bool is_rational(char *str)
{
    char *it = str;

    if ((*it == '+' || *it == '-') &&
        (*(it + 1) >= '0' && *(it + 1) <= '9'))
    {
        it += 2;
    }

    while (*it >= '0' && *it <= '9')    ++it;

    if (*it == '/' && *(it + 1) >= '0' && *(it + 1) <= '9')
    {
        it += 2;
        while (*it >= '0' && *it <= '9')    ++it;
    }

    return (*it == '\0');
}

static bool is_float(const char *str)
{
    size_t idx = 0;
    bool digit = false;

    if (str[idx] == '+' || str[idx] == '-')
        ++idx;
    
    if (str[idx] >= '0' && str[idx] <= '9')
    {
        ++idx;
        digit = true;
    }
    
    while (str[idx] >= '0' && str[idx] <= '9')
        ++idx;

    if (str[idx] != '.' && str[idx] != 'e')
        return false;

    if (str[idx] == '.')
        ++idx;

    if (str[idx] >= '0' && str[idx] <= '9')
    {
        ++idx;
        digit = true;
    }

    while (str[idx] >= '0' && str[idx] <= '9')
        ++idx;

    if (str[idx] == 'e')
    {
        ++idx;
        if (!str[idx])  return false;

        if ((str[idx] == '+' || str[idx] == '-') &&
            str[idx + 1] >= '0' && str[idx + 1] <= '9')
            idx += 2;

        while (str[idx] >= '0' && str[idx] <= '9')
            ++idx;
    }

    return digit && !str[idx];
}

static bool is_char(char *str)
{
    return (str[0] == '#' && str[1] == '\\' && str[2] != '\0');
}

static bool is_str(char *str)
{
    size_t len = strlen(str);

    if (len < 2 || str[0] != '\"' || str[len - 1] != '\"')
        return false;

    for (size_t i = 1; i < len; ++i)
    {
        if (str[i] == '\"' && str[i - 1] != '\\' && i + 1 != len)
            return false;
    }

    return true;
}

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
    // no add
    if (strcmp(sym, "...") == 0 ||   // ellipse
        strcmp(sym, ".") == 0 ||     // dot
        strcmp(sym, "_") == 0)       // wildcard
        return;


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

static size_t
ellipse_pos(SyntaxNode *ast)
{
    for (size_t i = 0; i < ast->childc; ++i)
    {
        if (ast->children[i]->sym && strcmp(ast->children[i]->sym, "...") == 0)
            return i;
    }

    return 0;
}

static bool
is_improper_list(SyntaxNode *ast)
{
    return (ast->type == SYNTAX_NODE_LIST &&
            ast->childc > 2 &&
            ast->children[ast->childc - 2]->sym &&
            strcmp(ast->children[ast->childc - 2]->sym, ".") == 0);
}

static bool
is_improper_list_end(SyntaxNode *ast, size_t i)
{
    return (i + 3 == ast->childc &&
            ast->children[i + 1]->sym &&
            strcmp(ast->children[i + 1]->sym, ".") == 0);
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
        if (!symbol_list_contains(reserved, match->sym))     // reserved
            match_table_add(table, match->sym, pdepth, minim_null);     
    }
}

static MinimObject *merge_pattern(MinimObject *a, MinimObject *b)
{
    MinimObject *t;

    printf("merge L: "); debug_print_minim_object(MINIM_CAR(MINIM_TRANSFORMER(a)), NULL);
    printf("merge R: "); debug_print_minim_object(MINIM_CAR(MINIM_TRANSFORMER(b)), NULL);

    MINIM_TAIL(t, MINIM_CAR(MINIM_TRANSFORMER(a)));
    MINIM_CDR(t) = minim_cons(MINIM_CAR(MINIM_TRANSFORMER(b)), minim_null);
    return a;
}

static MinimObject *add_pattern(MinimObject *a)
{
    MinimObject *val, *depth;

    val = MINIM_CAR(MINIM_TRANSFORMER(a));
    depth = MINIM_CDR(MINIM_TRANSFORMER(a));
    return minim_transform(minim_cons(minim_list(&val, 1), depth), MINIM_TRANSFORM_PATTERN);
}

static void debug_pattern(MinimObject *o)
{
    if (!MINIM_OBJ_TRANSFORMP(o))
        printf("Not a pattern!");

    debug_print_minim_object(MINIM_TRANSFORMER(o), NULL);
}

static bool
match_transform2(MinimEnv *env, SyntaxNode *match, MinimObject *thing, SymbolList *reserved, size_t pdepth)
{
    MinimObject *datum;

    datum = MINIM_OBJ_ASTP(thing) ? unsyntax_ast(env, MINIM_AST(thing)) : thing;

    printf("match: "); print_ast(match); printf("\n");
    printf("datum: "); debug_print_minim_object(datum, env);

    if (match->type == SYNTAX_NODE_PAIR)        // pairs first
    {
        if (MINIM_OBJ_PAIRP(datum))
        {
            return match_transform2(env, match->children[0], MINIM_CAR(datum), reserved, pdepth) &&
                   match_transform2(env, match->children[1], MINIM_CDR(datum), reserved, pdepth);

        }
        else
        {
            return false;
        }
    }
    else if (match->type == SYNTAX_NODE_DATUM)
    {
        MinimObject *val, *pattern;

        if (strcmp(match->sym, "_") == 0)       // wildcard
            return true;

        if (symbol_list_contains(reserved, match->sym) ||       // reserved name
            is_rational(match->sym) || is_float(match->sym) ||  // number
            is_str(match->sym) ||                               // string
            is_char(match->sym))                                // character
        {
            val = unsyntax_ast(env, match);
            return minim_equalp(val, datum);
        }

        pattern = env_get_sym(env, match->sym);
        if (pattern && MINIM_OBJ_TRANSFORMP(pattern) &&
            MINIM_TRANSFORM_TYPE(pattern) == MINIM_TRANSFORM_PATTERN)
        {
            return minim_equalp(MINIM_TRANSFORMER(pattern), datum);
        }
        
        val = minim_transform(minim_cons(datum, (void*) pdepth), MINIM_TRANSFORM_PATTERN);
        env_intern_sym(env, match->sym, val);
        return true;
    }
    else if (match->type == SYNTAX_NODE_VECTOR)
    {
        size_t ell_pos;

        if (!MINIM_OBJ_VECTORP(datum))
            return false;

        ell_pos = ellipse_pos(match);
        if (ell_pos != 0)     // much more complicated to do this
        {
            printf("matching vectors: not implemented");
            return false;
        }
        else
        {
            if (match->childc != MINIM_VECTOR_LEN(datum))
                return false;

            for (size_t i = 0; i < match->childc; ++i)
            {
                if (!match_transform2(env, match->children[i], MINIM_VECTOR_REF(datum, i),
                                      reserved, pdepth))
                    return false;
            }
        }
    }
    else // match->type == SYNTAX_NODE_LIST
    {
        size_t ell_pos;

        if (!MINIM_OBJ_PAIRP(datum))
            return false;
        
        ell_pos = ellipse_pos(match);
        if (ell_pos != 0)     // much more complicated to do this
        {
            MinimObject *it, *ell_it, *after_it;
            size_t before, after, len, i;

            len = minim_list_length(datum);
            before = ell_pos - 1;
            after = match->childc - ell_pos - 1;

            if (len < before + after)   // not enough space
                return false;

            // try matching front
            it = datum;
            for (i = 0; i < before; ++i, it = MINIM_CDR(it))
            {
                if (!match_transform2(env, match->children[i], MINIM_CAR(it), reserved, pdepth))
                    return false;
            }

            // try matching pattern w/ ellipse
            MINIM_CDNR(ell_it, datum, j, before);
            MINIM_CDNR(after_it, datum, j, len - after);
            if (len == before + after)
            {
                // Bind null
                return true;
            }
            else
            {
                MinimEnv *env2;

                init_env(&env2, env, NULL);
                for (; ell_it != after_it; ell_it = MINIM_CDR(ell_it))
                {
                    MinimEnv *env3;

                    init_env(&env3, env, NULL);
                    if (!match_transform2(env3, match->children[ell_pos - 1], MINIM_CAR(ell_it),
                                          reserved, pdepth + 1))
                        return false;

                    minim_symbol_table_merge2(env2->table, env3->table, merge_pattern, add_pattern);
                }

                minim_symbol_table_merge(env->table, env2->table);
            }

            // try matching end
            for (i = ell_pos + 1; i < match->childc; ++i)
            {
                 if (!match_transform2(env, match->children[i], MINIM_CAR(after_it), reserved, pdepth))
                    return false;

                after_it = MINIM_CDR(after_it);
            }

        }
        else
        {
            MinimObject *it = datum;

            for (size_t i = 0; i < match->childc; ++i, it = MINIM_CDR(it))
            {
                if (minim_nullp(it) || !MINIM_OBJ_PAIRP(it))    // fell off list
                    return false;

                if (is_improper_list_end(match, i))
                {
                    return match_transform2(env, match->children[i], MINIM_CAR(it), reserved, pdepth) &&
                           match_transform2(env, match->children[i + 2], MINIM_CDR(it), reserved, pdepth);
                }

                if (!match_transform2(env, match->children[i], MINIM_CAR(it), reserved, pdepth))
                    return false;
            }
        }

        return true;
    }

    return false;
}

static bool
match_transform(SyntaxNode *match, SyntaxNode *ast, MatchTable *table,
                SymbolList *reserved, size_t pdepth)
{
    if (match->type == SYNTAX_NODE_PAIR)    // pairs first
    {
        if (ast->type == SYNTAX_NODE_PAIR)
        {
            return match_transform(match->children[0], ast->children[0], table, reserved, pdepth) &&
                   match_transform(match->children[1], ast->children[1], table, reserved, pdepth);
        }

        if (ast->type != SYNTAX_NODE_LIST || ast->childc < 1)
            return false;

        // match car
        if (!match_transform(match->children[0], ast->children[0], table, reserved, pdepth))
            return false;

        // match cdr
        if (ast->childc == 1)
        { 
            match_table_add(table, match->children[1]->sym, 0, minim_null);
        }
        else
        {
            SyntaxNode *rest;

            if (ast->childc == 4 && ast->children[2]->sym && strcmp(ast->children[2]->sym, ".") == 0)
            {
                init_syntax_node(&rest, SYNTAX_NODE_PAIR);
                rest->childc = 2;
                rest->children = GC_alloc(rest->childc * sizeof(SyntaxNode*));
                rest->children[0] = ast->children[1];
                rest->children[1] = ast->children[3];
            }
            else
            {
                init_syntax_node(&rest, SYNTAX_NODE_LIST);
                rest->childc = ast->childc - 1;
                rest->children = GC_alloc(rest->childc * sizeof(SyntaxNode*));
                for (size_t i = 0; i < rest->childc; ++i)
                    rest->children[i] = ast->children[i + 1];
            }
            
            if (!match_transform(match->children[1], rest, table, reserved, pdepth))
                    return false;
        }

        return true;
    }
    else if (match->type == SYNTAX_NODE_DATUM)
    {
        if (strcmp(match->sym, "_") == 0)       // wildcard
            return true;

        if (strcmp(match->sym, ".") == 0)       // dot operator
            return strcmp(ast->sym, ".") == 0;

        if (symbol_list_contains(reserved, match->sym)) // reserved name
            return ast_equalp(match, ast);

        if (is_rational(match->sym) || is_float(match->sym) ||
            is_char(match->sym) || is_str(match->sym))
            return ast->sym && strcmp(match->sym, ast->sym) == 0;

        match_table_add(table, match->sym, pdepth, minim_ast(ast)); // wrap first
        return true;
    }
    else if (match->type == SYNTAX_NODE_LIST && is_improper_list(ast))
    {
        return false;
    }
    else if ((match->type == SYNTAX_NODE_LIST && ast->type == SYNTAX_NODE_LIST) ||
             (match->type == SYNTAX_NODE_VECTOR && ast->type == SYNTAX_NODE_VECTOR))
    {
        size_t i, j;

        if (ast->type == SYNTAX_NODE_DATUM)
            return false;

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

                if (match->childc + j > ast->childc + i + 2)    // not enough space for on ast
                    return false;

                if (match->childc + j == ast->childc + i + 2)
                {
                    add_null_variables(match->children[i], table, reserved, pdepth);
                }
                else
                {
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
                }

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

    return false;
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
        size_t i = 0;

        while (i < ast->childc)
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
                    printf(" at: "); print_ast(ast); printf("\n");
                    printf(" in: "); print_ast(ast->children[i]); printf("\n");
                    THROW(NULL, minim_error("PANIC!!", NULL));
                }

                if (len == 0)   // shrink by two
                {
                    for (size_t j = i; j + 2 < ast->childc; ++j)
                        ast->children[j] = ast->children[j + 2];

                    ast->childc -= 2;
                    ast->children = GC_realloc(ast->children, ast->childc * sizeof(SyntaxNode*));
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
                ++i;
            }
        }
    }
    else if (ast->type == SYNTAX_NODE_PAIR)
    {
        SyntaxNode *car, *cdr;
        
        car = apply_transformation(table, ast->children[0]);
        cdr = apply_transformation(table, ast->children[1]);
        if (cdr->type == SYNTAX_NODE_LIST)
        {
            ast->type = SYNTAX_NODE_LIST;
            ast->childc = cdr->childc + 1;
            ast->children = GC_alloc(ast->childc * sizeof(SyntaxNode*));
            ast->children[0] = car;
            for (size_t i = 1; i < ast->childc; ++i)
                ast->children[i] = cdr->children[i - 1];
        }
        else
        {
            ast->children[0] = car;
            ast->children[1] = cdr;
        }
    }
    else
    {
        SyntaxNode *cp;

        val = match_table_get(table, ast->sym);
        if (val == minim_null)
        {
            init_syntax_node(&cp, SYNTAX_NODE_LIST);
            return cp;
        }
        else if (val != NULL)    // replace
        {
            copy_syntax_node(&cp, MINIM_AST(val));
            return cp;
        }
    }

    return ast;
}

static void get_patterns(MinimEnv *env, SyntaxNode *ast, MinimObject *patterns)
{
    if (ast->childc > 0)
    {
        for (size_t i = 0; i < ast->childc; ++i)
            get_patterns(env, ast->children[i], patterns);
    }
    else if (ast->sym)
    {
        MinimObject *val;

        if (strcmp(ast->sym, "...") == 0 || strcmp(ast->sym, ".") == 0)     // early exit
            return;

        val = env_get_sym(env, ast->sym);
        if (MINIM_OBJ_TRANSFORMP(val) && MINIM_TRANSFORM_TYPE(val) == MINIM_TRANSFORM_PATTERN)
        {
            for (size_t i = 0; i < MINIM_VECTOR_LEN(patterns); ++i)
            {
                if (strcmp(MINIM_STRING(MINIM_CAR(MINIM_VECTOR_REF(patterns, i))), ast->sym) == 0)
                    return; // already in list
            }

            ++MINIM_VECTOR_LEN(patterns);
            MINIM_VECTOR(patterns) = GC_realloc(MINIM_VECTOR(patterns),
                                                MINIM_VECTOR_LEN(patterns) * sizeof(MinimObject*));
            MINIM_VECTOR_REF(patterns, MINIM_VECTOR_LEN(patterns) - 1) =
                minim_cons(minim_string(ast->sym), val);
        }
    }
}

static size_t
p_length(MinimObject *patterns)
{
    MinimObject *val;

    for (size_t i = 0; i < MINIM_VECTOR_LEN(patterns); ++i)
    {
        val = MINIM_TRANSFORMER(MINIM_VECTOR_REF(patterns, i));
        if (minim_listp(val))
            return minim_list_length(val);
    }

    // Panic!!
    THROW(NULL, minim_error("template contains no pattern variable before ellipse", "syntax-case"));
    return 0;
}

static SyntaxNode *
apply_transformation2(MinimEnv *env, SyntaxNode *ast, MinimObject *patterns)
{
    SyntaxNode *ret;

    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        init_syntax_node(&ret, ast->type);
        ret->childc = ast->childc;
        ret->children = GC_alloc(ret->childc * sizeof(SyntaxNode*));

        for (size_t i = 0; i < ret->childc; ++i)
        {
            if (i + 2 == ret->childc && ast->children[i]->sym &&            // dot
                strcmp(ast->children[i]->sym, ".") == 0)
            {
                copy_syntax_node(&ret->children[i], ast->children[i]);
            }
            else if (i + 1 < ret->childc && ast->children[i + 1]->sym &&   // ellipse
                     strcmp(ast->children[i + 1]->sym, "...") == 0)
            {
                size_t plen, pc;
                
                plen = p_length(patterns);
                pc = MINIM_VECTOR_LEN(patterns);
                for (size_t i = 0; i < plen; ++i)
                {
                    MinimObject *next_patterns, *k, *v, *val;

                    next_patterns = minim_vector(pc, GC_alloc(pc * sizeof(MinimObject*)));
                    for (size_t j = 0; i < pc; ++i)
                    {
                        k = MINIM_CAR(MINIM_VECTOR_REF(patterns, j));
                        v = MINIM_CDR(MINIM_VECTOR_REF(patterns, j));
                        val = MINIM_TRANSFORMER(v);
                        if (minim_listp(val))
                        {
                            MINIM_VECTOR_REF(next_patterns, j) = minim_cons(k, v);
                        }
                        else
                        {
                            v = minim_transform(minim_list_ref(val, i), MINIM_TRANSFORM_PATTERN);
                            MINIM_VECTOR_REF(next_patterns, j) = minim_cons(k, v);
                        }
                    }
                }
                
                THROW(env, minim_error("bad", NULL));
            }
            else        // normal
            {
                ret->children[i] = apply_transformation2(env, ast->children[i], patterns);
            }
        }
    }
    else if (ast->type == SYNTAX_NODE_PAIR)
    {
        SyntaxNode *car, *cdr;

        car = apply_transformation2(env, ast->children[0], patterns);
        cdr = apply_transformation2(env, ast->children[1], patterns);
        if (cdr->type == SYNTAX_NODE_LIST)
        {
            ++cdr->childc;
            cdr->children = GC_alloc(cdr->childc * sizeof(SyntaxNode*));
            for (size_t i = cdr->childc - 2; i > 0; --i)
                cdr->children[i + 1] = cdr->children[i];
            cdr->children[0] = car;
            ret = cdr;
        }
        else
        {
            init_syntax_node(&ret, SYNTAX_NODE_PAIR);
            ret->childc = 2;
            ret->children = GC_alloc(ret->childc * sizeof(SyntaxNode*));
            ret->children[0] = car;
            ret->children[1] = cdr;
        }
    }
    else  // datum
    {
        MinimObject *v = env_get_sym(env, ast->sym);

        if (v && MINIM_OBJ_TRANSFORMP(v) && MINIM_TRANSFORM_TYPE(v) == MINIM_TRANSFORM_PATTERN)
            ret = datum_to_syntax(env, MINIM_TRANSFORMER(v));
        else
            copy_syntax_node(&ret, ast);
    }

    return ret;
}

static SyntaxNode*
transform_loc(MinimEnv *env, MinimObject *trans, SyntaxNode *ast)
{
    MinimObject *body, *res;

    if (MINIM_TRANSFORM_TYPE(trans) != MINIM_TRANSFORM_MACRO)
        THROW(env, minim_syntax_error("illegal use of syntax transformer", ast->sym, ast, NULL));

    body = minim_ast(ast);
    res = eval_lambda(MINIM_CLOSURE(MINIM_TRANSFORMER(trans)), NULL, 1, &body);
    if (!MINIM_OBJ_ASTP(res))
        THROW(env, minim_syntax_error("expected syntax as a result", ast->sym, ast, NULL));

    return MINIM_AST(res);
}

static bool
valid_matchp(MinimEnv *env, SyntaxNode* match, MatchTable *table, SymbolList *reserved, size_t pdepth)
{
    if (match->type == SYNTAX_NODE_LIST || match->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < match->childc; ++i)
        {
            if (is_match_pattern(match, i))
            {
                if (!valid_matchp(env, match->children[i], table, reserved, pdepth + 1))
                    return false;

                ++i; // skip ellipse
            }
            else
            {
                if (!valid_matchp(env, match->children[i], table, reserved, pdepth))
                    return false;
            }
        }
    }
    else if (match->type == SYNTAX_NODE_PAIR)
    {
        return valid_matchp(env, match->children[0], table, reserved, pdepth) &&
               valid_matchp(env, match->children[1], table, reserved, pdepth);
    }
    else // datum
    {
        if (strcmp(match->sym, "...") == 0)
        {
            THROW(env, minim_syntax_error("ellipse not allowed here", NULL, match, NULL));
            return false;
        }
        
        if (!symbol_list_contains(reserved, match->sym) && strcmp(match->sym, "_") != 0)
            match_table_add(table, match->sym, pdepth, minim_null);
    }

    return true;
}

static bool
valid_replacep(MinimEnv *env, SyntaxNode* replace, MatchTable *table, SymbolList *reserved, size_t pdepth)
{
    if (replace->type == SYNTAX_NODE_LIST || replace->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < replace->childc; ++i)
        {
            if (is_match_pattern(replace, i))
            {
                if (!valid_replacep(env, replace->children[i], table, reserved, pdepth + 1))
                    return false;
                
                ++i;    // skip ellipse
            }
            else
            {
                if (!valid_replacep(env, replace->children[i], table, reserved, pdepth))
                    return false;
            }
        }
    }
    else if (replace->type == SYNTAX_NODE_PAIR)
    {
        return valid_replacep(env, replace->children[0], table, reserved, pdepth) &&
               valid_replacep(env, replace->children[1], table, reserved, pdepth);
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
                    THROW(env, minim_syntax_error("missing ellipse in pattern", NULL, replace, NULL));
                    return false;
                }

                if (pdepth < depth)
                {
                    THROW(env, minim_syntax_error("too many ellipses in pattern", NULL, replace, NULL));
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
        if (ast->childc == 0)   // early exit
            return ast;

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
        if (obj) THROW(env, minim_error("variable cannot be used outside of template", ast->sym));
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

void check_transform(MinimEnv *env, SyntaxNode *match, SyntaxNode *replace, MinimObject *reserved)
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
    valid_matchp(env, match, &table, &reserved_lst, 0);
    valid_replacep(env, replace, &table, &reserved_lst, 0);
}

// ================================ Builtins ================================

MinimObject *minim_builtin_def_syntaxes(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val, *trans;

    val = eval_ast_no_check(env, MINIM_AST(args[1]));
    if (!MINIM_OBJ_VALUESP(val))
    {
        if (MINIM_AST(args[0])->childc != 1)
            THROW(env, minim_values_arity_error("def-syntaxes", MINIM_AST(args[0])->childc,
                                            1, MINIM_AST(args[0])));
        
        if (!MINIM_OBJ_CLOSUREP(val))
            THROW(env, minim_syntax_error("expected a procedure of 1 argument",
                                      "def-syntaxes",
                                      MINIM_AST(args[1]),
                                      NULL));

        trans = minim_transform(val, transform_type(val));
        env_intern_sym(env, MINIM_AST(args[0])->children[0]->sym, trans);
    }
    else
    {
        if (MINIM_VALUES_SIZE(val) != MINIM_AST(args[0])->childc)
            THROW(env, minim_values_arity_error("def-syntaxes",
                                            MINIM_AST(args[0])->childc,
                                            MINIM_VALUES_SIZE(val),
                                            MINIM_AST(args[0])));

        for (size_t i = 0; i < MINIM_AST(args[0])->childc; ++i)
        {
            if (!MINIM_OBJ_CLOSUREP(MINIM_VALUES_REF(val, i)))
                THROW(env, minim_syntax_error("expected a procedure of 1 argument",
                                          "def-syntaxes",
                                          MINIM_AST(args[1]),
                                          NULL));

            trans = minim_transform(MINIM_VALUES_REF(val, i), transform_type(MINIM_VALUES_REF(val, i)));
            env_intern_sym(env, MINIM_AST(args[0])->children[i]->sym, trans);
        }
    }

    return minim_void;
}

static void debug_pattern_table(MinimEnv *env, MinimObject *args)
{
    PrintParams pp;
    MinimObject *sym, *val, *pat;
    size_t depth;

    set_default_print_params(&pp);
    for (size_t i = 0; i < MINIM_VECTOR_LEN(args); ++i)
    {
        sym = MINIM_CAR(MINIM_VECTOR_REF(args, i));
        pat = MINIM_TRANSFORMER(MINIM_CDR(MINIM_VECTOR_REF(args, i)));
        val = MINIM_CAR(pat);
        depth = (size_t) MINIM_CDR(pat);

        printf("%s[%zu]: ", MINIM_SYMBOL(sym), depth);
        print_minim_object(val, env, &pp);
        printf("\n");
    }
}

MinimObject *minim_builtin_template(MinimEnv *env, size_t argc, MinimObject **args)
{
    // Stores pattern data in a vector where each element
    // is a pair => (name, val)
    MinimObject *patterns;
    
    patterns = minim_vector(0, NULL);
    get_patterns(env, MINIM_AST(args[0]), patterns);

    debug_pattern_table(env, patterns);

    return minim_ast(apply_transformation2(env, MINIM_AST(args[0]), patterns));
}

MinimObject *minim_builtin_syntax_case(MinimEnv *env, size_t argc, MinimObject **args)
{
    SymbolList reserved;
    MinimObject *datum;

    init_symbol_list(&reserved, MINIM_AST(args[1])->childc);
    for (size_t i = 0; i < MINIM_AST(args[1])->childc; ++i)
        reserved.syms[i] = MINIM_AST(args[1])->children[i]->sym;

    datum = eval_ast_no_check(env, MINIM_AST(args[0]));
    for (size_t i = 2; i < argc; ++i)
    {
        SyntaxNode *match, *replace;
        MinimEnv *match_env;

        init_env(&match_env, NULL, NULL);       // empty 
        match = MINIM_AST(args[i])->children[0];
        replace = MINIM_AST(args[i])->children[1];
        if (match_transform2(match_env, match, datum, &reserved, 0))
        {
            MinimEnv *env2;

            printf("[Match]\n");

            init_env(&env2, env, NULL);
            minim_symbol_table_merge(env2->table, match_env->table);
            return eval_ast_no_check(env2, replace);
        }
    }

    THROW(env, minim_syntax_error("bad syntax", NULL, MINIM_AST(args[0]), NULL));
    return NULL;
}
