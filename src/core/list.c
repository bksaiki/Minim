#include "minimpriv.h"

static MinimObject *eval_nary(MinimEnv *env, MinimObject *proc, size_t argc, MinimObject **conss)
{
    MinimObject **args;

    if (MINIM_OBJ_BUILTINP(proc))
    {
        MinimBuiltin func = MINIM_BUILTIN(proc);
        MinimObject *err;

        if (!minim_check_arity(func, argc, env, &err))
            THROW(env, err);

        args = GC_alloc(argc * sizeof(MinimObject*));
        for (size_t i = 0; i < argc; ++i)
            args[i] = MINIM_CAR(conss[i]);
        return func(env, argc, args);
    }
    else if (MINIM_OBJ_CLOSUREP(proc))
    {
        MinimLambda *lam = MINIM_CLOSURE(proc);

        args = GC_alloc(argc * sizeof(MinimObject*));
        for (size_t i = 0; i < argc; ++i)
            args[i] = MINIM_CAR(conss[i]);
        return eval_lambda(lam, env, argc, args);
    }
    else
    {
        args = GC_alloc(argc * sizeof(MinimObject*));
        for (size_t i = 0; i < argc; ++i)
            args[i] = MINIM_CAR(conss[i]);
        
        // no return
        minim_long_jump(proc, env, argc, args);
        return NULL;
    }
}

static MinimObject *minim_list_append(size_t count, MinimObject **lsts)
{
    MinimObject *head, *it;

    head = NULL;
    for (size_t i = 0; i < count; ++i)
    {
        if (head)
        {
            if (!minim_nullp(lsts[i]))
            {
                MINIM_CDR(it) = lsts[i];
                MINIM_TAIL(it, it);
            }
        }
        else
        {
            if (!minim_nullp(lsts[i]))
            {
                head = lsts[i];
                MINIM_TAIL(it, head);
            }
        }
    }

    return (head ? head : minim_null);
}

static MinimObject *minim_list_reverse2(MinimObject *lst, MinimObject *curr)
{
    return (minim_nullp(lst) ? curr : minim_list_reverse2(MINIM_CDR(lst), minim_cons(MINIM_CAR(lst), curr)));
}

static MinimObject *minim_list_reverse(MinimObject *lst)
{
    if (minim_nullp(lst))
        return minim_null;

    return minim_list_reverse2(lst, minim_null);
}

static bool map_iters_nullp(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (minim_nullp(args[0]))
    {
        for (size_t i = 1; i < argc; ++i)
        {
            if (!minim_nullp(args[i]))
                THROW(env, minim_error("given lists of uneven lengths", "map"));
        }

        return true;
    }
    else
    {
        for (size_t i = 1; i < argc; ++i)
        {
            if (minim_nullp(args[i]))
                THROW(env, minim_error("given lists of uneven lengths", "map"));
        }

        return false;
    }
}

static MinimObject *minim_list_map(MinimEnv *env, MinimObject *map, size_t argc, MinimObject **args)
{
    MinimObject *head, *val, *c;
    MinimObject **it;

    if (map_iters_nullp(env, argc, args))
        return minim_null;

    it = GC_alloc(argc * sizeof(MinimObject**));
    for (size_t i = 0; i < argc; ++i)
        it[i] = args[i];

    c = NULL;
    head = NULL;
    while (!map_iters_nullp(env, argc, it))
    {
        val = eval_nary(env, map, argc, it);
        if (head)
        {
            MINIM_CDR(c) = minim_cons(val, NULL);
            c = MINIM_CDR(c);
        }
        else
        {
            head = minim_cons(val, NULL);
            c = head;
        }

        for (size_t i = 0; i < argc; ++i)
            it[i] = MINIM_CDR(it[i]);
    }
    
    MINIM_CDR(c) = minim_null;
    return head;
}

