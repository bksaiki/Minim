#ifndef _MINIM_BUILTIN_H_
#define _MINIM_BUILTIN_H_

#include "env.h"

typedef struct {
    size_t low, high;
} MinimArity;

#define DEFINE_BUILTIN_FUN(name)  MinimObject *minim_builtin_ ## name(MinimEnv *env, size_t argc, MinimObject **args);

// Variable / Control
DEFINE_BUILTIN_FUN(def)
DEFINE_BUILTIN_FUN(setb)
DEFINE_BUILTIN_FUN(if)
DEFINE_BUILTIN_FUN(let)
DEFINE_BUILTIN_FUN(letstar)
DEFINE_BUILTIN_FUN(for)
DEFINE_BUILTIN_FUN(for_list)
DEFINE_BUILTIN_FUN(begin)
DEFINE_BUILTIN_FUN(case)
DEFINE_BUILTIN_FUN(quote)
DEFINE_BUILTIN_FUN(quasiquote)
DEFINE_BUILTIN_FUN(unquote)
DEFINE_BUILTIN_FUN(lambda)
DEFINE_BUILTIN_FUN(exit)
DEFINE_BUILTIN_FUN(delay)
DEFINE_BUILTIN_FUN(force)

// Modules
DEFINE_BUILTIN_FUN(export)
DEFINE_BUILTIN_FUN(import)

// Transforms
DEFINE_BUILTIN_FUN(def_syntax)
DEFINE_BUILTIN_FUN(syntax)
DEFINE_BUILTIN_FUN(syntaxp)
DEFINE_BUILTIN_FUN(unwrap)
DEFINE_BUILTIN_FUN(syntax_error)

// Miscellaneous
DEFINE_BUILTIN_FUN(equalp)
DEFINE_BUILTIN_FUN(symbolp)
DEFINE_BUILTIN_FUN(printf)
DEFINE_BUILTIN_FUN(error)
DEFINE_BUILTIN_FUN(void)
DEFINE_BUILTIN_FUN(version);
DEFINE_BUILTIN_FUN(symbol_count);

// Procedure
DEFINE_BUILTIN_FUN(procedurep)
DEFINE_BUILTIN_FUN(procedure_arity)

// Promise
DEFINE_BUILTIN_FUN(promisep)

// Boolean
DEFINE_BUILTIN_FUN(boolp)
DEFINE_BUILTIN_FUN(not)

// Number
DEFINE_BUILTIN_FUN(numberp)
DEFINE_BUILTIN_FUN(zerop)
DEFINE_BUILTIN_FUN(positivep)
DEFINE_BUILTIN_FUN(negativep)
DEFINE_BUILTIN_FUN(evenp)
DEFINE_BUILTIN_FUN(oddp)
DEFINE_BUILTIN_FUN(exactp)
DEFINE_BUILTIN_FUN(inexactp)
DEFINE_BUILTIN_FUN(integerp)
DEFINE_BUILTIN_FUN(nanp)
DEFINE_BUILTIN_FUN(infinitep)
DEFINE_BUILTIN_FUN(eq)
DEFINE_BUILTIN_FUN(gt)
DEFINE_BUILTIN_FUN(lt)
DEFINE_BUILTIN_FUN(gte)
DEFINE_BUILTIN_FUN(lte)
DEFINE_BUILTIN_FUN(to_exact)
DEFINE_BUILTIN_FUN(to_inexact)

// String
DEFINE_BUILTIN_FUN(stringp)
DEFINE_BUILTIN_FUN(string_append)
DEFINE_BUILTIN_FUN(substring)
DEFINE_BUILTIN_FUN(string_to_symbol)
DEFINE_BUILTIN_FUN(symbol_to_string)
DEFINE_BUILTIN_FUN(format)

// Pair
DEFINE_BUILTIN_FUN(cons)
DEFINE_BUILTIN_FUN(consp)
DEFINE_BUILTIN_FUN(car)
DEFINE_BUILTIN_FUN(cdr)

DEFINE_BUILTIN_FUN(caar)
DEFINE_BUILTIN_FUN(cadr)
DEFINE_BUILTIN_FUN(cdar)
DEFINE_BUILTIN_FUN(cddr)

