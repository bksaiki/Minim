#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "assert.h"
#include "number.h"
#include "sequence.h"

void init_minim_seq(MinimSeq **pseq, MinimSeqType type, ...)
{
    MinimSeq* seq;
    va_list v;

    va_start(v, type);
    if (type == MINIM_SEQ_NUM_RANGE)
    {
        seq = malloc(sizeof(MinimSeq));
        seq->state = malloc(sizeof(size_t));
        seq->end = malloc(sizeof(size_t));

        *((size_t*) seq->state) = va_arg(v, size_t);
        *((size_t*) seq->end) = va_arg(v, size_t);
        seq->type = type;
        seq->done = (*((size_t*) seq->state) == *((size_t*) seq->end));

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
        seq->state = malloc(sizeof(size_t));
        seq->end = malloc(sizeof(size_t));

        memcpy(seq->state, src->state, sizeof(size_t));
        memcpy(seq->state, src->state, sizeof(size_t));
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
    if (seq->state)     free(seq->state);
    if (seq->end)       free(seq->end);
    free(seq);
}

MinimObject *minim_seq_get(MinimSeq *seq)
{
    if (seq->type == MINIM_SEQ_NUM_RANGE)
    {
        MinimNumber *num;
        MinimObject *obj;
        size_t *p = seq->state;

        init_minim_number(&num, MINIM_NUMBER_EXACT);
        mpq_set_ui(num->rat, *p, 1);
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
        size_t *pnum = seq->state;
        size_t *pend = seq->end;

        ++(*pnum);
        if (*pnum == *pend)
            seq->done = true;
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

/* Internals */

bool minim_sequencep(MinimObject *thing)
{
    return thing->type == MINIM_OBJ_SEQ;
}

/* Builtins */

MinimObject *minim_builtin_in_range(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_range_argc(&res, "in-range", 1, 2, argc) &&
        assert_exact_nonneg_int(args[0], &res, "Expected a non-negative exact integer \
                                in the 1st argument of 'in-range'") &&
        (argc == 1 || assert_exact_nonneg_int(args[1], &res, "Expected a non-negative \
                                              exact integer in the 1st argument of 'in-range'")))
    {
        MinimNumber *val;
        MinimSeq *seq;
        size_t begin, end;

        if (argc == 2)
        {
            val = args[0]->data;
            begin = mpz_get_ui(mpq_numref(val->rat));
            val = args[1]->data;
            end = mpz_get_ui(mpq_numref(val->rat));

            if (end < begin)
            {
                minim_error(&res, "Expected a valid range [begin, end) in 'in-range'");
                return res;
            }
        }
        else
        {
            begin = 0;
            val = args[0]->data;
            end = mpz_get_ui(mpq_numref(val->rat));
        }

        init_minim_seq(&seq, MINIM_SEQ_NUM_RANGE, begin, end);
        init_minim_object(&res, MINIM_OBJ_SEQ, seq);
    }

    return res;
}

MinimObject *minim_builtin_sequencep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "sequence?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_sequencep(args[0]));
    
    return res;
}