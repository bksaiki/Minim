#include "minimpriv.h"

#define transform_type(x)   (MINIM_OBJ_CLOSUREP(x) ? MINIM_TRANSFORM_MACRO : MINIM_TRANSFORM_UNKNOWN)

// forward declaration
MinimObject* transform_syntax(MinimEnv *env, MinimObject* ast);

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
is_list_match_pattern(MinimObject *lst)
{
    MinimObject *t = MINIM_CDR(lst);
    if (!MINIM_OBJ_PAIRP(t))
        return false;

    t = MINIM_CAR(t);
    return MINIM_STX_SYMBOLP(t) && strcmp(MINIM_STX_SYMBOL(t), "...") == 0;
}

static bool
is_vector_match_pattern(MinimObject *vec, size_t i)
{
    MinimObject **arr = MINIM_VECTOR(vec);
    return (i + 1 < MINIM_VECTOR_LEN(vec) &&
            MINIM_STX_SYMBOLP(arr[i + 1]) &&
            strcmp(MINIM_STX_SYMBOL(arr[i + 1]), "...") == 0);
}

static size_t
list_ellipse_pos(MinimObject *lst)
{
    size_t i = 0;
    for (MinimObject *it = lst; MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it), ++i)
    {
        MinimObject *elem = MINIM_CAR(it);
        if (MINIM_STX_SYMBOLP(elem) && strcmp(MINIM_STX_SYMBOL(elem), "...") == 0)
            return i;
    }

    return 0;
}

static size_t
vector_ellipse_pos(MinimObject *vec)
{
    for (size_t i = 0; i < MINIM_VECTOR_LEN(vec); ++i)
    {
        MinimObject *elem = MINIM_VECTOR_REF(vec, i);
        if (MINIM_STX_SYMBOLP(elem) && strcmp(MINIM_STX_SYMBOL(elem), "...") == 0)
            return i;
    }

    return 0;
}

