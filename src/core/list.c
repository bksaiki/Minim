#include <string.h>

#include "../gc/gc.h"
#include "arity.h"
#include "assert.h"
#include "builtin.h"
#include "jmp.h"
#include "error.h"
#include "lambda.h"
#include "list.h"
#include "number.h"

static MinimObject *eval_1ary(MinimEnv *env, MinimObject *proc, MinimObject *val)
{
    if (MINIM_OBJ_BUILTINP(proc))
    {
        MinimBuiltin func = MINIM_BUILTIN(proc);
        MinimObject *err;

        if (!minim_check_arity(func, 1, env, &err))
            THROW(env, err);
        return func(env, 1, &val);
    }
    else if (MINIM_OBJ_CLOSUREP(proc))
    {
        MinimLambda *lam = MINIM_CLOSURE(proc);
        return eval_lambda(lam, env, 1, &val);
    }
    else
    {
        // no return
        minim_long_jump(proc, env, 1, &val);
        return NULL;
    }
}

static MinimObject *eval_2ary(MinimEnv *env, MinimObject *proc, MinimObject *x, MinimObject *y)
{
    MinimObject **args;

    if (MINIM_OBJ_BUILTINP(proc))
    {
        MinimBuiltin func = MINIM_BUILTIN(proc);
        MinimObject *err;

        if (!minim_check_arity(func, 2, env, &err))
            THROW(env, err);

        args = GC_alloc(2 * sizeof(MinimObject*));
        args[0] = x;
        args[1] = y;
        return func(env, 2, args);
    }
    else if (MINIM_OBJ_CLOSUREP(proc))
    {
        MinimLambda *lam = MINIM_CLOSURE(proc);

        args = GC_alloc(2 * sizeof(MinimObject*));
        args[0] = x;
        args[1] = y;
        return eval_lambda(lam, env, 2, args);
    }
    else
    {
        args = GC_alloc(2 * sizeof(MinimObject*));
        args[0] = x;
        args[1] = y;
        
        // no return
        minim_long_jump(proc, env, 2, args);
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

static MinimObject *minim_list_remove(MinimObject *lst, MinimObject *rem)
{
    MinimObject *head, *it;

    if (minim_nullp(lst))
        return lst;

    head = NULL;
    for (MinimObject *it2 = lst; !minim_nullp(it2); it2 = MINIM_CDR(it2))
    {
        if (!minim_equalp(MINIM_CAR(it2), rem))
        {
            if (head)
            {
                MINIM_CDR(it) = minim_cons(MINIM_CAR(it2), NULL);
                it = MINIM_CDR(it);
            }
            else
            {
                head = minim_cons(MINIM_CAR(it2), NULL);
                it = head;
            }
        }
    }

    if (!head)  return minim_null;

    MINIM_CDR(it) = minim_null;
    return head;
}

static MinimObject *minim_list_map(MinimObject *lst, MinimObject *map, MinimEnv *env)
{
    MinimObject *head, *val, *it;

    if (minim_nullp(lst))
        return lst;

    head = NULL;
    for (MinimObject *it2 = lst; !minim_nullp(it2); it2 = MINIM_CDR(it2))
    {
        val = eval_1ary(env, map, MINIM_CAR(it2));
        if (head)
        {
            MINIM_CDR(it) = minim_cons(val, NULL);
            it = MINIM_CDR(it);
        }
        else
        {
            head = minim_cons(val, NULL);
            it = head;
        }
    }

    MINIM_CDR(it) = minim_null;
    return head;
}

static MinimObject *minim_list_filter(MinimObject *lst, MinimObject *filter, MinimEnv *env, bool negate)
{
    MinimObject *head, *val, *it;

    if (minim_nullp(lst))
        return lst;

    head = NULL;
    for (MinimObject *it2 = lst; !minim_nullp(it2); it2 = MINIM_CDR(it2))
    {
        val = eval_1ary(env, filter, MINIM_CAR(it2));
        if (negate != coerce_into_bool(val))    // adding to list
        {
            if (head)
            {
                MINIM_CDR(it) = minim_cons(MINIM_CAR(it2), NULL);
                it = MINIM_CDR(it);
            }
            else
            {
                head = minim_cons(MINIM_CAR(it2), NULL);
                it = head;
            }
        }
    }

    if (!head)  return minim_null;

    MINIM_CDR(it) = minim_null;
    return head;
}

static MinimObject *minim_list_foldl(MinimObject *lst, MinimObject *fold, MinimObject *init, MinimEnv *env)
{
    MinimObject *it;

    if (minim_nullp(lst))
        return init;

    // first step
    it = eval_2ary(env, fold, MINIM_CAR(lst), init);

    for (MinimObject *it2 = MINIM_CDR(lst); !minim_nullp(it2); it2 = MINIM_CDR(it2))
        it = eval_2ary(env, fold, MINIM_CAR(it2), it);

    return it;   
}

bool minim_consp(MinimObject* thing)
{
    return (MINIM_OBJ_PAIRP(thing) && MINIM_CAR(thing));
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

MinimObject *minim_list_ref(MinimObject *lst, size_t len)
{
    MinimObject *it;

    if (len == 0)
        return MINIM_CAR(lst);

    it = MINIM_CDR(lst);
    for (size_t i = 1; i < len; ++i, it = MINIM_CDR(it));
    return MINIM_CAR(it);
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

MinimObject *minim_builtin_caar(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!(minim_consp(args[0]) && minim_consp(MINIM_CAR(args[0]))))
        THROW(env, minim_argument_error("pair of (non empty pair . any)", "caar", 0, args[0]));
    
    return MINIM_CAAR(args[0]);
}

MinimObject *minim_builtin_cadr(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!(minim_consp(args[0]) && MINIM_CDR(args[0]) && minim_consp(MINIM_CDR(args[0]))))
        THROW(env, minim_argument_error("pair of (any, non-empty pair)", "cadr", 0, args[0]));
    
    return MINIM_CADR(args[0]);
}

MinimObject *minim_builtin_cdar(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!(minim_consp(args[0]) && minim_consp(MINIM_CAR(args[0]))))
        THROW(env, minim_argument_error("pair of (non-empty pair, any)", "cdar", 0, args[0]));
    
    return MINIM_CDAR(args[0]);
}

MinimObject *minim_builtin_cddr(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!(minim_consp(args[0]) && MINIM_CDR(args[0]) && minim_consp(MINIM_CDR(args[0]))))
        THROW(env, minim_argument_error("pair of (any, non-empty pair)", "cddr", 0, args[0]));

    return MINIM_CDDR(args[0]);
}

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