static MinimObject *minim_list_andmap(MinimEnv *env, MinimObject *map, size_t argc, MinimObject **args)
{
    MinimObject *val;
    MinimObject **it;

    if (map_iters_nullp(env, argc, args))
        return minim_null;

    it = GC_alloc(argc * sizeof(MinimObject**));
    for (size_t i = 0; i < argc; ++i)
        it[i] = args[i];

    while (!map_iters_nullp(env, argc, it))
    {
        val = eval_nary(env, map, argc, it);
        if (minim_falsep(val))  return val;

        for (size_t i = 0; i < argc; ++i)
            it[i] = MINIM_CDR(it[i]);
    }
    
    return minim_true;
}

static MinimObject *minim_list_ormap(MinimEnv *env, MinimObject *map, size_t argc, MinimObject **args)
{
    MinimObject *val;
    MinimObject **it;

    if (map_iters_nullp(env, argc, args))
        return minim_null;

    it = GC_alloc(argc * sizeof(MinimObject**));
    for (size_t i = 0; i < argc; ++i)
        it[i] = args[i];

    while (!map_iters_nullp(env, argc, it))
    {
        val = eval_nary(env, map, argc, it);
        if (!minim_falsep(val))  return minim_true;

        for (size_t i = 0; i < argc; ++i)
            it[i] = MINIM_CDR(it[i]);
    }
    
    return minim_false;
}

static void assert_pair_type(MinimEnv *env, const char *name, const char *desc, MinimObject *val, size_t depth, ...)
{
    MinimObject *it;
    va_list va;
    int direction;

    it = val;
    if (!minim_consp(it))
        THROW(env, minim_argument_error(desc, name, 0, val));

    va_start(va, depth);
    for (size_t i = 0; i < depth; ++i)
    {
        direction = va_arg(va, int);
        if (direction)
        {
            if (minim_consp(MINIM_CDR(it)))     it = MINIM_CDR(it);
            else                                THROW(env, minim_argument_error(desc, name, 0, val));
        }
        else
        {
            if (minim_consp(MINIM_CAR(it)))     it = MINIM_CAR(it);
            else                                THROW(env, minim_argument_error(desc, name, 0, val));
        }
    }


    va_end(va);
}

//
//  Internals
//

bool minim_consp(MinimObject *thing)
{
    return MINIM_OBJ_PAIRP(thing);
}

// list |= (obj? . list?)
//      |= (obj? . null)
bool minim_listp(MinimObject* thing)
{
    if (minim_nullp(thing))          return true;
    if (MINIM_OBJ_PAIRP(thing))     return minim_listp(MINIM_CDR(thing));
    
    return false;
}

bool minim_listof(MinimObject* list, MinimPred pred)
{
    if (minim_nullp(list)) // nullary
        return true;

    for (MinimObject *it = list; !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (!pred(MINIM_CAR(it)))
            return false;
    }

    return true;
}

bool minim_cons_eqp(MinimObject *a, MinimObject *b)
{
    bool nx = minim_nullp(a);
    bool ny = minim_nullp(b);
    
    if (nx && ny)     return true;
    if (nx != ny)     return false;

    if (!minim_equalp(MINIM_CAR(a), MINIM_CAR(b)))
        return false;
    
    if (MINIM_CDR(a) && MINIM_CDR(b))
        return minim_equalp(MINIM_CDR(a), MINIM_CDR(b));

    return !MINIM_CDR(a) && !MINIM_CDR(b);
}

void minim_cons_to_bytes(MinimObject *obj, Buffer *bf)
{
    for (MinimObject *it = obj; !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (MINIM_CAR(it))
        {
            Buffer *sbf = minim_obj_to_bytes(MINIM_CAR(it));
            writeb_buffer(bf, sbf);
        }
    }
}

MinimObject *minim_list(MinimObject **args, size_t len)
{
    MinimObject *head, *it;

    if (len == 0)
        return minim_null;

    head = minim_cons(args[0], NULL);
    it = head;
    for (size_t i = 1; i < len; ++i)
    {
        MINIM_CDR(it) = minim_cons(args[i], NULL);
        it = MINIM_CDR(it);
    }

    MINIM_CDR(it) = minim_null;
    return head;
}