static void
add_null_variables(MinimEnv *env, MinimObject *stx, SymbolList *reserved, size_t pdepth)
{
    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *it = MINIM_STX_VAL(stx);
        for (; MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            if (is_list_match_pattern(it))
            {
                add_null_variables(env, MINIM_CAR(it), reserved, pdepth + 1);
                it = MINIM_CDR(it);
            }
            else
            {
                add_null_variables(env, MINIM_CAR(it), reserved, pdepth);
            }
        }

        if (!minim_nullp(it))
            add_null_variables(env, MINIM_CAR(it), reserved, pdepth);
    }
    else if (MINIM_STX_VECTORP(stx))
    {
        MinimObject *vec = MINIM_STX_VAL(stx);
        for (size_t i = 0; i < MINIM_VECTOR_LEN(vec); ++i)
        {
            if (is_vector_match_pattern(vec, i))
            {
                add_null_variables(env, MINIM_VECTOR_REF(vec, i), reserved, pdepth + 1);
                ++i;
            }
            else
            {
                add_null_variables(env, MINIM_VECTOR_REF(vec, i), reserved, pdepth);
            }
        }
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        MinimObject *val;
        char *str = MINIM_STX_SYMBOL(stx);

        if (strcmp(str, "...") == 0 ||              // ellipse
            strcmp(str, "_") == 0 ||                // wildcard
            symbol_list_contains(reserved, str))    // reserved
            return;

        val = env_get_sym(env, str);
        if (val && MINIM_OBJ_TRANSFORMP(val))       // already bound
            return;

        val = minim_transform(minim_cons(minim_null, (void*) (pdepth + 1)), MINIM_TRANSFORM_PATTERN);
        env_intern_sym(env, str, val);
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
    return minim_transform(minim_cons(minim_cons(val, minim_null), depth), MINIM_TRANSFORM_PATTERN);
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
match_transform(MinimEnv *env, MinimObject *match, MinimObject *stx, SymbolList *reserved, size_t pdepth)
{
    MinimObject *match_e, *stx_e;

    // printf("match: "); print_syntax_to_port(match, stdout); printf("\n");
    // printf("stx:   "); print_syntax_to_port(stx, stdout); printf("\n");

    match_e = MINIM_STX_VAL(match);
    stx_e = MINIM_STX_VAL(stx);
    if (minim_nullp(match_e))
    {
        return minim_nullp(stx_e);
    }
    else if (MINIM_OBJ_PAIRP(match_e))
    {
        MinimObject *match_it, *stx_it;
        size_t match_len, stx_len, ell_pos;

        if (!MINIM_OBJ_PAIRP(stx_e) && !minim_nullp(stx_e))
            return false;

        match_len = syntax_proper_list_len(match);
        stx_len = syntax_proper_list_len(stx);
        ell_pos = list_ellipse_pos(match_e);
        if (ell_pos != 0)       // ellipse in pattern
        {
            size_t before, i;

            // must be a proper list
            if (stx_len == SIZE_MAX)
                return false;
            
            i = 0;
            before = ell_pos - 1;
            match_it = match_e;
            stx_it = stx_e;
            while (!minim_nullp(match_it))
            {
                if (i == before)        // ellipse encountered
                {
                    if (stx_len + 2 == match_len)       // null
                    {
                        add_null_variables(env, MINIM_CAR(match_it), reserved, pdepth);
                    }
                    else
                    {
                        MinimEnv *env2;
                        MinimObject *after_it;

                        if (stx_len + 2 < match_len)    // not long enough
                            return false;

                        env2 = init_env(env);
                        after_it = minim_list_drop(stx_it, stx_len + 2 - match_len);
                        for (; stx_it != after_it; stx_it = MINIM_CDR(stx_it))
                        {
                            MinimEnv *env3 = init_env(env);
                            if (!match_transform(env3, MINIM_CAR(match_it), MINIM_CAR(stx_it),
                                                reserved, pdepth + 1))
                                return false;

                            env_merge_local_symbols2(env2, env3, merge_pattern, add_pattern);
                        }

                        env_merge_local_symbols(env, env2);
                    }

                    match_it = MINIM_CDR(match_it);
                    if (minim_nullp(match_it))
                        return false;

                    match_it = MINIM_CDR(match_it);
                    ++i;
                }
                else
                {
                    if (minim_nullp(stx_it))
                        return false;

                    if (!match_transform(env, MINIM_CAR(match_it), MINIM_CAR(stx_it), reserved, pdepth))
                        return false;

                    match_it = MINIM_CDR(match_it);
                    ++i;

                    if (minim_nullp(stx_it))
                        return minim_nullp(match_it);

                    stx_it = MINIM_CDR(stx_it);
                }
            }
        }
        else
        {
            if (match_len == SIZE_MAX)
                match_len = syntax_list_len(match);

            if (stx_len == SIZE_MAX)
                stx_len = syntax_list_len(stx);

            match_it = match_e;
            stx_it = stx_e;
            while (MINIM_OBJ_PAIRP(match_it))
            {
                if (minim_nullp(stx_it))
                    return false;

                if (!match_transform(env, MINIM_CAR(match_it), MINIM_CAR(stx_it), reserved, pdepth))
                    return false;

                match_it = MINIM_CDR(match_it);
                stx_it = MINIM_CDR(stx_it);
            }
        }

        // proper list
        if (minim_nullp(match_it))
            return minim_nullp(stx_it);

        // reform syntax
        stx = datum_to_syntax(env, stx_it);
        return match_transform(env, match_it, stx, reserved, pdepth);
    }
    if (MINIM_STX_VECTORP(match))
    {
        size_t match_len, stx_len, ell_pos;

        // early exit not a vector
        if (!MINIM_OBJ_VECTORP(stx_e))
            return false;

        // empty vector
        match_len = MINIM_VECTOR_LEN(match_e);
        stx_len = MINIM_VECTOR_LEN(stx_e);
        if (match_len == 0)
            return stx_len == 0;

        ell_pos = vector_ellipse_pos(match_e);
        if (ell_pos != 0)     // ellipse in pattern
        {
            size_t before, after;

            before = ell_pos - 1;
            after = match_len - ell_pos - 1;

            // not enough space
            if (stx_len < before + after)
                return false;

            // try matching front
            for (size_t i = 0; i < before; ++i)
            {
                if (!match_transform(env, MINIM_VECTOR_REF(match_e, i),
                                     MINIM_VECTOR_REF(stx_e, i),
                                     reserved, pdepth))
                    return false;
            }

            if (stx_len == before + after)
            {
                // Bind null
                add_null_variables(env, MINIM_VECTOR_REF(match_e, ell_pos - 1), reserved, pdepth);
                return true;
            }
            else
            {
                MinimEnv *env2 = init_env(env);
                for (size_t i = before; i < stx_len - after; ++i)
                {
                    MinimEnv *env3 = init_env(env);
                    if (!match_transform(env3, MINIM_VECTOR_REF(match_e, ell_pos - 1),
                                         MINIM_VECTOR_REF(stx_e, i),
                                         reserved, pdepth + 1))
                        return false;

                     env_merge_local_symbols2(env2, env3, merge_pattern, add_pattern);
                }

                env_merge_local_symbols(env, env2);
            }

            for (size_t i = ell_pos + 1, j = stx_len - after; i < match_len; ++i, ++j)
            {
                if (!match_transform(env, MINIM_VECTOR_REF(match_e, i),
                                     MINIM_VECTOR_REF(stx_e, i),
                                     reserved, pdepth))
                    return false;
            }
        }
        else
        {
            if (match_len != stx_len)
                return false;

            for (size_t i = 0; i < match_len; ++i)
            {
                if (!match_transform(env, MINIM_VECTOR_REF(match_e, i),
                                     MINIM_VECTOR_REF(stx_e, i),
                                     reserved, pdepth))
                    return false;
            }
        }

        return true;
    }
    else if (MINIM_OBJ_SYMBOLP(match_e))
    {
        MinimObject *val, *pattern;

        if (strcmp(MINIM_SYMBOL(match_e), "_") == 0)       // wildcard
            return true;

        if (symbol_list_contains(reserved, MINIM_SYMBOL(match_e)))       // reserved name
        {
            return MINIM_OBJ_SYMBOLP(stx_e) &&
                   strcmp(MINIM_SYMBOL(match_e), MINIM_SYMBOL(stx_e)) == 0;
        }

        pattern = env_get_sym(env, MINIM_SYMBOL(match_e));
        if (pattern && MINIM_OBJ_TRANSFORMP(pattern) &&
            MINIM_TRANSFORM_TYPE(pattern) == MINIM_TRANSFORM_PATTERN)
        {
            return minim_equalp(MINIM_TRANSFORMER(pattern), stx_e);
        }
        
        val = minim_transform(minim_cons(stx_e, (void*) pdepth), MINIM_TRANSFORM_PATTERN);
        env_intern_sym(env, MINIM_SYMBOL(match_e), val);
        return true;
    }
    else
    {
        return minim_eqp(match_e, stx_e);
    }

    return false;
}

static void
get_patterns(MinimEnv *env, MinimObject *stx, MinimObject *patterns)
{
    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *trailing = NULL;
        for (MinimObject *it = MINIM_STX_VAL(stx); MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            get_patterns(env, MINIM_CAR(it), patterns);
            trailing = it;
        }

        if (trailing && !minim_nullp(MINIM_CDR(trailing)))
            get_patterns(env, MINIM_CDR(trailing), patterns);
    }
    else if (MINIM_STX_VECTORP(stx))
    {
        MinimObject *vec = MINIM_STX_VAL(stx);
        for (size_t i = 0; i < MINIM_VECTOR_LEN(vec); ++i)
            get_patterns(env, MINIM_VECTOR_REF(vec, i), patterns);
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        MinimObject *val;
        char *sym = MINIM_STX_SYMBOL(stx);

        if (strcmp(sym, "...") == 0 ||
            strcmp(sym, ".") == 0)     // early exit
            return;

        val = env_get_sym(env, sym);
        if (MINIM_OBJ_TRANSFORMP(val) && MINIM_TRANSFORM_TYPE(val) == MINIM_TRANSFORM_PATTERN)
        {
            for (size_t i = 0; i < MINIM_VECTOR_LEN(patterns); ++i)
            {
                MinimObject *key = MINIM_CAR(MINIM_VECTOR_REF(patterns, i));
                if (strcmp(MINIM_SYMBOL(key), sym) == 0)
                    return; // already in list
            }

            ++MINIM_VECTOR_LEN(patterns);
            MINIM_VECTOR(patterns) = GC_realloc(MINIM_VECTOR(patterns),
                                                MINIM_VECTOR_LEN(patterns) * sizeof(MinimObject*));
            MINIM_VECTOR_REF(patterns, MINIM_VECTOR_LEN(patterns) - 1) =
                minim_cons(intern(MINIM_STX_SYMBOL(stx)), val);
        }
    }
}

