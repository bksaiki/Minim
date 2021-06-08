#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../gc/gc.h"
#include "arity.h"
#include "assert.h"
#include "builtin.h"
#include "error.h"
#include "lambda.h"
#include "list.h"
#include "number.h"

static MinimObject *reverse_lists(MinimObject* head, MinimObject* tail, size_t count)
{
    if (count <= 1)
    {
        MINIM_CDR(head) = tail;
        return head;
    }
    else
    {
        MinimObject* cdr = MINIM_CDR(head);
        MINIM_CDR(head) = tail;
        return reverse_lists(cdr, head, count - 1);
    }
}

static MinimObject *construct_list_map(MinimObject* list, MinimObject* map, MinimEnv *env, size_t len)
{
    MinimObject *node, *val, *rest;

    if (len == 0)
        return NULL;

    if (map->type == MINIM_OBJ_FUNC)
    {
        MinimObject *arg = MINIM_CAR(list);
        MinimBuiltin func = map->u.ptrs.p1;

        if (!minim_check_arity(func, 1, env, &node))
            return node;
        
        val = func(env, &arg, 1);
    }
    else // map->type == MINIM_OBJ_CLOSURE
    {
        MinimObject *arg = MINIM_CAR(list);
        MinimLambda *lam = map->u.ptrs.p1;
        
        val = eval_lambda(lam, env, &arg, 1);
    }

    if (val->type == MINIM_OBJ_ERR)
        return val;

    rest = construct_list_map(MINIM_CDR(list), map, env, len - 1);
    if (rest && rest->type == MINIM_OBJ_ERR)
        return rest;

    init_minim_object(&node, MINIM_OBJ_PAIR, val, rest);
    return node;
}

static MinimObject *filter_list(MinimObject *list, MinimObject *filter, MinimEnv *env, bool negate)
{
    MinimObject *res, *it, *it2, *val;
    bool first = true;

    if (minim_nullp(list))
        return list;

    it = list;
    while (it != NULL)
    {
        if (filter->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = filter->u.ptrs.p1;
            if (minim_check_arity(func, 1, env, &val))
                val = func(env, &MINIM_CAR(it), 1);
        }
        else
        {
            MinimLambda *lam = filter->u.ptrs.p1;
            val = eval_lambda(lam, env, &MINIM_CAR(it), 1);
        }

        if (MINIM_OBJ_THROWNP(val))
             return val;

        if (negate != coerce_into_bool(val))
        {
            init_minim_object(&val, MINIM_OBJ_PAIR, MINIM_CAR(it), NULL);
            if (first)
            {
                first = false;
                res = val;
            }
            else
            {
                MINIM_CDR(it2) = val;
            }

            it2 = val;
        }


        val = it;
        it = MINIM_CDR(it);
    }

    if (first) init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    return res;
}

bool minim_consp(MinimObject* thing)
{
    return (thing->type == MINIM_OBJ_PAIR && MINIM_CAR(thing));
}

bool minim_listp(MinimObject* thing)
{
    return (thing->type == MINIM_OBJ_PAIR && (!MINIM_CAR(thing) || !MINIM_CDR(thing) ||
            MINIM_CDR(thing)->type == MINIM_OBJ_PAIR));
}

bool minim_nullp(MinimObject* thing)
{
    return (thing->type == MINIM_OBJ_PAIR && !MINIM_CAR(thing) && !MINIM_CDR(thing));
}

bool minim_listof(MinimObject* list, MinimPred pred)
{
    if (minim_nullp(list)) // nullary
        return true;

    for (MinimObject *it = list; it; it = MINIM_CDR(it))
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
    for (MinimObject *it = obj; it != NULL; it = MINIM_CDR(it))
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
    {
        init_minim_object(&head, MINIM_OBJ_PAIR, NULL, NULL);
    }
    else
    {
        init_minim_object(&it, MINIM_OBJ_PAIR, args[0], NULL);
        head = it;
        for (size_t i = 1; i < len; ++i)
        {
            init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, args[i], NULL);
            it = MINIM_CDR(it);
        }
    }

    return head;
}

