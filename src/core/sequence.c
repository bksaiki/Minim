#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "../gc/gc.h"
#include "assert.h"
#include "error.h"
#include "number.h"
#include "list.h"
#include "sequence.h"

static void gc_minim_seq_mrk(void (*mrk)(void*, void*), void *gc, void *ptr)
{
    MinimSeq *seq = (MinimSeq*) ptr;
    mrk(gc, seq->state);
    mrk(gc, seq->end);
}

void init_minim_seq(MinimSeq **pseq, MinimSeqType type, ...)
{
    MinimSeq* seq;
    va_list v;

    va_start(v, type);
    if (type == MINIM_SEQ_NUM_RANGE)
    {
        seq = GC_alloc_opt(sizeof(MinimSeq), NULL, gc_minim_seq_mrk);
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
    MinimSeq* seq = GC_alloc_opt(sizeof(MinimSeq), NULL, gc_minim_seq_mrk);

    if (src->type == MINIM_SEQ_NUM_RANGE)
    {
        MinimObject *begin, *end;

        begin = src->state;
        end = src->state;
        seq->state = begin;
        seq->end = end;
        seq->type = src->type;
        seq->done = src->done;   
        *pseq = seq;
    }
}

MinimObject *minim_seq_get(MinimSeq *seq)
{
    MinimObject *obj;

    if (seq->type == MINIM_SEQ_NUM_RANGE)
        obj = seq->state;
    else
        obj = NULL;

    return obj;
}

void minim_seq_next(MinimSeq *seq)
{
    if (seq->type == MINIM_SEQ_NUM_RANGE)
    {
        MinimObject *state, *one, *next;

        state = seq->state;
        one = int_to_minim_number(1);
        init_minim_object(&next, MINIM_OBJ_EXACT, gc_alloc_mpq_ptr());
        mpq_add(MINIM_EXACT(next), MINIM_EXACT(state), MINIM_EXACT(one));

        seq->done = (minim_number_cmp(next, seq->end) == 0);
        seq->state = next;
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
        begin = args[0];
        end = args[1];
        if (minim_number_cmp(begin, end) > 0)
            return minim_error("expected [begin, end)", "in-range");
    }
    else
    {
        begin = int_to_minim_number(0);
        end = args[0];
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

    begin = (argc == 1) ? args[0] : int_to_minim_number(0);
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

    seq = args[0];
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
    
    if (!it)    init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
    return res;
}
