#include <string.h>

#include "../gc/gc.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "list.h"
#include "string.h"
#include "syntax.h"

#define transform_type(x)   (MINIM_OBJ_CLOSUREP(x) ? MINIM_TRANSFORM_MACRO : MINIM_TRANSFORM_UNKNOWN)

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
is_improper_list_end(SyntaxNode *ast, size_t i)
{
    return (i + 3 == ast->childc &&
            ast->children[i + 1]->sym &&
            strcmp(ast->children[i + 1]->sym, ".") == 0);
}

static void
add_null_variables(MinimEnv *env, SyntaxNode *ast, SymbolList *reserved, size_t pdepth)
{
    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        for (size_t i = 0; i < ast->childc; ++i)
        {
            if (i + 1 < ast->childc && ast->children[i + 1]->sym &&
                strcmp(ast->children[i + 1]->sym, "...") == 0)
            {
                add_null_variables(env, ast->children[i], reserved, pdepth + 1);
                ++i;
            }
            else
            {
                add_null_variables(env, ast->children[i], reserved, pdepth);
            }
        }
    }
    else if (ast->type == SYNTAX_NODE_PAIR)
    {
        add_null_variables(env, ast->children[0], reserved, pdepth);
        add_null_variables(env, ast->children[1], reserved, pdepth);
    }
    else
    {
        MinimObject *val;

        if (symbol_list_contains(reserved, ast->sym) ||         // reserved name
            is_rational(ast->sym) || is_float(ast->sym) ||      // number
            is_str(ast->sym) ||                                 // string
            is_char(ast->sym) ||                                // character
            strcmp(ast->sym, "...") == 0 ||                     // ellipse
            strcmp(ast->sym, "_") == 0 ||                       // wildcard
            strcmp(ast->sym, ".") == 0)                         // dot
            return;
        
        val = env_get_sym(env, ast->sym);
        if (val && MINIM_OBJ_TRANSFORMP(val))       // already bound
            return;

        env_intern_sym(env, ast->sym,
                       minim_transform(minim_cons(minim_null, (void*) (pdepth + 1)),
                                                  MINIM_TRANSFORM_PATTERN));
    }
}