// List
DEFINE_BUILTIN_FUN(list)
DEFINE_BUILTIN_FUN(listp)
DEFINE_BUILTIN_FUN(nullp)
DEFINE_BUILTIN_FUN(head)
DEFINE_BUILTIN_FUN(tail)
DEFINE_BUILTIN_FUN(length)
DEFINE_BUILTIN_FUN(append)
DEFINE_BUILTIN_FUN(reverse)
DEFINE_BUILTIN_FUN(remove)
DEFINE_BUILTIN_FUN(member)
DEFINE_BUILTIN_FUN(list_ref)
DEFINE_BUILTIN_FUN(map)
DEFINE_BUILTIN_FUN(apply)
DEFINE_BUILTIN_FUN(filter)
DEFINE_BUILTIN_FUN(filtern)
DEFINE_BUILTIN_FUN(foldl)
DEFINE_BUILTIN_FUN(foldr)
DEFINE_BUILTIN_FUN(assoc)

// Hash table
DEFINE_BUILTIN_FUN(hash)
DEFINE_BUILTIN_FUN(hashp)
DEFINE_BUILTIN_FUN(hash_keyp)
DEFINE_BUILTIN_FUN(hash_ref)
DEFINE_BUILTIN_FUN(hash_remove)
DEFINE_BUILTIN_FUN(hash_set)
DEFINE_BUILTIN_FUN(hash_setb)
DEFINE_BUILTIN_FUN(hash_removeb)
DEFINE_BUILTIN_FUN(hash_to_list)

// Vector
DEFINE_BUILTIN_FUN(vector)
DEFINE_BUILTIN_FUN(make_vector)
DEFINE_BUILTIN_FUN(vectorp)
DEFINE_BUILTIN_FUN(vector_length)
DEFINE_BUILTIN_FUN(vector_ref)
DEFINE_BUILTIN_FUN(vector_setb)
DEFINE_BUILTIN_FUN(vector_to_list)
DEFINE_BUILTIN_FUN(list_to_vector)
DEFINE_BUILTIN_FUN(vector_fillb)

// Sequence
DEFINE_BUILTIN_FUN(sequencep)
DEFINE_BUILTIN_FUN(in_range)
DEFINE_BUILTIN_FUN(in_naturals)
DEFINE_BUILTIN_FUN(sequence_to_list)

// Math
DEFINE_BUILTIN_FUN(add)
DEFINE_BUILTIN_FUN(sub)
DEFINE_BUILTIN_FUN(mul)
DEFINE_BUILTIN_FUN(div)
DEFINE_BUILTIN_FUN(abs)
DEFINE_BUILTIN_FUN(max)
DEFINE_BUILTIN_FUN(min)
DEFINE_BUILTIN_FUN(modulo)
DEFINE_BUILTIN_FUN(remainder)
DEFINE_BUILTIN_FUN(quotient)
DEFINE_BUILTIN_FUN(numerator)
DEFINE_BUILTIN_FUN(denominator)
DEFINE_BUILTIN_FUN(gcd)
DEFINE_BUILTIN_FUN(lcm)

DEFINE_BUILTIN_FUN(floor)
DEFINE_BUILTIN_FUN(ceil)
DEFINE_BUILTIN_FUN(trunc)
DEFINE_BUILTIN_FUN(round)

DEFINE_BUILTIN_FUN(sqrt)
DEFINE_BUILTIN_FUN(exp)
DEFINE_BUILTIN_FUN(log)
DEFINE_BUILTIN_FUN(pow)
DEFINE_BUILTIN_FUN(sin)
DEFINE_BUILTIN_FUN(cos)
DEFINE_BUILTIN_FUN(tan)
DEFINE_BUILTIN_FUN(asin)
DEFINE_BUILTIN_FUN(acos)
DEFINE_BUILTIN_FUN(atan)

// Loads a single function into the environment
void minim_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...);

// Loads every builtin symbol in the base library.
void minim_load_builtins(MinimEnv *env);

#endif
