#ifndef _MINIM_BUILTIN_H_
#define _MINIM_BUILTIN_H_

#include "env.h"

#define DEFINE_BUILTIN_FUN(name)  MinimObject *minim_builtin_ ## name(MinimEnv *env, MinimObject **args, size_t argc);

// Variable / Control
DEFINE_BUILTIN_FUN(def)
DEFINE_BUILTIN_FUN(setb)
DEFINE_BUILTIN_FUN(if)
DEFINE_BUILTIN_FUN(let)
DEFINE_BUILTIN_FUN(letstar)
DEFINE_BUILTIN_FUN(for)
DEFINE_BUILTIN_FUN(for_list)
DEFINE_BUILTIN_FUN(begin)
DEFINE_BUILTIN_FUN(quote)
DEFINE_BUILTIN_FUN(lambda)

// Miscellaneous
DEFINE_BUILTIN_FUN(equalp)
DEFINE_BUILTIN_FUN(symbolp)
DEFINE_BUILTIN_FUN(printf)

// Boolean
DEFINE_BUILTIN_FUN(boolp)
DEFINE_BUILTIN_FUN(not)
DEFINE_BUILTIN_FUN(or)
DEFINE_BUILTIN_FUN(and)

// Number
DEFINE_BUILTIN_FUN(numberp)
DEFINE_BUILTIN_FUN(zerop)
DEFINE_BUILTIN_FUN(positivep)
DEFINE_BUILTIN_FUN(negativep)
DEFINE_BUILTIN_FUN(exactp)
DEFINE_BUILTIN_FUN(inexactp)
DEFINE_BUILTIN_FUN(eq)
DEFINE_BUILTIN_FUN(gt)
DEFINE_BUILTIN_FUN(lt)
DEFINE_BUILTIN_FUN(gte)
DEFINE_BUILTIN_FUN(lte)

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

// List
DEFINE_BUILTIN_FUN(list)
DEFINE_BUILTIN_FUN(listp)
DEFINE_BUILTIN_FUN(nullp)
DEFINE_BUILTIN_FUN(head)
DEFINE_BUILTIN_FUN(tail)
DEFINE_BUILTIN_FUN(length)
DEFINE_BUILTIN_FUN(append)
DEFINE_BUILTIN_FUN(reverse)
DEFINE_BUILTIN_FUN(list_ref)
DEFINE_BUILTIN_FUN(map)
DEFINE_BUILTIN_FUN(apply)
DEFINE_BUILTIN_FUN(filter)
DEFINE_BUILTIN_FUN(filtern)

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
DEFINE_BUILTIN_FUN(vector_ref)

// Sequence
DEFINE_BUILTIN_FUN(in_range)
DEFINE_BUILTIN_FUN(sequencep)

// Math
DEFINE_BUILTIN_FUN(add)
DEFINE_BUILTIN_FUN(sub)
DEFINE_BUILTIN_FUN(mul)
DEFINE_BUILTIN_FUN(div)
DEFINE_BUILTIN_FUN(sqrt)
DEFINE_BUILTIN_FUN(exp)
DEFINE_BUILTIN_FUN(log)
DEFINE_BUILTIN_FUN(pow)

// Loads a single function into the environment
void env_load_builtin(MinimEnv *env, const char *name, MinimObjectType type, ...);

// Loads every builtin symbol in the base library.
void env_load_builtins(MinimEnv *env);

#endif