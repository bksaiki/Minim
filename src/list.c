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
#include "util.h"

static void append_lists(MinimObject *head, int count, MinimObject **rest)
{
    MinimObject *it;
    int desc;

    if (count != 0)
    {
        desc = minim_list_length(head) - 1;
        it = head;
        for (int i = 0; i < desc; ++i, it = MINIM_CDR(it));

        MINIM_CDR(it) = rest[0];
        append_lists(rest[0], count - 1, &rest[1]);
    }
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
        MinimObject **args = malloc(sizeof(MinimObject*));
        MinimBuiltin func = map->data;

        copy_minim_object(&args[0], MINIM_CAR(list));
        val = func(env, 1, args);
    }
    else // map->type == MINIM_OBJ_CLOSURE
    {
        MinimObject **args = malloc(sizeof(MinimObject*));
        MinimLambda *lam = map->data;
        
        copy_minim_object(&args[0], MINIM_CAR(list));
        val = eval_lambda(lam, env, 1, args);
        free_minim_objects(1, args);
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
    MinimObject **args, *val, *next; 

    if (!list)
        return NULL;
    
    args = malloc(sizeof(MinimObject*));
    copy_minim_object(&args[0], MINIM_CAR(list));
    if (filter->type == MINIM_OBJ_FUNC)
    {
        MinimBuiltin func = filter->data;
        val = func(env, 1, &args[0]);
    }
    else
    {
        MinimLambda *lam = filter->data;
        val = eval_lambda(lam, env, 1, &args[0]);
    }

    if (val->type == MINIM_OBJ_ERR)
        return val;

    next = filter_list(MINIM_CDR(list), filter, env, negate);
    if (next && next->type == MINIM_OBJ_ERR)
    {
        free_minim_object(val);
        return next;
    }

    if (negate != coerce_into_bool(val))
    {
        MINIM_CDR(list) = next;
        free_minim_object(val);
        return list;
    }
    else
    {
        MINIM_CDR(list) = NULL;
        free_minim_object(list);
        free_minim_object(val);
        return next;
    }
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

bool assert_listof(MinimObject *arg, MinimObject **ret, MinimObjectPred pred, const char *msg)
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

bool minim_listof(MinimObject* list, MinimObjectPred pred)
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

MinimObject *minim_construct_list(int argc, MinimObject** args)
{
    MinimObject *obj;
    
    if (argc == 0)
        return NULL;

    init_minim_object(&obj, MINIM_OBJ_PAIR, args[0], minim_construct_list(argc - 1, args + 1));
    return obj;   
}

int minim_list_length(MinimObject *list)
{
    int len = 0;

    for (MinimObject *it = list; it != NULL; it = MINIM_CDR(it))
        if (MINIM_CAR(it))  ++len;
    
    return len;
}

void env_load_module_list(MinimEnv *env)
{
    env_load_builtin(env, "cons", MINIM_OBJ_FUNC, minim_builtin_cons);
    env_load_builtin(env, "pair?", MINIM_OBJ_FUNC, minim_builtin_consp);
    env_load_builtin(env, "car", MINIM_OBJ_FUNC, minim_builtin_car);
    env_load_builtin(env, "cdr", MINIM_OBJ_FUNC, minim_builtin_cdr);

    env_load_builtin(env, "list", MINIM_OBJ_FUNC, minim_builtin_list);
    env_load_builtin(env, "list?", MINIM_OBJ_FUNC, minim_builtin_listp);
    env_load_builtin(env, "null?", MINIM_OBJ_FUNC, minim_builtin_nullp);
    env_load_builtin(env, "head", MINIM_OBJ_FUNC, minim_builtin_head);
    env_load_builtin(env, "tail", MINIM_OBJ_FUNC, minim_builtin_tail);
    env_load_builtin(env, "length", MINIM_OBJ_FUNC, minim_builtin_length);

    env_load_builtin(env, "append", MINIM_OBJ_FUNC, minim_builtin_append);
    env_load_builtin(env, "reverse", MINIM_OBJ_FUNC, minim_builtin_reverse);
    env_load_builtin(env, "list-ref", MINIM_OBJ_FUNC, minim_builtin_list_ref);

    env_load_builtin(env, "map", MINIM_OBJ_FUNC, minim_builtin_map);
    env_load_builtin(env, "apply", MINIM_OBJ_FUNC, minim_builtin_apply);
    env_load_builtin(env, "filter", MINIM_OBJ_FUNC, minim_builtin_filter);
    env_load_builtin(env, "filtern", MINIM_OBJ_FUNC, minim_builtin_filtern);
}

MinimObject *minim_builtin_cons(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "cons", 2))
    {
        if (minim_nullp(args[1]))
        {
            init_minim_object(&res, MINIM_OBJ_PAIR, args[0], NULL);
            args[0] = NULL;
        }
        else
        {
            init_minim_object(&res, MINIM_OBJ_PAIR, args[0], args[1]);   
            args[0] = NULL;
            args[1] = NULL;
        }
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_consp(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "pair?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_consp(args[0]));
    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_car(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "car", 1) &&
        assert_pair(args[0], &res, "Expected a pair for 'car'") &&
        assert_generic(&res, "Expected a non-null list for 'car'", MINIM_CAR(args[0])))
    {
        res = MINIM_CAR(args[0]);
        MINIM_CAR(args[0]) = NULL;
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_cdr(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "cdr", 1) &&
        assert_pair(args[0], &res, "Expected a pair for 'cdr'") &&
        assert_generic(&res, "Expected a non-null list for 'cdr'", MINIM_CAR(args[0])))
    {
        res = MINIM_CDR(args[0]);
        MINIM_CDR(args[0]) = NULL;
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_listp(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "list?", 1))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_listp(args[0]));
    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_nullp(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "null?", 1))
    {
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_nullp(args[0]));
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_list(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (argc == 0)
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    else
        res = minim_construct_list(argc, args);

    free(args);
    return res;
}