size_t minim_list_length(MinimObject *list)
{
    size_t len = 0;

    for (MinimObject *it = list; it != NULL; it = MINIM_CDR(it))
        if (MINIM_CAR(it))  ++len;
    
    return len;
}

//
//  Builtins
//

MinimObject *minim_builtin_cons(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *car, *cdr;

    car = args[0];
    if (minim_nullp(args[1]))
    {
        init_minim_object(&res, MINIM_OBJ_PAIR, car, NULL);
    }
    else
    {
        cdr = args[1];
        init_minim_object(&res, MINIM_OBJ_PAIR, car, cdr);
    }

    return res;
}

MinimObject *minim_builtin_consp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_consp(args[0]));
    return res;
}

MinimObject *minim_builtin_car(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!minim_consp(args[0]))
        return minim_argument_error("pair", "car", 0, args[0]);
    
    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "car", 0, args[0]);

    return MINIM_CAR(args[0]);
}

MinimObject *minim_builtin_cdr(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    
    if (!minim_consp(args[0]))
        return minim_argument_error("pair", "cdr", 0, args[0]);
    
    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "cdr", 0, args[0]);

    if (MINIM_CDR(args[0]))
        res = MINIM_CDR(args[0]);
    else
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);

    return res;
}

MinimObject *minim_builtin_caar(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!(minim_consp(args[0]) && minim_consp(MINIM_CAR(args[0]))))
        return minim_argument_error("pair of (non empty pair . any)", "caar", 0, args[0]);
    
    return MINIM_CAAR(args[0]);
}

MinimObject *minim_builtin_cadr(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!(minim_consp(args[0]) && MINIM_CDR(args[0]) && minim_consp(MINIM_CDR(args[0]))))
        return minim_argument_error("pair of (any, non-empty pair)", "cadr", 0, args[0]);
    
    return MINIM_CADR(args[0]);
}

MinimObject *minim_builtin_cdar(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!(minim_consp(args[0]) && minim_consp(MINIM_CAR(args[0]))))
        return minim_argument_error("pair of (non-empty pair, any)", "cdar", 0, args[0]);
    
    if (MINIM_CDAR(args[0]))
        res = MINIM_CDAR(args[0]);
    else
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);

    return res;
}

MinimObject *minim_builtin_cddr(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!(minim_consp(args[0]) && MINIM_CDR(args[0]) && minim_consp(MINIM_CDR(args[0]))))
        return minim_argument_error("pair of (any, non-empty pair)", "cddr", 0, args[0]);

    if (MINIM_CDDR(args[0]))
        res = MINIM_CDDR(args[0]);
    else
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);

    return res;
}

MinimObject *minim_builtin_listp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_listp(args[0]));
    return res;
}

MinimObject *minim_builtin_nullp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_nullp(args[0]));
    return res;
}

MinimObject *minim_builtin_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    return minim_list(args, argc);
}

MinimObject *minim_builtin_head(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "head", 0, args[0]);
        
    return MINIM_CAR(args[0]);
}

MinimObject *minim_builtin_tail(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *it;

    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "tail", 0, args[0]);

    MINIM_TAIL(it, args[0]);
    return MINIM_CAR(it);
}

MinimObject *minim_builtin_length(MinimEnv *env, MinimObject **args, size_t argc)
{
    size_t len = minim_list_length(args[0]);
    
    return uint_to_minim_number(len);
}

MinimObject *minim_builtin_append(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it;

    for (size_t i = 0; i < argc; ++i)
    {
        if (!minim_listp(args[i]))
            return minim_argument_error("list", "append", i, args[i]);
    }

    it = NULL;
    for (size_t i = 0; i < argc; ++i)
    {
        if (!minim_nullp(args[i]))
        {
            if (!it)
            {
                it = args[i];
                res = it;
            }
            else
            {
                MINIM_CDR(it) = args[i];
            }

            MINIM_TAIL(it, it);
        }
    }

    if (!it) init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    return res;
}

