#ifndef _MINIM_ASSERT_H_
#define _MINIM_ASSERT_H_

#include "object.h"

typedef bool (*MinimPred)(MinimObject*); // TODO: remove
typedef bool (*MinimPred)(MinimObject*);

// *** Arity *** //

bool assert_min_argc(MinimObject** ret, const char *op, size_t expected, size_t actual);
bool assert_exact_argc(MinimObject** ret, const char *op, size_t expected, size_t actual);
bool assert_range_argc(MinimObject** ret, const char *op, size_t min, size_t max, size_t actual);

// *** Pred *** //
bool assert_for_all(MinimObject **ret, MinimObject **args, size_t argc, const char *msg, MinimPred pred);

// *** Generic *** //
bool assert_generic(MinimObject **ret, const char *msg, bool pred);

#endif