static void
next_patterns_rec(MinimEnv *env, MinimObject *stx, MinimObject *pats, MinimObject *npats)
{
    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *trailing = NULL;
        for (MinimObject *it = MINIM_STX_VAL(stx); MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            next_patterns_rec(env, MINIM_CAR(it), pats, npats);
            trailing = it;
        }

        if (trailing && !minim_nullp(MINIM_CDR(trailing)))
            next_patterns_rec(env, MINIM_CDR(trailing), pats, npats);
    }
    else if (MINIM_STX_VECTORP(stx))
    {
        MinimObject *vec = MINIM_STX_VAL(stx);
        for (size_t i = 0; i < MINIM_VECTOR_LEN(vec); ++i)
            next_patterns_rec(env, MINIM_VECTOR_REF(vec, i), pats, npats);
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        char *name;

        if (strcmp(MINIM_STX_SYMBOL(stx), "...") == 0 ||
            strcmp(MINIM_STX_SYMBOL(stx), ".") == 0)     // early exit
            return;

        for (size_t i = 0; i < MINIM_VECTOR_LEN(pats); ++i)
        {
            name = MINIM_SYMBOL(MINIM_CAR(MINIM_VECTOR_REF(pats, i)));
            if (strcmp(MINIM_STX_SYMBOL(stx), name) == 0)  // in pats
            {
                for (size_t j = 0; j < MINIM_VECTOR_LEN(npats); ++j)
                {
                    name = MINIM_SYMBOL(MINIM_CAR(MINIM_VECTOR_REF(npats, j)));
                    if (strcmp(MINIM_STX_SYMBOL(stx), name) == 0)    // in npats
                        return;
                }

                MINIM_VECTOR_RESIZE(npats, MINIM_VECTOR_LEN(npats) + 1);
                MINIM_VECTOR_REF(npats, MINIM_VECTOR_LEN(npats) - 1) = MINIM_VECTOR_REF(pats, i);
            }
        }
    }
}