MinimObject *minim_builtin_reverse(MinimEnv *env, MinimObject **args, size_t argc)
{
    size_t len;

    if (!minim_listp(args[0]))
        return minim_argument_error("list", "reverse", 0, args[0]);

    len = minim_list_length(args[0]);
    return reverse_lists(args[0], NULL, len);
}

MinimObject *minim_builtin_remove(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *tmp;

    if (!minim_listp(args[1]))
        return minim_argument_error("list", "remove", 1, args[1]);

    if (minim_nullp(args[1]))
        return args[1];

    res = args[1];
    if (minim_equalp(MINIM_CAR(res), args[0]))
    {
        tmp = res;
        if (MINIM_CDR(tmp))
        {
            res = MINIM_CDR(tmp);
            MINIM_CDR(tmp) = NULL;
        }
        else
        {
            init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
        }
    }

    tmp = res;
    for (MinimObject *it = MINIM_CDR(res); it; it = MINIM_CDR(it))
    {
        if (minim_equalp(MINIM_CAR(it), args[0]))
        {
            MINIM_CDR(tmp) = MINIM_CDR(it);
            it = tmp;
        }
        else
        {
            tmp = it;
        }
    }

    return res;
}

MinimObject *minim_builtin_member(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!minim_listp(args[1]))
        return minim_argument_error("list", "member", 1, args[1]);

    for (MinimObject *it = args[1]; MINIM_CAR(it); it = MINIM_CDR(it))
    {
        if (minim_equalp(MINIM_CAR(it), args[0]))
            return it;
    }


    init_minim_object(&res, MINIM_OBJ_BOOL, 0);
    return res;
}

MinimObject *minim_builtin_list_ref(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *it;
    size_t idx;

    if (!minim_listp(args[0]))
        return minim_argument_error("list", "list-ref", 0, args[0]);

    if (!minim_exact_nonneg_intp(args[1]))
        return minim_argument_error("exact non-negative-integer", "list-ref", 1, args[1]);

    it = args[0];
    idx = MINIM_NUMBER_TO_UINT(args[1]);

    for (size_t i = 0; it && i < idx; ++i)
        it = MINIM_CDR(it);

    if (!it)
        return minim_error("index out of bounds: ~u", "list-ref", idx);
    
    return MINIM_CAR(it);
}

MinimObject *minim_builtin_map(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    size_t len;

    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "map", 0, args[0]);
    
    if (!minim_listp(args[1]))
        return minim_argument_error("list", "map", 1, args[1]);

    len = minim_list_length(args[1]);
    if (len == 0)   init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    else            res = construct_list_map(args[1], args[0], env, len);

    return res;
}

MinimObject *minim_builtin_apply(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it, **vals;
    size_t i, len, valc;

    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "apply", 0, args[0]);
    
    if (!minim_listp(args[argc - 1]))
        return minim_argument_error("list", "apply", argc - 1, args[argc - 1]);

    len = minim_list_length(args[argc - 1]);
    valc = len + argc - 2;
    vals = GC_alloc(valc * sizeof(MinimObject*));

    for (i = 0; i < argc - 2; ++i)
        vals[i] = args[i + 1];

    it = args[argc - 1];
    for (; i < valc; ++i, it = MINIM_CDR(it))
        vals[i] = MINIM_CAR(it);

    if (args[0]->type == MINIM_OBJ_FUNC)
    {
        MinimBuiltin func = args[0]->u.ptrs.p1;
        if (minim_check_arity(func, valc, env, &res))
            res = func(env, vals, valc);
    }
    else // MINIM_OBJ_CLOSURE
    {
        MinimLambda *lam = args[0]->u.ptrs.p1;
        res = eval_lambda(lam, env, vals, valc);
    }

    return res;
}

