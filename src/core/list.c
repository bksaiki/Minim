#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
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
    {  
        free_minim_object(val);
        return rest;
    }

    init_minim_object(&node, MINIM_OBJ_PAIR, val, rest);
    return node;
}

static MinimObject *filter_list(MinimObject *list, MinimObject *filter, MinimEnv *env, bool negate)
{
    MinimObject *res, *it, *it2, *val;
    bool first = true, use = false,
         owns = MINIM_OBJ_OWNERP(list);

    if (minim_nullp(list))
        return fresh_minim_object(list);

    it = list;
    while (it != NULL)
    {
        if (filter->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = filter->u.ptrs.p1;
            val = func(env, &MINIM_CAR(it), 1);
        }
        else
        {
            MinimLambda *lam = filter->u.ptrs.p1;
            val = eval_lambda(lam, env, &MINIM_CAR(it), 1);
        }

        if (val->type == MINIM_OBJ_ERR)
        {
            return val;
        }

        use = negate != coerce_into_bool(val);
        if (use)
        {
            free_minim_object(val);
            init_minim_object(&val, MINIM_OBJ_PAIR, (owns ? MINIM_CAR(it) : copy2_minim_object(MINIM_CAR(it))), NULL);

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
        else
        {
            free_minim_object(val);
        }


        val = it;
        it = MINIM_CDR(it);

        if (owns)
        {
            if (use)    MINIM_CAR(val) = NULL;
            MINIM_CDR(val) = NULL;
            free_minim_object(val);
        }
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

static bool minim_cons_eqp_h(MinimObject *a, MinimObject *b)
{
    if (!minim_equalp(MINIM_CAR(a), MINIM_CAR(b)))
        return false;

    if (MINIM_CDR(a) && MINIM_CDR(b))   return minim_cons_eqp(MINIM_CDR(a), MINIM_CDR(b));
    if (!MINIM_CDR(a) && !MINIM_CDR(b)) return true;
    return false;
}

bool minim_cons_eqp(MinimObject *a, MinimObject *b)
{
    bool x = minim_nullp(a), y = minim_nullp(b);
    
    if (x && y)     return true;
    if (x != y)     return false;
    return minim_cons_eqp_h(a, b);
}

void minim_cons_to_bytes(MinimObject *obj, Buffer *bf)
{
    for (MinimObject *it = obj; it != NULL; it = MINIM_CDR(it))
    {
        if (MINIM_CAR(it))
        {
            Buffer *sbf = minim_obj_to_bytes(MINIM_CAR(it));
            writeb_buffer(bf, sbf);
            free_buffer(sbf);
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
        init_minim_object(&it, MINIM_OBJ_PAIR, fresh_minim_object(args[0]), NULL);
        head = it;
        for (size_t i = 1; i < len; ++i)
        {
            init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, fresh_minim_object(args[i]), NULL);
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

    OPT_MOVE(car, args[0]);
    if (minim_nullp(args[1]))
    {
        init_minim_object(&res, MINIM_OBJ_PAIR, car, NULL);
    }
    else
    {
        OPT_MOVE(cdr, args[1]);
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
    MinimObject *res;

    if (!minim_consp(args[0]))
        return minim_argument_error("pair", "car", 0, args[0]);
    
    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "car", 0, args[0]);

    OPT_MOVE_REF2(res, MINIM_CAR(args[0]), args[0]);
    return res;
}

MinimObject *minim_builtin_cdr(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    
    if (!minim_consp(args[0]))
        return minim_argument_error("pair", "cdr", 0, args[0]);
    
    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "cdr", 0, args[0]);

    if (MINIM_CDR(args[0]))
    {
        OPT_MOVE_REF2(res, MINIM_CDR(args[0]), args[0]);
    }
    else
    {
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    }

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
    MinimObject *res;

    res = minim_list(args, argc);
    RELEASE_OWNED_ARGS(args, argc);

    return res;
}

MinimObject *minim_builtin_head(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "head", 0, args[0]);

    OPT_MOVE_REF2(res, MINIM_CAR(args[0]), args[0]);
    return res;
}

MinimObject *minim_builtin_tail(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it;

    if (!MINIM_CAR(args[0]))
        return minim_argument_error("non-empty list", "tail", 0, args[0]);

    MINIM_TAIL(it, args[0]);
    OPT_MOVE_REF2(res, MINIM_CAR(it), args[0]);
    return res;
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

    res = fresh_minim_object(args[0]);
    MINIM_TAIL(it, res);
    for (size_t i = 1; i < argc; ++i)
    {
        MINIM_CDR(it) = fresh_minim_object(args[i]);
        MINIM_TAIL(it, it);
    }

    RELEASE_OWNED_ARGS(args, argc);
    return res;
}

MinimObject *minim_builtin_reverse(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *li;
    size_t len;

    if (!minim_listp(args[0]))
        return minim_argument_error("list", "reverse", 0, args[0]);

    li = fresh_minim_object(args[0]);
    len = minim_list_length(li);
    res = reverse_lists(li, NULL, len);
    RELEASE_OWNED_ARGS(args, argc);

    return res;
}

MinimObject *minim_builtin_remove(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *tmp;

    if (!minim_listp(args[1]))
        return minim_argument_error("list", "remove", 1, args[1]);

    if (minim_nullp(args[1]))
    {
        OPT_MOVE_REF(res, args[1]);
        return res;
    }

    OPT_MOVE(res, args[1]);
    if (minim_equalp(MINIM_CAR(res), args[0]))
    {
        tmp = res;
        res = MINIM_CDR(tmp);
        MINIM_CDR(tmp) = NULL;
        free_minim_object(tmp);
        return res;
    }

    tmp = res;
    for (MinimObject *it = MINIM_CDR(res); MINIM_CDR(it); it = MINIM_CDR(it))
    {
        if (minim_equalp(MINIM_CAR(it), args[0]))
        {
            MINIM_CDR(tmp) = MINIM_CDR(it);
            MINIM_CDR(it) = NULL;
            free_minim_object(it);
            it = tmp;
        }
        else
        {
            tmp = it;
        }
    }

    return res;
}

MinimObject *minim_builtin_list_ref(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it;
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

    OPT_MOVE_REF2(res, MINIM_CAR(it), args[0]);
    return res;
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
    size_t len;

    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "apply", 0, args[0]);
    
    if (!minim_listp(args[1]))
        return minim_argument_error("list", "apply", 1, args[1]);

    len = minim_list_length(args[1]);
    vals = malloc(len * sizeof(MinimObject*));

    it = args[1];
    for (size_t i = 0; i < len; ++i, it = MINIM_CDR(it))
        vals[i] = copy2_minim_object(MINIM_CAR(it));

    if (args[0]->type == MINIM_OBJ_FUNC)
    {
        MinimBuiltin func = args[0]->u.ptrs.p1;
        res = func(env, vals, len);
    }
    else // MINIM_OBJ_CLOSURE
    {
        MinimLambda *lam = args[0]->u.ptrs.p1;
        res = eval_lambda(lam, env, vals, len);
    }

    free_minim_objects(vals, len);
    return res;
}

MinimObject *minim_builtin_filter(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    bool owns;

    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "filter", 0, args[0]);
    
    if (!minim_listp(args[1]))
        return minim_argument_error("list", "filter", 1, args[1]);

    owns = MINIM_OBJ_OWNERP(args[1]);
    res = filter_list(args[1], args[0], env, false);
    if (owns) args[1] = NULL;

    return res;
}

MinimObject *minim_builtin_filtern(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    bool owns;

    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "filtern", 0, args[0]);
    
    if (!minim_listp(args[1]))
        return minim_argument_error("list", "filtern", 1, args[1]);

    owns = MINIM_OBJ_OWNERP(args[1]);
    res = filter_list(args[1], args[0], env, true);
    if (owns) args[1] = NULL;

    return res;
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
        OPT_MOVE_REF(res, args[1]);
    }
    else
    {
        MinimObject **vals = malloc(2 * sizeof(MinimObject*));
        MinimObject *tmp;

        OPT_MOVE(vals[1], args[1]);
        if (args[0]->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = args[0]->u.ptrs.p1;
            
            for (MinimObject *it = args[2]; it; it = MINIM_CDR(it))
            {
                OPT_MOVE(vals[0], MINIM_CAR(it))
                tmp = func(env, vals, 2);
                
                if (vals[0])    free_minim_object(vals[0]);
                if (vals[1])    free_minim_object(vals[1]);

                vals[1] = tmp;
                if (tmp->type == MINIM_OBJ_ERR) break;
            }
        }
        else
        {
            MinimLambda *lam = args[0]->u.ptrs.p1;

            for (MinimObject *it = args[2]; it; it = MINIM_CDR(it))
            {
                OPT_MOVE(vals[0], MINIM_CAR(it));
                tmp = eval_lambda(lam, env, vals, 2);

                if (tmp->type == MINIM_OBJ_ERR) break;
                if (vals[0])    free_minim_object(vals[0]);
                if (vals[1])    free_minim_object(vals[1]);

                vals[1] = tmp;
            }
        }

        res = vals[1];
        free(vals);
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

        vals = malloc(2 * sizeof(MinimObject*));
        OPT_MOVE(vals[0], MINIM_CAR(li));
        vals[1] = tmp;

        if (proc->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = proc->u.ptrs.p1;
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
        vals = malloc(2 * sizeof(MinimObject*));
        OPT_MOVE(vals[0], MINIM_CAR(li));
        vals[1] = copy2_minim_object(init);

        if (proc->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = proc->u.ptrs.p1;
            res = func(env, vals, 2);
        }
        else
        {
            MinimLambda *lam = proc->u.ptrs.p1;
            res = eval_lambda(lam, env, vals, 2);
        }
    }

    free_minim_objects(vals, 2);
    return res;
}

MinimObject *minim_builtin_foldr(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (!MINIM_OBJ_FUNCP(args[0]))
        return minim_argument_error("function", "foldr", 0, args[0]);
    
    if (!minim_listp(args[2]))
        return minim_argument_error("list", "foldr", 2, args[2]);

    if (minim_nullp(args[2]))
    {
        OPT_MOVE_REF(res, args[1]);
    }
    else
    {
        res = minim_foldr_h(env, args[0], args[2], args[1]);
    }

    return res;
}