MinimObject *minim_builtin_head(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_listp(args[0]) || !MINIM_CAR(args[0]))
        THROW(env, minim_argument_error("non-empty list", "head", 0, args[0]));
        
    return MINIM_CAR(args[0]);
}

MinimObject *minim_builtin_tail(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *it;

    if (!minim_listp(args[0]) || !MINIM_CAR(args[0]))
        THROW(env, minim_argument_error("non-empty list", "tail", 0, args[0]));

    MINIM_TAIL(it, args[0]);
    return MINIM_CAR(it);
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

MinimObject *minim_builtin_remove(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_listp(args[1]))
        THROW(env, minim_argument_error("list", "remove", 1, args[1]));

    return minim_list_remove(args[1], args[0]);
}

MinimObject *minim_builtin_member(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_listp(args[1]))
        THROW(env, minim_argument_error("list", "member", 1, args[1]));
    
    if (minim_nullp(args[1]))
        return to_bool(0);

    for (MinimObject *it = args[1]; !minim_nullp(it); it = MINIM_CDR(it))
    {
        if (minim_equalp(MINIM_CAR(it), args[0]))
            return it;
    }

    return to_bool(0);
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

    return minim_list_map(args[1], args[0], env);   
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

MinimObject *minim_builtin_filter(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "filter", 0, args[0]));
    
    if (!minim_listp(args[1]))
        THROW(env, minim_argument_error("list", "filter", 1, args[1]));

    return minim_list_filter(args[1], args[0], env, false);
}

MinimObject *minim_builtin_filtern(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "filtern", 0, args[0]));
    
    if (!minim_listp(args[1]))
        THROW(env, minim_argument_error("list", "filtern", 1, args[1]));

    return minim_list_filter(args[1], args[0], env, true);
}

MinimObject *minim_builtin_foldl(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "foldl", 0, args[0]));
    
    if (!minim_listp(args[2]))
        THROW(env, minim_argument_error("list", "foldl", 2, args[2]));

    return minim_list_foldl(args[2], args[0], args[1], env);
}

static MinimObject *minim_foldr_h(MinimEnv *env, MinimObject *proc, MinimObject *li, MinimObject *init)
{
    MinimObject **vals;
    MinimObject *res, *tmp;

    if (!minim_nullp(MINIM_CDR(li)))
    {
        tmp = minim_foldr_h(env, proc, MINIM_CDR(li), init);
        vals = GC_alloc(2 * sizeof(MinimObject*));
        vals[0] = MINIM_CAR(li);
        vals[1] = tmp;

        if (MINIM_OBJ_BUILTINP(proc))
        {
            MinimBuiltin func = MINIM_BUILTIN(proc);
            if (!minim_check_arity(func, 2, env, &res))
                THROW(env, res);
            
                res = func(env, 2, vals);
        }
        else if MINIM_OBJ_CLOSUREP(proc)
        {
            MinimLambda *lam = MINIM_CLOSURE(proc);
            res = eval_lambda(lam, env, 2, vals);
        }
        else
        {
            // no return
            minim_long_jump(proc, env, 2, vals);
        }
    }
    else
    {
        vals = GC_alloc(2 * sizeof(MinimObject*));
        vals[0] = MINIM_CAR(li);
        vals[1] = init;

        if (MINIM_OBJ_BUILTINP(proc))
        {
            MinimBuiltin func = MINIM_BUILTIN(proc);
            if (!minim_check_arity(func, 2, env, &res))
                THROW(env, res);
            
            res = func(env, 2, vals);
        }
        else if MINIM_OBJ_CLOSUREP(proc)
        {
            MinimLambda *lam = MINIM_CLOSURE(proc);
            res = eval_lambda(lam, env, 2, vals);
        }
        else
        {
            // no return
            minim_long_jump(proc, env, 2, vals);
        }
    }

    return res;
}

MinimObject *minim_builtin_foldr(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        THROW(env, minim_argument_error("function", "foldr", 0, args[0]));
    
    if (!minim_listp(args[2]))
        THROW(env, minim_argument_error("list", "foldr", 2, args[2]));

    return (minim_nullp(args[2])) ? args[1] : minim_foldr_h(env, args[0], args[2], args[1]);
}

MinimObject *minim_builtin_assoc(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!minim_listof(args[1], minim_consp))
        THROW(env, minim_argument_error("list of pairs", "assoc", 1, args[1]));

    if (!minim_nullp(args[1]))
    {
        for (MinimObject *it = args[1]; it; it = MINIM_CDR(it))
        {
            if (minim_equalp(args[0], MINIM_CAAR(it)))
                return MINIM_CAR(it);
        }
    }

    return to_bool(0);
}