MinimObject *minim_list_append2(MinimObject *a, MinimObject *b)
{
    MinimObject *tail;
    MINIM_TAIL(tail, a);
    MINIM_CDR(tail) = b;
    return a;
}

MinimObject *minim_list_drop(MinimObject *lst, size_t n)
{
    MinimObject *it;

    if (n == 0)
        return lst;

    it = MINIM_CDR(lst);
    for (size_t i = 1; i < n; ++i)
        it = MINIM_CDR(it);
    return it;
}

size_t minim_list_length(MinimObject *lst)
{
    size_t len = 0;
    for (MinimObject *it = lst; !minim_nullp(it); it = MINIM_CDR(it))
        ++len;
    return len;
}

//
//  Builtins
//

MinimObject *minim_builtin_cons(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_cons(args[0], args[1]);
}

MinimObject *minim_builtin_consp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_consp(args[0]));
}

// depth 1 accessors

MinimObject *minim_builtin_car(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_consp(args[0]))
        THROW(env, minim_argument_error("pair", "car", 0, args[0]));
    
    if (!MINIM_CAR(args[0]))
        THROW(env, minim_argument_error("non-empty list", "car", 0, args[0]));

    return MINIM_CAR(args[0]);
}

MinimObject *minim_builtin_cdr(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_consp(args[0]))
        THROW(env, minim_argument_error("pair", "cdr", 0, args[0]));
    
    if (!MINIM_CAR(args[0]))
        THROW(env, minim_argument_error("non-empty list", "cdr", 0, args[0]));

    return MINIM_CDR(args[0]);
}

// depth 2 accessors

MinimObject *minim_builtin_caar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caar", "pair of (non empty pair . any)", args[0], 1, 0);
    return MINIM_CAAR(args[0]);
}

MinimObject *minim_builtin_cadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cadr", "pair of (any . non-empty pair)", args[0], 1, 1);
    return MINIM_CADR(args[0]);
}

MinimObject *minim_builtin_cdar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdar", "pair of (non empty pair . any)", args[0], 1, 0);
    return MINIM_CDAR(args[0]);
}

MinimObject *minim_builtin_cddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cddr", "pair of (any . non-empty pair)", args[0], 1, 1);
    return MINIM_CDDR(args[0]);
}

// depth 3 accessors

MinimObject *minim_builtin_caaar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caaar", "pair of (pair of (non empty pair . any) . any)", args[0], 2, 0, 0);
    return MINIM_CAR(MINIM_CAAR(args[0]));
}

MinimObject *minim_builtin_caadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caadr", "pair of (any . pair of (non empty pair . any))", args[0], 2, 1, 0);
    return MINIM_CAR(MINIM_CADR(args[0]));
}

MinimObject *minim_builtin_cadar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cadar", "pair of (pair of (any . non-empty pair) . any)", args[0], 2, 0, 1);
    return MINIM_CAR(MINIM_CDAR(args[0]));
}

MinimObject *minim_builtin_caddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caddr", "pair of (any . pair of (any . non empty pair))", args[0], 2, 1, 1);
    return MINIM_CAR(MINIM_CDDR(args[0]));
}

MinimObject *minim_builtin_cdaar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdaar", "pair of (pair of (non empty pair . any) . any)", args[0], 2, 0, 0);
    return MINIM_CDR(MINIM_CAAR(args[0]));
}

MinimObject *minim_builtin_cdadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdadr", "pair of (any . pair of (non empty pair . any))", args[0], 2, 1, 0);
    return MINIM_CDR(MINIM_CADR(args[0]));
}

MinimObject *minim_builtin_cddar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cddar", "pair of (pair of (any . non-empty pair) . any)", args[0], 2, 0, 1);
    return MINIM_CDR(MINIM_CDAR(args[0]));
}