MinimObject *minim_builtin_filter(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "filter", 0, args[0]);
    
    if (!minim_listp(args[1]))
        return minim_argument_error("list", "filter", 1, args[1]);

    return filter_list(args[1], args[0], env, false);
}

MinimObject *minim_builtin_filtern(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "filtern", 0, args[0]);
    
    if (!minim_listp(args[1]))
        return minim_argument_error("list", "filtern", 1, args[1]);

    return filter_list(args[1], args[0], env, true);
}

MinimObject *minim_builtin_foldl(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "foldl", 0, args[0]);
    
    if (!minim_listp(args[2]))
        return minim_argument_error("list", "foldl", 2, args[2]);

    if (minim_nullp(args[2]))
    {
        res = args[1];
    }
    else
    {
        MinimObject **vals = GC_alloc(2 * sizeof(MinimObject*));

        vals[1] = args[1];
        if (args[0]->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = args[0]->u.ptrs.p1;
            for (MinimObject *it = args[2]; it; it = MINIM_CDR(it))
            {
                vals[0] = MINIM_CAR(it);
                if (minim_check_arity(func, 2, env, &vals[1]))
                    vals[1] = func(env, vals, 2);
                    
                if (MINIM_OBJ_THROWNP(vals[1])) break;
            }
        }
        else
        {
            MinimLambda *lam = args[0]->u.ptrs.p1;
            for (MinimObject *it = args[2]; it; it = MINIM_CDR(it))
            {
                vals[0] = MINIM_CAR(it);
                vals[1] = eval_lambda(lam, env, vals, 2);

                if (vals[1]->type == MINIM_OBJ_ERR)
                    break;
            }
        }

        res = vals[1];
    }

    return res;
}

static MinimObject *minim_foldr_h(MinimEnv *env, MinimObject *proc, MinimObject *li, MinimObject *init)
{
    MinimObject **vals;
    MinimObject *res, *tmp;

    if (MINIM_CDR(li))
    {
        tmp = minim_foldr_h(env, proc, MINIM_CDR(li), init);
        if (tmp->type == MINIM_OBJ_ERR)
            return tmp;

        vals = GC_alloc(2 * sizeof(MinimObject*));
        vals[0] = MINIM_CAR(li);
        vals[1] = tmp;

        if (proc->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = proc->u.ptrs.p1;
            if (minim_check_arity(func, 2, env, &res))
                res = func(env, vals, 2);
        }
        else
        {
            MinimLambda *lam = proc->u.ptrs.p1;
            res = eval_lambda(lam, env, vals, 2);
        }
    }
    else
    {
        vals = GC_alloc(2 * sizeof(MinimObject*));
        vals[0] = MINIM_CAR(li);
        vals[1] = init;

        if (proc->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = proc->u.ptrs.p1;
            if (minim_check_arity(func, 2, env, &res))
                res = func(env, vals, 2);
        }
        else
        {
            MinimLambda *lam = proc->u.ptrs.p1;
            res = eval_lambda(lam, env, vals, 2);
        }
    }

    return res;
}

MinimObject *minim_builtin_foldr(MinimEnv *env, MinimObject **args, size_t argc)
{
    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "foldr", 0, args[0]);
    
    if (!minim_listp(args[2]))
        return minim_argument_error("list", "foldr", 2, args[2]);

    return (minim_nullp(args[2])) ? args[1] : minim_foldr_h(env, args[0], args[2], args[1]);
}

MinimObject *minim_builtin_assoc(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!minim_listof(args[1], minim_consp))
        return minim_argument_error("list of pairs", "assoc", 1, args[1]);

    if (!minim_nullp(args[1]))
    {
        for (MinimObject *it = args[1]; it; it = MINIM_CDR(it))
        {
            if (minim_equalp(args[0], MINIM_CAAR(it)))
                return MINIM_CAR(it);
        }
    }

    init_minim_object(&res, MINIM_OBJ_BOOL, 0);
    return res;
}
