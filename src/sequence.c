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

/* Internals */

bool minim_sequencep(MinimObject *thing)
{
    return thing->type == MINIM_OBJ_SEQ;
}

/* Builtins */

MinimObject *minim_builtin_sequencep(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_exact_argc(&res, "sequence?", 1, argc))
        init_minim_object(&res, MINIM_OBJ_BOOL, minim_sequencep(args[0]));
    
    return res;
}

MinimObject *minim_builtin_in_range(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res;

    if (assert_range_argc(&res, "in-range", 1, 2, argc) &&
        assert_exact_nonneg_int(args[0], &res, "Expected a non-negative exact integer \
                                in the 1st argument of 'in-range'") &&
        (argc == 1 || assert_exact_nonneg_int(args[1], &res, "Expected a non-negative \
                                              exact integer in the 1st argument of 'in-range'")))
    {
        MinimNumber *begin, *end;
        MinimSeq *seq;

        if (argc == 2)
        {
            copy_minim_number(&begin, args[0]->data);
            copy_minim_number(&end, args[1]->data);
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
            copy_minim_number(&end, args[0]->data);
        }

        init_minim_seq(&seq, MINIM_SEQ_NUM_RANGE, begin, end);
        init_minim_object(&res, MINIM_OBJ_SEQ, seq);
    }

    return res;
}