#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../gc/gc.h"
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
        seq = GC_alloc(sizeof(MinimSeq));
        seq->state = va_arg(v, MinimObject*);
        seq->end = va_arg(v, MinimObject*);
        seq->type = type;
        seq->done = (minim_number_cmp(seq->state, seq->end) == 0);
        *pseq = seq;
    }

    va_end(v);
}

void copy_minim_seq(MinimSeq **pseq, MinimSeq *src)
{
    MinimSeq* seq = GC_alloc(sizeof(MinimSeq));

    if (src->type == MINIM_SEQ_NUM_RANGE)
    {
        MinimObject *begin, *end;

        copy_minim_object(&begin, src->state);
        copy_minim_object(&end, src->end);
        seq->state = begin;
        seq->end = end;
        seq->type = src->type;
        seq->done = src->done;   
        *pseq = seq;
    }
}

void free_minim_seq(MinimSeq *seq)
{
    switch (seq->type)
    {
    case MINIM_SEQ_NUM_RANGE:
        free_minim_object(seq->state);
        free_minim_object(seq->end);
        break;
    }
}

MinimObject *minim_seq_get(MinimSeq *seq)
{
    MinimObject *obj;

    if (seq->type == MINIM_SEQ_NUM_RANGE)
        copy_minim_object(&obj, seq->state);
    else
        obj = NULL;

    return obj;
}

void minim_seq_next(MinimSeq *seq)
{
    if (seq->type == MINIM_SEQ_NUM_RANGE)
    {
        MinimObject *state, *one;

        state = seq->state;
        one = int_to_minim_number(1);
        mpq_add(MINIM_EXACT(state), MINIM_EXACT(state), MINIM_EXACT(one));
        free_minim_object(one);

        seq->done = (minim_number_cmp(state, seq->end) == 0);     
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

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_SEQP(args[0]));
    return res;
}

MinimObject *minim_builtin_in_range(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *begin, *end;
    MinimSeq *seq;

    if (!minim_exact_nonneg_intp(args[0]))
        return minim_argument_error("non-negative exact integer", "in-range", 0, args[0]);

    if (argc == 2 && !minim_exact_nonneg_intp((args[1])))
        return minim_argument_error("non-negative exact integer", "in-range", 1, args[1]);

    if (argc == 2)
    {
        begin = copy2_minim_object(args[0]);
        end = copy2_minim_object(args[1]);
        if (minim_number_cmp(begin, end) > 0)
            return minim_error("expected [begin, end)", "in-range");
    }
    else
    {
        begin = int_to_minim_number(0);
        end = copy2_minim_object(args[0]);
    }

    init_minim_seq(&seq, MINIM_SEQ_NUM_RANGE, begin, end);
    init_minim_object(&res, MINIM_OBJ_SEQ, seq);

    return res;
}

MinimObject *minim_builtin_in_naturals(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *begin, *end;
    MinimSeq *seq;

    if (argc == 1 && !minim_exact_nonneg_intp((args[0])))
        return minim_argument_error("non-negative exact integer", "in-naturals", 1, args[0]);

    if (argc == 1)
        begin = copy2_minim_object(args[0]);
    else
        begin = int_to_minim_number(0);

    end = int_to_minim_number(-1);
    init_minim_seq(&seq, MINIM_SEQ_NUM_RANGE, begin, end);
    init_minim_object(&res, MINIM_OBJ_SEQ, seq);

    return res;
}

MinimObject *minim_builtin_sequence_to_list(MinimEnv *env, MinimObject **args, size_t argc)
{
    MinimObject *res, *seq, *val, *it;

    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence->list", 0, args[0]);

    seq = fresh_minim_object(args[0]);
    it = NULL;
    while (!minim_seq_donep(seq->u.ptrs.p1))
    {
        val = minim_seq_get(seq->u.ptrs.p1);
        if (!it)
        {
            init_minim_object(&res, MINIM_OBJ_PAIR, val, NULL);
            it = res;
        }
        else
        {   
            init_minim_object(&MINIM_CDR(it), MINIM_OBJ_PAIR, val, NULL);
            it = MINIM_CDR(it);
        }

        minim_seq_next(seq->u.ptrs.p1);
    }
    
    if (!it)
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);

    if (!MINIM_OBJ_OWNERP(args[0]))
        free_minim_object(seq);

    return res;
}