static MinimObject *merge_pattern(MinimObject *a, MinimObject *b)
{
    MinimObject *t;

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

// static void debug_pattern(MinimObject *o)
// {
//     if (!MINIM_OBJ_TRANSFORMP(o))
//         printf("Not a pattern!");

//     debug_print_minim_object(MINIM_TRANSFORMER(o), NULL);
// }

// static void debug_pattern_table(MinimEnv *env, MinimObject *args)
// {
//     PrintParams pp;
//     MinimObject *sym, *val, *pat;
//     size_t depth;

//     set_default_print_params(&pp);
//     for (size_t i = 0; i < MINIM_VECTOR_LEN(args); ++i)
//     {
//         sym = MINIM_CAR(MINIM_VECTOR_REF(args, i));
//         pat = MINIM_TRANSFORMER(MINIM_CDR(MINIM_VECTOR_REF(args, i)));
//         val = MINIM_CAR(pat);
//         depth = (size_t) MINIM_CDR(pat);

//         printf("%s[%zu]: ", MINIM_SYMBOL(sym), depth);
//         debug_print_minim_object(val, NULL);
//     }
// }

static bool
match_transform(MinimEnv *env, SyntaxNode *match, MinimObject *thing, SymbolList *reserved, size_t pdepth)
{
    MinimObject *datum;

    // printf("match: "); print_ast(match); printf("\n");
    // printf("ast:   "); debug_print_minim_object(thing, NULL);

    datum = MINIM_OBJ_ASTP(thing) ? unsyntax_ast(env, MINIM_AST(thing)) : thing;
    if (match->type == SYNTAX_NODE_PAIR)        // pairs first
    {
        if (MINIM_OBJ_PAIRP(datum))
        {
            return match_transform(env, match->children[0], MINIM_CAR(datum), reserved, pdepth) &&
                   match_transform(env, match->children[1], MINIM_CDR(datum), reserved, pdepth);

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

        if (match->childc == 0)
            return MINIM_VECTOR_LEN(datum) == 0;

        ell_pos = ellipse_pos(match);
        if (ell_pos != 0)     // much more complicated to do this
        {
            size_t before, after, len;

            len = MINIM_VECTOR_LEN(datum);
            before = ell_pos - 1;
            after = match->childc - ell_pos - 1;

            if (len < before + after)   // not enough space
                return false;

            // try matching front
            for (size_t i = 0; i < before; ++i)
            {
                if (!match_transform(env, match->children[i], MINIM_VECTOR_REF(datum, i), reserved, pdepth))
                    return false;
            }

            if (len == before + after)
            {
                // Bind null
                add_null_variables(env, match->children[ell_pos - 1], reserved, pdepth);
                return true;
            }
            else
            {
                MinimEnv *env2;

                init_env(&env2, env, NULL);
                for (size_t i = before; i < len - after; ++i)
                {
                    MinimEnv *env3;

                    init_env(&env3, env, NULL);
                    if (!match_transform(env3, match->children[ell_pos - 1],
                                         MINIM_VECTOR_REF(datum, i),
                                         reserved, pdepth + 1))
                        return false;

                     minim_symbol_table_merge2(env2->table, env3->table, merge_pattern, add_pattern);
                }

                minim_symbol_table_merge(env->table, env2->table);
            }

            for (size_t i = ell_pos + 1, j = len - after; i < match->childc; ++i, ++j)
            {
                if (!match_transform(env, match->children[i], MINIM_VECTOR_REF(datum, j), reserved, pdepth))
                    return false;
            }
        }
        else
        {
            if (match->childc != MINIM_VECTOR_LEN(datum))
                return false;

            for (size_t i = 0; i < match->childc; ++i)
            {
                if (!match_transform(env, match->children[i], MINIM_VECTOR_REF(datum, i),
                                      reserved, pdepth))
                    return false;
            }
        }

        return true;
    }
    else // match->type == SYNTAX_NODE_LIST
    {
        size_t ell_pos;

        if (match->childc == 0)
            return minim_nullp(datum);

        if (!MINIM_OBJ_PAIRP(datum) && !minim_nullp(datum))
            return false;
        
        ell_pos = ellipse_pos(match);
        if (ell_pos != 0)     // much more complicated to do this
        {
            MinimObject *it, *ell_it, *after_it;
            size_t before, after, len, i;

            if (!minim_listp(datum))  // only proper lists are allowed here
                return false;

            len = minim_list_length(datum);
            before = ell_pos - 1;
            after = match->childc - ell_pos - 1;

            if (len < before + after)   // not enough space
                return false;

            // try matching front
            it = datum;
            for (i = 0; i < before; ++i, it = MINIM_CDR(it))
            {
                if (!match_transform(env, match->children[i], MINIM_CAR(it), reserved, pdepth))
                    return false;
            }

            // try matching pattern w/ ellipse
            MINIM_CDNR(ell_it, datum, j, before);
            MINIM_CDNR(after_it, datum, j, len - after);
            if (len == before + after)
            {
                // Bind null
                add_null_variables(env, match->children[ell_pos - 1], reserved, pdepth);
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
                    if (!match_transform(env3, match->children[ell_pos - 1], MINIM_CAR(ell_it),
                                         reserved, pdepth + 1))
                        return false;

                    minim_symbol_table_merge2(env2->table, env3->table, merge_pattern, add_pattern);
                }

                minim_symbol_table_merge(env->table, env2->table);
            }

            // try matching end
            for (i = ell_pos + 1; i < match->childc; ++i)
            {
                 if (!match_transform(env, match->children[i], MINIM_CAR(after_it), reserved, pdepth))
                    return false;

                after_it = MINIM_CDR(after_it);
            }

            // didn't reach end of ast
            if (!minim_nullp(after_it))
                return false;
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
                    return match_transform(env, match->children[i], MINIM_CAR(it), reserved, pdepth) &&
                           match_transform(env, match->children[i + 2], MINIM_CDR(it), reserved, pdepth);
                }

                if (!match_transform(env, match->children[i], MINIM_CAR(it), reserved, pdepth))
                    return false;
            }

            // didn't reach end of ast
            if (!minim_nullp(it))
                return false;
        }

        return true;
    }

    return false;
}

