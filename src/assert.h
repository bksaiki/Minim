#ifndef _MINIM_ASSERT_H_
#define _MINIM_ASSERT_H_

#include "object.h"

typedef bool (*MinimPred)(MinimObject*); // TODO: remove
typedef bool (*MinimPred)(MinimObject*);

// *** Arity *** //

bool assert_min_argc(int argc, MinimObject **args, MinimObject** ret, const char *op, int min);
bool assert_exact_argc(int argc, MinimObject **args, MinimObject** ret, const char *op, int exact);
bool assert_range_argc(int argc, MinimObject **args, MinimObject** ret, const char *op, int min, int max);

// *** Pred *** //
bool assert_for_all(int argc, MinimObject **args, MinimObject **ret, const char *msg, MinimPred pred);

// *** Generic *** //
bool assert_generic(MinimObject **ret, const char *msg, bool pred);

#endif