// Gathers pattern variables in the ast if they are also in `pats` 
static MinimObject *
next_patterns(MinimEnv *env, MinimObject *stx, MinimObject *pats)
{
    MinimObject *npats;
    
    npats = minim_vector(0, NULL);
    next_patterns_rec(env, stx, pats, npats);
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
next_depth_patterns(MinimEnv *env, MinimObject *fpats, size_t plen, size_t pc, size_t j)
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

    return npats;
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

static MinimObject *
apply_transformation(MinimEnv *env, MinimObject *stx, MinimObject *patterns)
{
    // printf("trans: "); print_syntax_to_port(stx, stdout); printf("\n");
    // debug_pattern_table(env, patterns);

    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *hd, *tl, *trailing;

        hd = minim_null;
        tl = NULL;
        trailing = NULL;
        for (MinimObject *it = MINIM_STX_VAL(stx); MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            if (is_list_match_pattern(it))
            {
                MinimObject *fpats;
                size_t plen, pc;

                fpats = next_patterns(env, MINIM_CAR(it), patterns);
                plen = pattern_length(fpats);
                pc = MINIM_VECTOR_LEN(fpats);
                if (plen != 0)
                {
                    for (size_t j = 0; j < plen; ++j)
                    {
                        MinimObject *npats, *val;
                        
                        npats = next_depth_patterns(env, fpats, plen, pc, j);
                        val = apply_transformation(env, MINIM_CAR(it), npats);
                        if (tl != NULL)     // list already started
                        {
                            MINIM_CDR(tl) = minim_cons(val, minim_null);
                            tl = MINIM_CDR(tl);
                        }
                        else                // start of list
                        {
                            hd = minim_cons(val, minim_null);
                            tl = hd;
                        }
                    }
                }

                it = MINIM_CDR(it);
            }
            else
            {
                MinimObject *val = apply_transformation(env, MINIM_CAR(it), patterns);
                if (tl != NULL)     // list already started
                {
                    MINIM_CDR(tl) = minim_cons(val, minim_null);
                    tl = MINIM_CDR(tl);
                }
                else                // start of list
                {
                    hd = minim_cons(val, minim_null);
                    tl = hd;
                }
            }

            trailing = it;
        }

        // check for improper lists
        if (trailing && !minim_nullp(MINIM_CDR(trailing)))
        {
            MinimObject *rest = apply_transformation(env, MINIM_CDR(trailing), patterns);
            if (hd == trailing)     // cons cell
            {
                MINIM_CDR(hd) = rest;
                return stx;
            }
            else                    // trailing
            {
                return minim_ast(minim_list_append2(hd, MINIM_STX_VAL(rest)), NULL);
            }
        }
        else
        {
            return minim_ast(hd, NULL);
        }
    }
    else if (MINIM_STX_VECTORP(stx))
    {
        MinimObject **arr, *vec;
        size_t len;

        vec = MINIM_STX_VAL(stx);
        len = MINIM_VECTOR_LEN(vec);
        arr = GC_alloc(len * sizeof(MinimObject*));
        for (size_t i = 0, r = 0; i < len; ++i, ++r)
        {
            if (is_vector_match_pattern(vec, i))
            {
                MinimObject *fpats;
                size_t plen, pc;

                fpats = next_patterns(env, MINIM_VECTOR_REF(vec, i), patterns);
                plen = pattern_length(fpats);
                pc = MINIM_VECTOR_LEN(fpats);
                if (plen == 0)          len -= 2;               // reduce by 2
                else if (plen == 1)     len -= 1;               // reduce by 1
                else if (plen > 2)      len -= (plen - 2);      // expand by plen - 2

                arr = GC_realloc(arr, len * sizeof(MinimObject*));
                if (plen != 0)
                {
                    for (size_t j = 0; j < plen; ++j)
                    {
                        MinimObject *npats = next_depth_patterns(env, fpats, plen, pc, j);
                        arr[r + j] = apply_transformation(env, MINIM_VECTOR_REF(vec, i), npats);
                    }
                }
                
                i += 1;
                r = (r + plen) - 1;
            }
            else
            {
                arr[r] = apply_transformation(env, MINIM_VECTOR_REF(vec, i), patterns);
            }
        }

        return minim_ast(minim_vector(len, arr), NULL);
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        MinimObject *v = get_pattern(MINIM_STX_SYMBOL(stx), patterns);
        if (!v)                         return minim_ast(MINIM_STX_VAL(stx), MINIM_STX_LOC(stx));
        else if (MINIM_OBJ_ASTP(v))     return minim_ast(MINIM_STX_VAL(v), MINIM_STX_LOC(v));
        else                            return datum_to_syntax(env, v);
    }
    else
    {
        return minim_ast(MINIM_STX_VAL(stx), MINIM_STX_LOC(stx));
    }
}

