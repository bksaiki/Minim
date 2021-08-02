#include <stdarg.h>
#include <string.h>

#include "../gc/gc.h"
#include "assert.h"
#include "error.h"
#include "number.h"
#include "lambda.h"
#include "list.h"
#include "sequence.h"

void init_minim_seq(MinimSeq **pseq, MinimObject *init, MinimLambda *first, MinimLambda *rest, MinimLambda *donep)
{
    MinimSeq *seq;

    seq = GC_alloc(sizeof(MinimSeq));
    seq->val = init;
    seq->first = first;
    seq->rest = rest;
    seq->donep = donep;

    *pseq = seq;
}

void copy_minim_seq(MinimSeq **pseq, MinimSeq *src)
{
    MinimSeq *seq;

    seq = GC_alloc(sizeof(MinimSeq));
    copy_minim_object(&seq->val, src->val);
    copy_minim_lambda(&seq->first, src->first);
    copy_minim_lambda(&seq->rest, src->rest);
    copy_minim_lambda(&seq->donep, src->donep);

    *pseq = seq;
}

static MinimObject *minim_seq_first(MinimSeq *seq)
{
    return eval_lambda(seq->first, NULL, 1, &seq->val);
}

static MinimSeq *minim_seq_rest(MinimSeq *seq)
{
    MinimObject *next;
    MinimSeq *seq2;

    next = eval_lambda(seq->rest, NULL, 1, &seq->val);
    init_minim_seq(&seq2, next, seq->first, seq->rest, seq->donep);
    return seq2;
}

static bool minim_seq_donep(MinimSeq *seq)
{
    return coerce_into_bool(eval_lambda(seq->donep, NULL, 1, &seq->val));
}

// ================================ Builtins ================================

MinimObject *minim_builtin_sequence(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;
    MinimSeq *seq;

    if ((!MINIM_OBJ_CLOSUREP(args[1])) ||
        (!MINIM_CLOSURE(args[1])->rest && MINIM_CLOSURE(args[1])->argc != 1))
    {
        return minim_argument_error("procedure of one argument", "sequence", 1, args[1]);
    }

    if ((!MINIM_OBJ_CLOSUREP(args[2])) ||
        (!MINIM_CLOSURE(args[2])->rest && MINIM_CLOSURE(args[2])->argc != 1))
    {
        return minim_argument_error("procedure of one argument", "sequence", 2, args[2]);
    }

    if ((!MINIM_OBJ_CLOSUREP(args[3])) ||
        (!MINIM_CLOSURE(args[3])->rest && MINIM_CLOSURE(args[3])->argc != 1))
    {
        return minim_argument_error("procedure of one argument", "sequence", 3, args[3]);
    }

    init_minim_seq(&seq, args[0], MINIM_CLOSURE(args[1]), MINIM_CLOSURE(args[2]), MINIM_CLOSURE(args[3]));
    init_minim_object(&res, MINIM_OBJ_SEQ, seq);
    return res;
}

MinimObject *minim_builtin_sequencep(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    init_minim_object(&res, MINIM_OBJ_BOOL, MINIM_OBJ_SEQP(args[0]));
    return res;
}

MinimObject *minim_builtin_sequence_first(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence-first", 0, args[0]);

    if (minim_seq_donep(MINIM_SEQ(args[0])))
    {
        return minim_argument_error("empty sequence",
                                    "sequence-first",
                                    SIZE_MAX,
                                    MINIM_SEQ(args[0])->val);
    }
        
    return minim_seq_first(MINIM_SEQ(args[0]));
}

MinimObject *minim_builtin_sequence_rest(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence-rest", 0, args[0]);

    if (minim_seq_donep(MINIM_SEQ(args[0])))
    {
        return minim_argument_error("empty sequence",
                                    "sequence-rest",
                                    SIZE_MAX,
                                    MINIM_SEQ(args[0])->val);
    }
    
    init_minim_object(&res, MINIM_OBJ_SEQ, minim_seq_rest(MINIM_SEQ(args[0])));
    return res;
}

MinimObject *minim_builtin_sequence_donep(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res;

    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence-first", 0, args[0]);

    init_minim_object(&res, MINIM_OBJ_BOOL, minim_seq_donep(MINIM_SEQ(args[0])));
    return res;
}

MinimObject *minim_builtin_sequence_to_list(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *res, *head, *lst;
    MinimSeq *it;

    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence->list", 0, args[0]);

    if (minim_seq_donep(MINIM_SEQ(args[0])))
    {
        init_minim_object(&res, MINIM_OBJ_PAIR, NULL, NULL);
        return res;
    }

    it = MINIM_SEQ(args[0]);
    init_minim_object(&head, MINIM_OBJ_PAIR, minim_seq_first(it), NULL);
    lst = head;
    for (it = minim_seq_rest(it); minim_seq_donep(it); it = minim_seq_rest(it))
    {
        init_minim_object(&MINIM_CDR(lst), MINIM_OBJ_PAIR, minim_seq_first(it), NULL);
        lst = MINIM_CDR(lst);
    }

    return head;
}
