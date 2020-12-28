#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "bool.h"
#include "lambda.h"
#include "list.h"
#include "number.h"
#include "variable.h"

#define OPT_MOVE(dest, src)             \
{                                       \
    if (MINIM_OBJ_OWNERP(src))          \
    { dest = src; src = NULL; }         \
    else                                \
    { dest = fresh_minim_object(src); } \
}

#define OPT_MOVE_REF(dest, src)         \
{                                       \
    if (MINIM_OBJ_OWNERP(src))          \
    { dest = src; src = NULL; }         \
    else                                \
    { ref_minim_object(&dest, src); }   \
}

#define OPT_MOVE_REF2(dest, src, obj)   \
{                                       \
    if (MINIM_OBJ_OWNERP(obj))          \
    { dest = src; src = NULL; }         \
    else                                \
    { ref_minim_object(&dest, src); }   \
}

#define RELEASE_OWNED_ARGS(args, argc)      \
{                                           \
    for (int i = 0; i < argc; ++i)          \
    {                                       \
        if (MINIM_OBJ_OWNERP((args)[i]))      \
            (args)[i] = NULL;                 \
    }                                       \
}

static MinimObject *reverse_lists(MinimObject* head, MinimObject* tail, int count)
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

static MinimObject *construct_list_map(MinimObject* list, MinimObject* map, MinimEnv *env, int len)
{
    MinimObject *node, *val, *rest;

    if (len == 0)
        return NULL;

    if (map->type == MINIM_OBJ_FUNC)
    {
        MinimObject *arg = MINIM_CAR(list);
        MinimBuiltin func = map->data;

        val = func(env, 1, &arg);
    }
    else // map->type == MINIM_OBJ_CLOSURE
    {
        MinimObject *arg = MINIM_CAR(list);
        MinimLambda *lam = map->data;
        
        val = eval_lambda(lam, env, 1, &arg);
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
            MinimBuiltin func = filter->data;
            val = func(env, 1, &MINIM_CAR(it));
        }
        else
        {
            MinimLambda *lam = filter->data;
            val = eval_lambda(lam, env, 1, &MINIM_CAR(it));
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

//
//  Visible functions
//

bool assert_pair(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!minim_consp(arg))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool assert_list(MinimObject *arg, MinimObject **ret, const char *msg)
{
    if (!minim_listp(arg))
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool assert_list_len(MinimObject *arg, MinimObject **ret, int len, const char *msg)
{
    if (!assert_list(arg, ret, msg))
        return false;

    if (minim_list_length(arg) != len)
    {
        minim_error(ret, "%s", msg);
        return false;
    }

    return true;
}

bool assert_listof(MinimObject *arg, MinimObject **ret, MinimPred pred, const char *msg)
{
    MinimObject *it;
    int len;

    if (!assert_list(arg, ret, msg))
        return false;

    len = minim_list_length(arg);
    it = arg;
    for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
    {
        if (!pred(MINIM_CAR(it)))
        {
            minim_error(ret, "%s", msg);
            return false;
        }
    }

    return true;
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

MinimObject *minim_list(MinimObject **args, int len)
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
        for (int i = 1; i < len; ++i)
        {
            init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, fresh_minim_object(args[i]), NULL);
            it = MINIM_CDR(it);
        }
    }

    return head;
}

int minim_list_length(MinimObject *list)
{
    int len = 0;

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

    if (assert_exact_argc(&res, "cons", 2, argc))
    {
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
    }

    return res;
}

MinimObject *minim_builtin_consp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "pair?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_consp(args[0]));

    return res;
}

MinimObject *minim_builtin_car(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "car", 1, argc) &&
        assert_pair(args[0], &res, "Expected a pair for 'car'") &&
        assert_generic(&res, "Expected a non-null list for 'car'", MINIM_CAR(args[0])))
    {
        OPT_MOVE_REF2(res, MINIM_CAR(args[0]), args[0]);
    }

    return res;
}

MinimObject *minim_builtin_cdr(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "cdr", 1, argc) &&
        assert_pair(args[0], &res, "Expected a pair for 'cdr'") &&
        assert_generic(&res, "Expected a non-null list for 'cdr'", MINIM_CAR(args[0])))
    {
        OPT_MOVE_REF2(res, MINIM_CDR(args[0]), args[0]);
    }

    return res;
}

MinimObject *minim_builtin_listp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "list?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_listp(args[0]));
    
    return res;
}

MinimObject *minim_builtin_nullp(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "null?", 1, argc))
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

    if (assert_exact_argc(&res, "head", 1, argc))
    {
        if (MINIM_CAR(args[0]))
        {
            OPT_MOVE_REF2(res, MINIM_CAR(args[0]), args[0]);
        }
        else
        {
            minim_error(&res, "Expected a non-empty list");
        }
    }

    return res;
}