MinimObject*
transform_loc(MinimEnv *env, MinimObject *trans, MinimObject *stx)
{
    MinimObject *res;

    if (MINIM_TRANSFORM_TYPE(trans) != MINIM_TRANSFORM_MACRO)
    {
        THROW(env, minim_syntax_error("illegal use of syntax transformer",
                                      MINIM_STX_SYMBOL(stx),
                                      stx, NULL));
    }

    res = eval_lambda2(MINIM_CLOSURE(MINIM_TRANSFORMER(trans)), env, 1, &stx);
    if (!MINIM_OBJ_ASTP(res))
    {
        THROW(env, minim_syntax_error("expected syntax as a result",
                                      MINIM_STX_SYMBOL(stx),
                                      stx, res));
    }

    return res;
}

static bool valid_matchp(MinimEnv *env, MinimObject* match, MatchTable *table,
                         SymbolList *reserved, size_t pdepth)
{
    if (MINIM_STX_PAIRP(match))
    {
        MinimObject *trailing = NULL;
        for (MinimObject *it = MINIM_STX_VAL(match); MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            if (is_list_match_pattern(it))
            {
                if (!valid_matchp(env, MINIM_CAR(it), table, reserved, pdepth + 1))
                    return false;
                it = MINIM_CDR(it);     // skip ellipse
            }
            else
            {
                if (!valid_matchp(env, MINIM_CAR(it), table, reserved, pdepth))
                    return false;
            }

            trailing = it;
        }


        // improper list
        if (trailing && !minim_nullp(MINIM_CDR(trailing)) &&
            !valid_matchp(env, MINIM_CDR(trailing), table, reserved, pdepth))
            return false;
    }
    else if (MINIM_STX_VECTORP(match))
    {
        MinimObject *vec = MINIM_STX_VAL(match);
        for (size_t i = 0; i < MINIM_VECTOR_LEN(vec); ++i)
        {
            if (is_vector_match_pattern(vec, i))
            {
                if (!valid_matchp(env, MINIM_VECTOR_REF(vec, i), table, reserved, pdepth + 1))
                    return false;
                ++i;    // skip ellipse
            }
            else
            {
                if (!valid_matchp(env, MINIM_VECTOR_REF(vec, i), table, reserved, pdepth))
                    return false;
            }
        }
    }
    else if (MINIM_STX_SYMBOLP(match))
    {
        if (strcmp(MINIM_STX_SYMBOL(match), "...") == 0)
        {
            THROW(env, minim_syntax_error("ellipse not allowed here", NULL, match, NULL));
            return false;
        }
        
        if (!symbol_list_contains(reserved, MINIM_STX_SYMBOL(match)) &&
            strcmp(MINIM_STX_SYMBOL(match), "_") != 0)
            match_table_add(table, MINIM_STX_SYMBOL(match), pdepth, minim_null);
    }

    return true;
}