static void
get_patterns(MinimEnv *env, SyntaxNode *ast, MinimObject *patterns)
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
                if (strcmp(MINIM_SYMBOL(MINIM_CAR(MINIM_VECTOR_REF(patterns, i))), ast->sym) == 0)
                    return; // already in list
            }

            ++MINIM_VECTOR_LEN(patterns);
            MINIM_VECTOR(patterns) = GC_realloc(MINIM_VECTOR(patterns),
                                                MINIM_VECTOR_LEN(patterns) * sizeof(MinimObject*));
            MINIM_VECTOR_REF(patterns, MINIM_VECTOR_LEN(patterns) - 1) =
                minim_cons(minim_symbol(ast->sym), val);
        }
    }
}

static void
next_patterns_rec(MinimEnv *env, SyntaxNode *ast, MinimObject *pats, MinimObject *npats)
{
    if (ast->childc > 0)
    {
        for (size_t i = 0; i < ast->childc; ++i)
            next_patterns_rec(env, ast->children[i], pats, npats);
    }
    else if (ast->sym)
    {
        char *name;

        if (strcmp(ast->sym, "...") == 0 || strcmp(ast->sym, ".") == 0)     // early exit
            return;

        for (size_t i = 0; i < MINIM_VECTOR_LEN(pats); ++i)
        {
            name = MINIM_SYMBOL(MINIM_CAR(MINIM_VECTOR_REF(pats, i)));
            if (strcmp(ast->sym, name) == 0)  // in pats
            {
                for (size_t j = 0; j < MINIM_VECTOR_LEN(npats); ++j)
                {
                    name = MINIM_SYMBOL(MINIM_CAR(MINIM_VECTOR_REF(npats, j)));
                    if (strcmp(ast->sym, name) == 0)    // in npats
                        return;
                }

                ++MINIM_VECTOR_LEN(npats);
                MINIM_VECTOR(npats) = GC_realloc(MINIM_VECTOR(npats), MINIM_VECTOR_LEN(npats) * sizeof(MinimObject*));
                MINIM_VECTOR_REF(npats, MINIM_VECTOR_LEN(npats) - 1) = MINIM_VECTOR_REF(pats, i);
            }
        }
    }
}

// Gathers pattern variables in the ast if they are also in `pats` 
static MinimObject *
next_patterns(MinimEnv *env, SyntaxNode *ast, MinimObject *pats)
{
    MinimObject *npats;
    
    npats = minim_vector(0, NULL);
    next_patterns_rec(env, ast, pats, npats);
    return npats;
}

static size_t
pattern_length(MinimObject *patterns)
{
    MinimObject *trans;
    size_t depth;
    
    for (size_t i = 0; i < MINIM_VECTOR_LEN(patterns); ++i)
    {
        trans = MINIM_TRANSFORMER(MINIM_CDR(MINIM_VECTOR_REF(patterns, i)));
        depth = (size_t) MINIM_CDR(trans);
        if (depth > 0)  return minim_list_length(MINIM_CAR(trans));
    }

    // Panic!!
    THROW(NULL, minim_error("template contains no pattern variable before ellipse", "syntax-case"));
    return 0;
}

static MinimObject *
get_pattern(const char *sym, MinimObject *pats)
{
    for (size_t i = 0; i < MINIM_VECTOR_LEN(pats); ++i)
    {
        if (strcmp(sym, MINIM_SYMBOL(MINIM_CAR(MINIM_VECTOR_REF(pats, i)))) == 0)
            return MINIM_CAR(MINIM_TRANSFORMER(MINIM_CDR(MINIM_VECTOR_REF(pats, i))));
    }

    return NULL;
}

