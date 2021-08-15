#include <stdarg.h>
#include <string.h>

#include "../gc/gc.h"
#include "arity.h"
#include "assert.h"
#include "error.h"
#include "jmp.h"
#include "number.h"
#include "lambda.h"
#include "list.h"
#include "sequence.h"

void init_minim_seq(MinimSeq **pseq, MinimObject *init, MinimObject *first, MinimObject *rest, MinimObject *donep)
{
    MinimSeq *seq;

    seq = GC_alloc(sizeof(MinimSeq));
    seq->val = init;
    seq->first = first;
    seq->rest = rest;
    seq->donep = donep;

    *pseq = seq;
}

static MinimObject *minim_seq_first(MinimEnv *env, MinimSeq *seq)
{
    if (MINIM_OBJ_JUMPP(seq->first))    // no return
        minim_long_jump(seq->first, env, 1, &seq->val);

    return MINIM_OBJ_CLOSUREP(seq->first) ?
           eval_lambda(MINIM_CLOSURE(seq->first), NULL, 1, &seq->val) :
           ((MinimBuiltin) MINIM_BUILTIN(seq->first))(env, 1, &seq->val);
}

static MinimSeq *minim_seq_rest(MinimEnv *env, MinimSeq *seq)
{
    MinimObject *next;
    MinimSeq *seq2;

    if (MINIM_OBJ_JUMPP(seq->rest))    // no return
        minim_long_jump(seq->rest, env, 1, &seq->val);

    next = MINIM_OBJ_CLOSUREP(seq->rest) ?
           eval_lambda(MINIM_CLOSURE(seq->rest), NULL, 1, &seq->val) :
           ((MinimBuiltin) MINIM_BUILTIN(seq->rest))(env, 1, &seq->val);
    init_minim_seq(&seq2, next, seq->first, seq->rest, seq->donep);
    return seq2;
}

static bool minim_seq_donep(MinimEnv *env, MinimSeq *seq)
{
    if (MINIM_OBJ_JUMPP(seq->donep))    // no return
        minim_long_jump(seq->donep, env, 1, &seq->val);

    return coerce_into_bool(MINIM_OBJ_CLOSUREP(seq->donep) ?
                            eval_lambda(MINIM_CLOSURE(seq->donep), NULL, 1, &seq->val) :
                            ((MinimBuiltin) MINIM_BUILTIN(seq->donep))(env, 1, &seq->val));
}

static bool func_1aryp(MinimObject *func)
{
    MinimArity arity;

    if (MINIM_OBJ_BUILTINP(func))
        minim_get_builtin_arity(MINIM_BUILTIN(func), &arity);
    else
        minim_get_lambda_arity(MINIM_CLOSURE(func), &arity);

    return (arity.low == 1 && arity.high == 1) ||
           (arity.low <= 1 && arity.high == SIZE_MAX);
}

// ================================ Builtins ================================

MinimObject *minim_builtin_sequence(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimSeq *seq;

    if (!MINIM_OBJ_FUNCP(args[1]) || !func_1aryp(args[1]))
        return minim_argument_error("procedure of one argument", "sequence", 1, args[1]);

    if (!MINIM_OBJ_FUNCP(args[2]) || !func_1aryp(args[2]))
        return minim_argument_error("procedure of one argument", "sequence", 2, args[2]);

    if (!MINIM_OBJ_FUNCP(args[3]) || !func_1aryp(args[3]))
        return minim_argument_error("procedure of one argument", "sequence", 3, args[3]);

    init_minim_seq(&seq, args[0], args[1], args[2], args[3]);
    return minim_sequence(seq);
}

MinimObject *minim_builtin_sequencep(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_SEQP(args[0]));
}

MinimObject *minim_builtin_sequence_first(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence-first", 0, args[0]);

    if (minim_seq_donep(env, MINIM_SEQUENCE(args[0])))
    {
        return minim_argument_error("empty sequence",
                                    "sequence-first",
                                    SIZE_MAX,
                                    MINIM_SEQUENCE(args[0])->val);
    }
        
    return minim_seq_first(env, MINIM_SEQUENCE(args[0]));
}

MinimObject *minim_builtin_sequence_rest(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence-rest", 0, args[0]);

    if (minim_seq_donep(env, MINIM_SEQUENCE(args[0])))
    {
        return minim_argument_error("empty sequence",
                                    "sequence-rest",
                                    SIZE_MAX,
                                    MINIM_SEQUENCE(args[0])->val);
    }
    

    return minim_sequence(minim_seq_rest(env, MINIM_SEQUENCE(args[0])));
}

MinimObject *minim_builtin_sequence_donep(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_SEQP(args[0]))
        return minim_argument_error("sequence", "sequence-first", 0, args[0]);

    return to_bool(minim_seq_donep(env, MINIM_SEQUENCE(args[0])));
}