static bool
valid_replacep(MinimEnv *env, MinimObject* replace, MatchTable *table,
               SymbolList *reserved, size_t pdepth)
{
    if (MINIM_STX_PAIRP(replace))
    {
        MinimObject *trailing = NULL;
        for (MinimObject *it = MINIM_STX_VAL(replace); MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            if (is_list_match_pattern(it))
            {
                if (!valid_replacep(env, MINIM_CAR(it), table, reserved, pdepth + 1))
                    return false;
                it = MINIM_CDR(it);     // skip ellipse
            }
            else
            {
                if (!valid_replacep(env, MINIM_CAR(it), table, reserved, pdepth))
                    return false;
            }

            trailing = it;
        }


        // improper list
        if (trailing && !minim_nullp(MINIM_CDR(trailing)) &&
            !valid_replacep(env, MINIM_CDR(trailing), table, reserved, pdepth))
        {
            return false;
        }

    }
    else if (MINIM_STX_VECTORP(replace))
    {
        MinimObject *vec = MINIM_STX_VAL(replace);
        for (size_t i = 0; i < MINIM_VECTOR_LEN(vec); ++i)
        {
            if (is_vector_match_pattern(vec, i))
            {
                if (!valid_replacep(env, MINIM_VECTOR_REF(vec, i), table, reserved, pdepth + 1))
                    return false;
                ++i;    // skip ellipse
            }
            else
            {
                if (!valid_replacep(env, MINIM_VECTOR_REF(vec, i), table, reserved, pdepth))
                    return false;
            }
        }
    }
    else if (MINIM_STX_SYMBOLP(replace))
    {
        if (!symbol_list_contains(reserved, MINIM_SYMBOL(replace)))
        {
            size_t depth = match_table_get_depth(table, MINIM_SYMBOL(replace));
            if (depth != SIZE_MAX && pdepth < depth)  // in table but too deep
            {
                THROW(env, minim_syntax_error("too many ellipses in pattern", NULL, replace, NULL));
                return false;
            }
        }
    }

    return true;
}