MinimObject *minim_builtin_head(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, **pair;

    if (!assert_exact_argc(argc, args, &res, "head", 1))
    {
        free_minim_objects(argc, args);
    }
    else
    {
        if (MINIM_CAR(args[0]))
        {
            pair = ((MinimObject**) args[0]->data);
            res = pair[0];
            free_minim_object(pair[1]);

            pair[0] = NULL;
            pair[1] = NULL;
            free_minim_object(args[0]);
            free(args);
        }
        else
        {
            free_minim_objects(argc, args);
            minim_error(&res, "Expected a non-empty list");
        }
    }

    return res;
}

MinimObject *minim_builtin_tail(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, *it, **pair;

    if (assert_exact_argc(argc, args, &res, "tail", 1))
    {
        if (MINIM_CAR(args[0]))
        {
            it = args[0];
            while (MINIM_CDR(it))   it = MINIM_CDR(it);

            pair = ((MinimObject**) it->data);
            res = pair[0];
            free_minim_object(pair[1]);

            pair[0] = NULL;
            pair[1] = NULL;
        }
        else
        {
            minim_error(&res, "Expected a non-empty list");
        }
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_length(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    MinimNumber *num;
    int len;

    if (assert_exact_argc(argc, args, &res, "length", 1))
    {
        len = minim_list_length(args[0]);
        init_minim_number(&num, MINIM_NUMBER_EXACT);
        mpq_set_si(num->rat, len, 1);
        init_minim_object(&res, MINIM_OBJ_NUM, num);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_append(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (!assert_min_argc(argc, args, &res, "append", 1))
    {
        free_minim_objects(argc, args);
        return res;
    }

    for (int i = 0; i < argc; ++i)
    {
        if (!assert_list(args[i], &res, "Arguments must be lists for 'append'"))
        {
            free_minim_objects(argc, args);
            return res;
        }
    }

    append_lists(args[0], argc - 1, &args[1]);
    res = args[0];

    free(args);
    return res;
}

MinimObject *minim_builtin_reverse(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    int len;

    if (!assert_exact_argc(argc, args, &res, "reverse", 1) &&
        !assert_list(args[0], &res, "Expected a list for 'reverse'"))
    {
        free_minim_objects(argc, args);
        return res;
    }

    len = minim_list_length(args[0]);
    res = reverse_lists(args[0], NULL, len);

    free(args);
    return res;
}

MinimObject *minim_builtin_list_ref(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "list-ref", 2) &&
        assert_list(args[0], &res, "Expected a list for the 1st argument of 'list-ref") &&
        assert_exact_nonneg_int(args[1], &res, "Expected an exact non-negative integer for the 2nd argument of 'list-ref'"))
    {
        MinimObject *it = args[0];
        MinimNumber* num = args[1]->data;
        int idx = mpz_get_si(mpq_numref(num->rat));

        for (int i = 0; i < idx; ++i, it = MINIM_CDR(it))
            if (!it) break;

        if (!it)
        {
            minim_error(&res, "list-ref out of bounds: length %d, index %d", minim_list_length(args[0]), idx);
            free_minim_objects(argc, args);
            return res;
        }

        res = MINIM_CAR(it);
        MINIM_CAR(it) = NULL;
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_map(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res;
    int len;

    if (assert_exact_argc(argc, args, &res, "map", 2) &&
        assert_func(args[0], &res, "Expected a function in the 1st argument of 'map'") &&
        assert_list(args[1], &res, "Expected a list in the 2nd argument of 'map'"))
    {
        len = minim_list_length(args[1]);
        if (len == 0)   init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
        else            res = construct_list_map(args[1], args[0], env, len);
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_apply(MinimEnv *env, int argc, MinimObject** args)
{
    MinimObject *res, *it, **vals;
    int len;

    if (assert_exact_argc(argc, args, &res, "apply", 2) &&
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
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_filter(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "filter", 2) &&
        assert_func(args[0], &res, "Expected a function in the 1st argument of 'filter'") &&
        assert_list(args[1], &res, "Expected a list in the 2nd argument of 'filter'"))
    {
        res = filter_list(args[1], args[0], env, false);
        if (res->type != MINIM_OBJ_ERR)
            args[1] = NULL;
    }

    free_minim_objects(argc, args);
    return res;
}

MinimObject *minim_builtin_filtern(MinimEnv *env, int argc, MinimObject **args)
{
    MinimObject *res;

    if (assert_exact_argc(argc, args, &res, "filtern", 2) &&
        assert_func(args[0], &res, "Expected a function in the 1st argument of 'filtern'") &&
        assert_list(args[1], &res, "Expected a list in the 2nd argument of 'filtern'"))
    {
        res = filter_list(args[1], args[0], env, true);
        if (res->type != MINIM_OBJ_ERR)
            args[1] = NULL;
    }

    free_minim_objects(argc, args);
    return res;
}