MinimObject *minim_builtin_cdddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdddr", "pair of (any . pair of (any . non empty pair))", args[0], 2, 1, 1);
    return MINIM_CDR(MINIM_CDDR(args[0]));
}

// depth 4 accessors

MinimObject *minim_builtin_caaaar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caaaar", "pair of (pair of (pair of (non empty pair . any) . any) . any)", args[0], 3, 0, 0, 0);
    return MINIM_CAAR(MINIM_CAAR(args[0]));
}

MinimObject *minim_builtin_caaadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caaadr", "pair of (any . pair of (pair of (non empty pair . any) . any))", args[0], 3, 1, 0, 0);
    return MINIM_CAAR(MINIM_CADR(args[0]));
}

MinimObject *minim_builtin_caadar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caadar", "pair of (pair of (any . pair of (non empty pair . any)) . any)", args[0], 3, 0, 1, 0);
    return MINIM_CAAR(MINIM_CDAR(args[0]));
}

MinimObject *minim_builtin_caaddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caaddr", "pair of (any . pair of (any . pair of (non empty pair . any)))", args[0], 3, 1, 1, 0);
    return MINIM_CAAR(MINIM_CDDR(args[0]));
}

MinimObject *minim_builtin_cadaar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cadaar", "pair of (pair of (pair of (any . non empty pair) . any) . any)", args[0], 3, 0, 0, 0);
    return MINIM_CADR(MINIM_CAAR(args[0]));
}

MinimObject *minim_builtin_cadadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cadadr", "pair of (any . pair of (pair of (any . non empty pair) . any))", args[0], 3, 1, 0, 0);
    return MINIM_CADR(MINIM_CADR(args[0]));
}

MinimObject *minim_builtin_caddar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "caddar", "pair of (pair of (any . pair of (any . non empty pair)) . any)", args[0], 3, 0, 1, 0);
    return MINIM_CADR(MINIM_CDAR(args[0]));
}

MinimObject *minim_builtin_cadddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cadddr", "pair of (any . pair of (any . pair of (any . non empty pair)))", args[0], 3, 1, 1, 0);
    return MINIM_CADR(MINIM_CDDR(args[0]));
}

MinimObject *minim_builtin_cdaaar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdaaar", "pair of (pair of (pair of (non empty pair . any) . any) . any)", args[0], 3, 0, 0, 1);
    return MINIM_CDAR(MINIM_CAAR(args[0]));
}

MinimObject *minim_builtin_cdaadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdaadr", "pair of (any . pair of (pair of (non empty pair . any) . any))", args[0], 3, 1, 0, 1);
    return MINIM_CDAR(MINIM_CADR(args[0]));
}

MinimObject *minim_builtin_cdadar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdadar", "pair of (pair of (any . pair of (non empty pair . any)) . any)", args[0], 3, 0, 1, 1);
    return MINIM_CDAR(MINIM_CDAR(args[0]));
}

MinimObject *minim_builtin_cdaddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdaddr", "pair of (any . pair of (any . pair of (non empty pair . any)))", args[0], 3, 1, 1, 1);
    return MINIM_CDAR(MINIM_CDDR(args[0]));
}

MinimObject *minim_builtin_cddaar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cddaar", "pair of (pair of (pair of (any . non empty pair) . any) . any)", args[0], 3, 0, 0, 1);
    return MINIM_CDDR(MINIM_CAAR(args[0]));
}

MinimObject *minim_builtin_cddadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cddadr", "pair of (any . pair of (pair of (any . non empty pair) . any))", args[0], 3, 1, 0, 1);
    return MINIM_CDDR(MINIM_CADR(args[0]));
}

MinimObject *minim_builtin_cdddar(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cdddar", "pair of (pair of (any . pair of (any . non empty pair)) . any)", args[0], 3, 0, 1, 1);
    return MINIM_CDDR(MINIM_CDAR(args[0]));
}