// ================================ Public ================================

MinimObject* transform_syntax(MinimEnv *env, MinimObject* stx)
{
    if (MINIM_STX_PAIRP(stx))
    {
        MinimObject *trailing;

        if (MINIM_STX_SYMBOLP(MINIM_STX_CAR(stx)))
        {
            MinimObject *op = env_get_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(stx)));
            if (op)
            {
                if (MINIM_OBJ_SYNTAXP(op))
                {
                    if (MINIM_SYNTAX(op) == minim_builtin_template ||
                        MINIM_SYNTAX(op) == minim_builtin_syntax ||
                        MINIM_SYNTAX(op) == minim_builtin_quote ||
                        MINIM_SYNTAX(op) == minim_builtin_quasiquote)
                    {
                        return stx;
                    }

                    if (MINIM_SYNTAX(op) == minim_builtin_def_syntaxes)
                    {
                        MinimObject *drop2 = minim_list_drop(MINIM_STX_VAL(stx), 2);
                        MINIM_CAR(drop2) = transform_syntax(env, MINIM_CAR(drop2));
                        return stx;
                    }

                    if (MINIM_SYNTAX(op) == minim_builtin_syntax_case)
                    {
                        MinimObject *it, *r;
                        
                        it = minim_list_drop(MINIM_STX_VAL(stx), 3);
                        while (!minim_nullp(it))
                        {
                            r = MINIM_STX_CDR(MINIM_CAR(it));
                            MINIM_CAR(r) = transform_syntax(env, MINIM_CAR(r));
                            it = MINIM_CDR(it);
                        }

                        return stx;
                    }
                }
                else if (MINIM_OBJ_TRANSFORMP(op))
                {
                    MinimObject *trans;
                    // printf("t> "); print_syntax_to_port(stx, stdout); printf("\n");
                    trans = transform_loc(env, op, stx);
                    // printf("t< "); print_syntax_to_port(trans, stdout); printf("\n");
                    return transform_syntax(env, trans);
                }
            }
        }

        trailing = NULL;
        for (MinimObject *it = MINIM_STX_VAL(stx); MINIM_OBJ_PAIRP(it); it = MINIM_CDR(it))
        {
            MINIM_CAR(it) = transform_syntax(env, MINIM_CAR(it));
            trailing = it;
        }

        if (trailing && !minim_nullp(MINIM_CDR(trailing)))
        {
            trailing = MINIM_CDR(trailing);
            MINIM_CAR(trailing) = transform_syntax(env, MINIM_CAR(trailing));
        }
    }
    else if (MINIM_STX_VECTORP(stx))
    {
        MinimObject *vec = MINIM_STX_VAL(stx);
        for (size_t i = 0; i < MINIM_VECTOR_LEN(vec); ++i)
            MINIM_VECTOR_REF(vec, i) = transform_syntax(env, MINIM_VECTOR_REF(vec, i));
    }
    else if (MINIM_STX_SYMBOLP(stx))
    {
        MinimObject *val = env_get_sym(env, MINIM_STX_SYMBOL(stx));
        if (val && MINIM_OBJ_TRANSFORMP(val))
            return transform_syntax(env, transform_loc(env, val, stx));
    }

    return stx;
}

void check_transform(MinimEnv *env, MinimObject *match, MinimObject *replace, MinimObject *reserved)
{
    MatchTable table;
    SymbolList reserved_lst;
    MinimObject *it;
    size_t reservedc;

    reservedc = syntax_list_len(reserved);
    init_symbol_list(&reserved_lst, reservedc);
    it = MINIM_STX_VAL(reserved);
    for (size_t i = 0; i < reservedc; ++i, it = MINIM_CDR(it))
        reserved_lst.syms[i] = MINIM_STX_SYMBOL(MINIM_CAR(it));

    init_match_table(&table);
    valid_matchp(env, match, &table, &reserved_lst, 0);
    valid_replacep(env, replace, &table, &reserved_lst, 0);
}

