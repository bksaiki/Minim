#ifndef _MINIM_ASSERT_H_
#define _MINIM_ASSERT_H_

#include "object.h"

typedef bool (*MinimObjectPred)(MinimObject*); // TODO: remove
typedef bool (*MinimPred)(MinimObject*);

// *** Argument types *** //

bool assert_pair_arg(MinimObject *arg, MinimObject **ret, const char *op);
bool assert_sym_arg(MinimObject *arg, MinimObject **ret, const char *op);

bool assert_numerical_args(int argc, MinimObject **args, MinimObject **ret, const char *op);

// *** Arity *** //

bool assert_min_argc(int argc, MinimObject **args, MinimObject** ret, const char *op, int min);
bool assert_exact_argc(int argc, MinimObject **args, MinimObject** ret, const char *op, int exact);
bool assert_range_argc(int argc, MinimObject **args, MinimObject** ret, const char *op, int min, int max);

// *** Pred *** //
bool assert_for_all(int argc, MinimObject **args, MinimObject **ret, const char *msg, MinimPred pred);

#endif