MinimObject *minim_builtin_cddddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    assert_pair_type(env, "cddddr", "pair of (any . pair of (any . pair of (any . non empty pair)))", args[0], 3, 1, 1, 1);
    return MINIM_CDDR(MINIM_CDDR(args[0]));
}

// lists

MinimObject *minim_builtin_listp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_listp(args[0]));
}

MinimObject *minim_builtin_nullp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(minim_nullp(args[0]));
}

MinimObject *minim_builtin_list(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_list(args, argc);
}

MinimObject *minim_builtin_length(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_listp(args[0]))
        THROW(env, minim_argument_error("list", "length", 0, args[0]));

    size_t len = minim_list_length(args[0]);
    return uint_to_minim_number(len);
}

MinimObject *minim_builtin_append(MinimEnv *env, size_t argc, MinimObject **args)
{
    for (size_t i = 0; i < argc; ++i)
    {
        if (!minim_listp(args[i]))
            THROW(env, minim_argument_error("list", "append", i, args[i]));
    }

    return minim_list_append(argc, args);
}

MinimObject *minim_builtin_reverse(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_listp(args[0]))
        THROW(env, minim_argument_error("list", "reverse", 0, args[0]));

    return minim_list_reverse(args[0]);
}

MinimObject *minim_builtin_list_ref(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *it;
    size_t idx;

    if (!minim_listp(args[0]))
        THROW(env, minim_argument_error("list", "list-ref", 0, args[0]));

    if (!minim_exact_nonneg_intp(args[1]))
        THROW(env, minim_argument_error("exact non-negative-integer", "list-ref", 1, args[1]));

    it = args[0];
    idx = MINIM_NUMBER_TO_UINT(args[1]);

    for (size_t i = 0; i < idx; ++i, it = MINIM_CDR(it))
    {
        if (minim_nullp(it))
            THROW(env, minim_error("index out of bounds: ~u", "list-ref", idx));    
    }

    return MINIM_CAR(it);
}

MinimObject *minim_builtin_map(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "map", 0, args[0]));
    
    if (!minim_listp(args[1]))
        THROW(env, minim_argument_error("list", "map", 1, args[1]));

    return minim_list_map(env, args[0], argc - 1, &args[1]);
}

MinimObject *minim_builtin_andmap(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "andmap", 0, args[0]));
    
    if (!minim_listp(args[1]))
        THROW(env, minim_argument_error("list", "andmap", 1, args[1]));

    return minim_list_andmap(env, args[0], argc - 1, &args[1]);
}

MinimObject *minim_builtin_ormap(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "ormap", 0, args[0]));
    
    if (!minim_listp(args[1]))
        THROW(env, minim_argument_error("list", "ormap", 1, args[1]));

    return minim_list_ormap(env, args[0], argc - 1, &args[1]);
}

MinimObject *minim_builtin_apply(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *it, **vals;
    size_t i, len, valc;

    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "apply", 0, args[0]));
    
    if (!minim_listp(args[argc - 1]))
        THROW(env, minim_argument_error("list", "apply", argc - 1, args[argc - 1]));

    len = minim_list_length(args[argc - 1]);
    valc = len + argc - 2;
    vals = GC_alloc(valc * sizeof(MinimObject*));

    for (i = 0; i < argc - 2; ++i)
        vals[i] = args[i + 1];

    it = args[argc - 1];
    for (; i < valc; ++i, it = MINIM_CDR(it))
        vals[i] = MINIM_CAR(it);

    if (MINIM_OBJ_BUILTINP(args[0]))
    {
        MinimBuiltin func = MINIM_BUILTIN(args[0]);
        if (!minim_check_arity(func, valc, env, &res))
            THROW(env, res);
        
            res = func(env, valc, vals);
    }
    else if (MINIM_OBJ_CLOSUREP(args[0]))
    {
        MinimLambda *lam = MINIM_CLOSURE(args[0]);
        res = eval_lambda(lam, env, valc, vals);
    }
    else
    {
        // no return
        minim_long_jump(args[0], env, valc, vals);
        return NULL;
    }

    return res;
}