// ================================ Builtins ================================

MinimObject *minim_builtin_def_syntaxes(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *val, *trans;
    size_t bindc;

    bindc = syntax_list_len(args[0]);
    val = eval_ast_no_check(env, transform_syntax(env, args[1]));
    if (!MINIM_OBJ_VALUESP(val))
    {
        if (bindc != 1)
        {
            THROW(env, minim_values_arity_error("def-syntaxes", bindc,
                                                1, args[0]));
        }

        if (!MINIM_OBJ_CLOSUREP(val))
        {
            THROW(env, minim_syntax_error("expected a procedure of 1 argument",
                                          "def-syntaxes",
                                          args[1],
                                          NULL));
        }

        trans = minim_transform(val, transform_type(val));
        env_intern_sym(env, MINIM_STX_SYMBOL(MINIM_STX_CAR(args[0])), trans);
    }
    else
    {
        MinimObject *it;

        if (MINIM_VALUES_SIZE(val) != bindc)
        {
            THROW(env, minim_values_arity_error("def-values", bindc,
                                                MINIM_VALUES_SIZE(val), args[0]));
        }

        it = MINIM_STX_VAL(args[0]);
        for (size_t i = 0; i < bindc; ++i)
        {
            env_intern_sym(env, MINIM_STX_SYMBOL(MINIM_CAR(it)), MINIM_VALUES(val)[i]);
            it = MINIM_STX_CDR(it);
        }
    }

    return minim_void;
}

MinimObject *minim_builtin_template(MinimEnv *env, size_t argc, MinimObject **args)
{
    // Stores pattern data in a vector where each element
    // is a pair => (name, val)
    MinimObject *patterns;
    MinimObject *final;
    
    patterns = minim_vector(0, NULL);
    get_patterns(env, args[0], patterns);

    // printf("template: "); print_syntax_to_port(args[0], stdout); printf("\n");
    // debug_pattern_table(env, patterns);
    final = apply_transformation(env, args[0], patterns);
    // printf("final: "); print_syntax_to_port(final, stdout); printf("\n");
    return final;
}

MinimObject *minim_builtin_syntax_case(MinimEnv *env, size_t argc, MinimObject **args)
{
    SymbolList reserved;
    MinimObject *it, *datum;
    size_t resc;

    it = MINIM_STX_VAL(args[1]);
    resc = syntax_list_len(args[1]);
    init_symbol_list(&reserved, resc);
    for (size_t i = 0; i < resc; ++i)
    {
        reserved.syms[i] = MINIM_STX_SYMBOL(MINIM_CAR(it));
        it = MINIM_CDR(it);
    }

    datum = eval_ast_no_check(env, args[0]);
    for (size_t i = 2; i < argc; ++i)
    {
        MinimObject *match, *replace;
        MinimEnv *match_env;

        match_env = init_env(NULL);
        match = MINIM_STX_CAR(args[i]);
        replace = MINIM_STX_CADR(args[i]);
        if (match_transform(match_env, match, datum, &reserved, 0))
        {
            MinimEnv *env2;
            MinimObject *val, *trans;

            // printf("match:   "); print_syntax_to_port(match, stdout); printf("\n");
            // printf("replace: "); print_syntax_to_port(replace, stdout); printf("\n");

            env2 = init_env(env);
            env_merge_local_symbols(env2, match_env);
            env2->flags &= ~MINIM_ENV_TAIL_CALLABLE;
            val = eval_ast_no_check(env2, replace);
            if (!MINIM_OBJ_ASTP(val))
                THROW(env, minim_error("expected syntax as result", "syntax-case"));

            // printf("sc>: "); debug_print_minim_object(datum, NULL);
            // printf("sc<: "); debug_print_minim_object(val, NULL);
            trans = transform_syntax(env, val);
            return minim_ast(MINIM_STX_VAL(trans), MINIM_STX_LOC(args[0]));
        }
    }

    THROW(env, minim_syntax_error("bad syntax", NULL, datum_to_syntax(env, datum), NULL));
    return NULL;
}