static SyntaxNode *
apply_transformation(MinimEnv *env, SyntaxNode *ast, MinimObject *patterns)
{
    SyntaxNode *ret;

    // printf("trans: "); print_ast(ast); printf("\n");
    // debug_pattern_table(env, patterns);
    if (ast->type == SYNTAX_NODE_LIST || ast->type == SYNTAX_NODE_VECTOR)
    {
        init_syntax_node(&ret, ast->type);
        ret->childc = ast->childc;
        ret->children = GC_alloc(ret->childc * sizeof(SyntaxNode*));
        for (size_t i = 0, r = 0; i < ast->childc; ++i, ++r)
        {
            if (i + 2 == ast->childc && ast->children[i]->sym &&            // dot
                strcmp(ast->children[i]->sym, ".") == 0)
            {
                SyntaxNode *rest;
                
                rest = apply_transformation(env, ast->children[i + 1], patterns);
                if (rest->type == SYNTAX_NODE_LIST)
                {
                    if (rest->childc == 0)
                    {
                        ret->childc -= 2;
                        ret->children = GC_realloc(ret->children, ret->childc * sizeof(SyntaxNode*));
                    }
                    else if (rest->childc == 1)
                    {  
                        ret->childc -= 1;
                        ret->children = GC_realloc(ret->children, ret->childc * sizeof(SyntaxNode*));
                        ret->children[r] = rest->children[0];
                    }
                    else if (rest->childc == 2)
                    {
                        ret->children[r] = rest->children[0];
                        ret->children[r + 1] = rest->children[1]; 
                    }
                    else
                    {
                        ret->childc += rest->childc;
                        ret->childc -= 2;
                        ret->children = GC_realloc(ret->children, ret->childc * sizeof(SyntaxNode*));
                        for (size_t i = 0; i < rest->childc; ++i)
                            ret->children[r + i] = rest->children[i];
                    }
                }
                else
                {
                    copy_syntax_node(&ret->children[r], ast->children[i]);
                    ret->children[i] = rest;
                }
            }
            else if (i + 1 < ast->childc && ast->children[i + 1]->sym &&   // ellipse
                     strcmp(ast->children[i + 1]->sym, "...") == 0)
            {
                MinimObject *fpats;
                size_t plen, pc;

                fpats = next_patterns(env, ast->children[i], patterns);
                plen = pattern_length(fpats);
                pc = MINIM_VECTOR_LEN(fpats);
                if (plen == 0)  // reduce by 2
                {
                    ret->childc -= 2;
                    ret->children = GC_realloc(ret->children, ret->childc * sizeof(SyntaxNode*));
                }
                else if (plen == 1)  // reduce by 1
                {
                    ret->childc -= 1;
                    ret->children = GC_realloc(ret->children, ret->childc * sizeof(SyntaxNode*));
                }
                else if (plen > 2)  // expand by plen - 2
                {
                    ret->childc += (plen - 2);
                    ret->children = GC_realloc(ret->children, ret->childc * sizeof(SyntaxNode*));
                }

                if (plen == 0 && r < ret->childc)
                {
                    init_syntax_node(&ret->children[r], SYNTAX_NODE_LIST);
                    ret->children[r]->childc = 0;
                }
                else
                {
                    for (size_t j = 0; j < plen; ++j)
                    {
                        MinimObject *npats, *sym, *trans, *val;
                        size_t depth;

                        npats = minim_vector(pc, GC_alloc(pc * sizeof(MinimObject*)));
                        for (size_t k = 0; k < pc; ++k)
                        {
                            sym = MINIM_CAR(MINIM_VECTOR_REF(fpats, k));
                            trans = MINIM_TRANSFORMER(MINIM_CDR(MINIM_VECTOR_REF(fpats, k)));
                            val = MINIM_CAR(trans);
                            depth = (size_t) MINIM_CDR(trans);
                            if (depth > 0)
                            {
                                PrintParams pp;
                                Buffer *bf;

                                if (j == 0 && minim_list_length(val) != plen)
                                {
                                    init_buffer(&bf);
                                    set_default_print_params(&pp);
                                    writes_buffer(bf, "pattern length mismatch: ");
                                    print_to_buffer(bf, val, env, &pp);
                                    THROW(env, minim_error(get_buffer(bf), NULL));
                                }
                                
                                val = minim_cons(minim_list_ref(val, j), (void*)(depth - 1));
                                trans = minim_transform(val, MINIM_TRANSFORM_PATTERN);
                                MINIM_VECTOR_REF(npats, k) = minim_cons(sym, trans);
                            }
                            else
                            {
                                MINIM_VECTOR_REF(npats, k) = MINIM_VECTOR_REF(fpats, k);
                            }
                        }

                        ret->children[r + j] = apply_transformation(env, ast->children[i], npats);
                    }
                }
                
                i += 1;
                r += plen;
                r -= 1;
            }
            else    // normal
            {
                ret->children[r] = apply_transformation(env, ast->children[i], patterns);
            }
        }
    }
    else if (ast->type == SYNTAX_NODE_PAIR)
    {
        SyntaxNode *car, *cdr;

        car = apply_transformation(env, ast->children[0], patterns);
        cdr = apply_transformation(env, ast->children[1], patterns);
        if (cdr->type == SYNTAX_NODE_LIST)
        {
            init_syntax_node(&ret, SYNTAX_NODE_LIST);
            ret->childc = cdr->childc + 1;
            ret->children = GC_alloc(ret->childc * sizeof(SyntaxNode*));
            ret->children[0] = car;

            for (size_t i = 0; i < cdr->childc; ++i)
                ret->children[i + 1] = cdr->children[i];
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
        MinimObject *v = get_pattern(ast->sym, patterns);
        if (v)
        {
            if (MINIM_OBJ_ASTP(v))  copy_syntax_node(&ret, MINIM_AST(v));
            else                    ret = datum_to_syntax(env, v);
        }
        else
        {
            copy_syntax_node(&ret, ast);
        }
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
    res = eval_lambda2(MINIM_CLOSURE(MINIM_TRANSFORMER(trans)), env, 1, &body);
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
                if (minim_specialp(op))
                    return ast;

                if (MINIM_SYNTAX(op) == minim_builtin_template ||
                    MINIM_SYNTAX(op) == minim_builtin_syntax ||
                    MINIM_SYNTAX(op) == minim_builtin_quote ||
                    MINIM_SYNTAX(op) == minim_builtin_quasiquote)
                    return ast;

                if (MINIM_SYNTAX(op) == minim_builtin_def_syntaxes)
                {
                    ast->children[2] = transform_syntax(env, ast->children[2]);
                    return ast;
                }

                if (MINIM_SYNTAX(op) == minim_builtin_syntax_case)
                {
                    for (size_t i = 3; i < ast->childc; ++i)
                        ast->children[i]->children[1] = transform_syntax(env, ast->children[i]->children[1]);
                    return ast;
                }

                if (MINIM_OBJ_TRANSFORMP(op))
                {
                    SyntaxNode *trans;
                    // printf("t> "); print_ast(ast); printf("\n");
                    trans = transform_loc(env, op, ast);
                    // printf("t< "); print_ast(trans); printf("\n");
                    return transform_syntax(env, trans);
                }
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
                return transform_syntax(env, transform_loc(env, val, ast));
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

    val = eval_ast_no_check(env, transform_syntax(env, MINIM_AST(args[1])));
    if (!MINIM_OBJ_VALUESP(val))
    {
        if (MINIM_AST(args[0])->childc != 1)
            THROW(env, minim_values_arity_error("def-syntaxes",
                                                MINIM_AST(args[0])->childc,
                                                1,
                                                MINIM_AST(args[0])));
        
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

MinimObject *minim_builtin_template(MinimEnv *env, size_t argc, MinimObject **args)
{
    // Stores pattern data in a vector where each element
    // is a pair => (name, val)
    MinimObject *patterns;
    SyntaxNode *final;
    
    patterns = minim_vector(0, NULL);
    get_patterns(env, MINIM_AST(args[0]), patterns);

    // printf("template: "); print_ast(MINIM_AST(args[0])); printf("\n");
    // debug_pattern_table(env, patterns);
    final = apply_transformation(env, MINIM_AST(args[0]), patterns);
    // printf("final:    "); print_ast(final); printf("\n");
    return minim_ast(final);
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
        if (match_transform(match_env, match, datum, &reserved, 0))
        {
            MinimEnv *env2;
            MinimObject *val;

            init_env(&env2, env, NULL);
            minim_symbol_table_merge(env2->table, match_env->table);
            val = eval_ast_no_check(env2, replace);
            if (!MINIM_OBJ_ASTP(val))
                THROW(env, minim_error("expected syntax as result", "syntax-case"));

            // printf("sc>: "); debug_print_minim_object(datum, NULL);
            // printf("sc<: "); debug_print_minim_object(val, NULL);
            return minim_ast(transform_syntax(env, MINIM_AST(val)));
        }
    }

    THROW(env, minim_syntax_error("bad syntax", NULL, datum_to_syntax(env, datum), NULL));
    return NULL;
}