MinimObject *minim_builtin_tail(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it;

    if (assert_exact_argc(&res, "tail", 1, argc))
    {
        if (MINIM_CAR(args[0]))
        {
            it = args[0];
            while (MINIM_CDR(it))   it = MINIM_CDR(it);
            OPT_MOVE_REF2(res, MINIM_CAR(it), args[0]);
        }
        else
        {
            minim_error(&res, "Expected a non-empty list");
        }
    }

    return res;
}

MinimObject *minim_builtin_length(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *num;
    int len;

    if (assert_exact_argc(&res, "length", 1, argc))
    {
        len = minim_list_length(args[0]);
        init_minim_number(&num, MINIM_NUMBER_EXACT);
        mpq_set_si(num->rat, len, 1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    return res;
}

MinimObject *minim_builtin_append(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it;

    if (assert_min_argc(&res, "append", 1, argc))
    {
        for (int i = 0; i < argc; ++i)
        {
            if (!assert_list(args[i], &res, "Arguments must be lists for 'append'"))
                return res;
        }

        res = fresh_minim_object(args[0]);
        MINIM_TAIL(it, res);
        for (int i = 1; i < argc; ++i)
        {
            MINIM_CDR(it) = fresh_minim_object(args[i]);
            MINIM_TAIL(it, it);
        }

        RELEASE_OWNED_ARGS(args, argc);
    }

    return res;
}

MinimObject *minim_builtin_reverse(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *li;
    int len;

    if (assert_exact_argc(&res, "reverse", 1, argc) &&
        assert_list(args[0], &res, "Expected a list for 'reverse'"))
    {
        li = fresh_minim_object(args[0]);
        len = minim_list_length(li);
        res = reverse_lists(li, NULL, len);
        RELEASE_OWNED_ARGS(args, argc);
    }

    return res;
}

MinimObject *minim_builtin_list_ref(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "list-ref", 2, argc) &&
        assert_list(args[0], &res, "Expected a list for the 1st argument of 'list-ref") &&
        assert_exact_nonneg_int(args[1], &res, "Expected an exact non-negative integer for the 2nd argument of 'list-ref'"))
    {
        MinimObject *it = args[0];
        MinimNumber* num = args[1]->data;
        int idx = mpz_get_si(mpq_numref(num->rat));

        for (int i = 0; it && i < idx; ++i)
            it = MINIM_CDR(it);

        if (!it)
        {
            minim_error(&res, "list-ref out of bounds: length %d, index %d", minim_list_length(args[0]), idx);
            return res;
        }

        OPT_MOVE_REF2(res, MINIM_CAR(it), args[0]);
    }

    return res;
}

MinimObject *minim_builtin_map(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    int len;

    if (assert_exact_argc(&res, "map", 2, argc) &&
        assert_func(args[0], &res, "Expected a function in the 1st argument of 'map'") &&
        assert_list(args[1], &res, "Expected a list in the 2nd argument of 'map'"))
    {
        len = minim_list_length(args[1]);
        if (len == 0)   init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
        else            res = construct_list_map(args[1], args[0], env, len);
    }

    return res;
}

MinimObject *minim_builtin_apply(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *it, **vals;
    int len;

    if (assert_exact_argc(&res, "apply", 2, argc) &&
        assert_func(args[0], &res, "Expected a function in the 1st argument of 'apply'") &&
        assert_list(args[1], &res, "Expected a list in the 2nd argument of 'apply'"))
    {
        len = minim_list_length(args[1]);
        vals = malloc(len * sizeof(MinimObject*));

        it = args[1];
        for (int i = 0; i < len; ++i, it = MINIM_CDR(it))
        {
            vals[i] = MINIM_CAR(it);
            MINIM_CAR(it) = NULL;
        }

        if (args[0]->type == MINIM_OBJ_FUNC)
        {
            MinimBuiltin func = args[0]->data;
            res = func(env, len, vals);
        }
        else // MINIM_OBJ_CLOSURE
        {
            MinimLambda *lam = args[0]->data;
            res = eval_lambda(lam, env, len, vals);
        }

        free_minim_objects(vals, len);
    }

    return res;
}

MinimObject *minim_builtin_filter(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;
    bool owns;

    if (assert_exact_argc(&res, "filter", 2, argc) &&
        assert_func(args[0], &res, "Expected a function in the 1st argument of 'filter'") &&
        assert_list(args[1], &res, "Expected a list in the 2nd argument of 'filter'"))
    {
        owns = MINIM_OBJ_OWNERP(args[1]);
        res = filter_list(args[1], args[0], env, false);
        if (owns) args[1] = NULL;
    }

    return res;
}

MinimObject *minim_builtin_filtern(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;
    bool owns;

    if (assert_exact_argc(&res, "filtern", 2, argc) &&
        assert_func(args[0], &res, "Expected a function in the 1st argument of 'filtern'") &&
        assert_list(args[1], &res, "Expected a list in the 2nd argument of 'filtern'"))
    {
        owns = MINIM_OBJ_OWNERP(args[1]);
        res = filter_list(args[1], args[0], env, true);
        if (owns) args[1] = NULL;
    }

    return res;
}