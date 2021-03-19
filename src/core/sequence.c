#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "error.h"
#include "number.h"
#include "list.h"
#include "sequence.h"

void init_minim_seq(MinimSeq **pseq, MinimSeqType type, ...)
{
    MinimSeq* seq;
    va_list v;

    va_start(v, type);
    if (type == MINIM_SEQ_NUM_RANGE)
    {
        seq = malloc(sizeof(MinimSeq));
        seq->state = va_arg(v, MinimNumber*);
        seq->end = va_arg(v, MinimNumber*);
        seq->type = type;
        seq->done = (minim_number_cmp(seq->state, seq->end) == 0);
        *pseq = seq;
    }
    else
    {
        *pseq = NULL;
        printf("Unknown sequence type\n...");
    }

    va_end(v);
}

void copy_minim_seq(MinimSeq **pseq, MinimSeq *src)
{
    MinimSeq* seq = malloc(sizeof(MinimSeq));

    if (src->type == MINIM_SEQ_NUM_RANGE)
    {
        MinimNumber *begin, *end;

        copy_minim_number(&begin, src->state);
        copy_minim_number(&end, src->end);
        seq->state = begin;
        seq->end = end;
        seq->type = src->type;
        seq->done = src->done;   
        *pseq = seq;
    }
    else
    {
        *pseq = NULL;
        printf("Unknown sequence type\n...");
    }

}

void free_minim_seq(MinimSeq *seq)
{
    switch (seq->type)
    {
    case MINIM_SEQ_NUM_RANGE:
        free_minim_number(seq->state);
        free_minim_number(seq->end);
        break;
    
    default:
        printf("Unknown sequence type\n...");
        break;
    }

    free(seq);
}

MinimObject *minim_seq_get(MinimSeq *seq)
{
    if (seq->type == MINIM_SEQ_NUM_RANGE)
    {
        MinimObject *obj;
        MinimNumber *num;

        copy_minim_number(&num, seq->state);
        init_minim_object(&obj, MINIM_OBJ_NUM, num);
        return obj;
    }
    else
    {
        printf("Unknown sequence type\n...");
        return NULL;
    }
}

void minim_seq_next(MinimSeq *seq)
{
    if (seq->type == MINIM_SEQ_NUM_RANGE)
    {
        mpq_t one;

        mpq_init(one);
        mpq_set_ui(one, 1, 1);
        mpq_add(seq->state, seq->state, one);
        mpq_clear(one);
    
        seq->done = (minim_number_cmp(seq->state, seq->end) == 0);     
    }
    else
    {
        printf("Unknown sequence type\n...");
    }
}

bool minim_seq_donep(MinimSeq *seq)
{
    return seq->done;
}

/* Builtins */

MinimObject *minim_builtin_sequencep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "sequence?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_SEQP(args[0]));
    
    return res;
}

MinimObject *minim_builtin_in_range(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *begin, *end;
    MinimSeq *seq;

    if (!assert_range_argc(&res, "in-range", 1, 2, argc))
        return res;

    if (!minim_exact_nonneg_intp(args[0]))
        return minim_argument_error("non-negative exact integer", "in-range", 0, args[0]);

    if (argc == 2 && !minim_exact_nonneg_intp((args[1])))
        return minim_argument_error("non-negative exact integer", "in-range", 1, args[1]);

    if (argc == 2)
    {
        copy_minim_number(&begin, args[0]->u.ptrs.p1);
        copy_minim_number(&end, args[1]->u.ptrs.p1);
        if (minim_number_cmp(begin, end) > 0)
        {
            minim_error(&res, "Expected a valid range [begin, end) in 'in-range'");
            return res;
        }
    }
    else
    {
        init_minim_number(&begin, MINIM_NUMBER_EXACT);
        mpq_set_ui(begin->rat, 0, 1);
        copy_minim_number(&end, args[0]->u.ptrs.p1);
    }

    init_minim_seq(&seq, MINIM_SEQ_NUM_RANGE, begin, end);
    init_minim_object(&res, MINIM_OBJ_SEQ, seq);

    return res;
}

MinimObject *minim_builtin_in_naturals(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;
    MinimNumber *begin, *end;
    MinimSeq *seq;

    if (!assert_range_argc(&res, "in-naturals", 0, 1, argc))
        return res;

    if (argc == 1 && !minim_exact_nonneg_intp((args[0])))
        return minim_argument_error("non-negative exact integer", "in-naturals", 1, args[0]);

    if (argc == 1)
    {
        copy_minim_number(&begin, args[0]->u.ptrs.p1);
    }
    else
    {
        init_minim_number(&begin, MINIM_NUMBER_EXACT);
        mpq_set_ui(begin->rat, 0, 1);
    }

    init_minim_number(&end, MINIM_NUMBER_EXACT);
    mpq_set_si(end->rat, -1, 1);

    init_minim_seq(&seq, MINIM_SEQ_NUM_RANGE, begin, end);
    init_minim_object(&res, MINIM_OBJ_SEQ, seq);

    return res;
}

MinimObject *minim_builtin_sequence_to_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *seq, *val, *it;
    bool first;

    if (!assert_exact_argc(&res, "sequence->list", 1, argc))
        return res;

    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence->list", 0, args[0]);

    seq = fresh_minim_object(args[0]);
    first = true;

    while (!minim_seq_donep(seq->u.ptrs.p1))
    {
        val = minim_seq_get(seq->u.ptrs.p1);
        if (first)
        {
            init_minim_object(&res, MINIM_OBJ_PAIR, val, NULL);
            it = res;
            first = false;
        }
        else
        {   
            init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, val, NULL);
            it = MINIM_CDR(it);
        }

        minim_seq_next(seq->u.ptrs.p1);
    }
    
    if (first)
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);

    if (!MINIM_OBJ_OWNERP(args[0]))
        free_minim_object(seq);

    return res;
}