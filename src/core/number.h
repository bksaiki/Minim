#ifndef _MINIM_NUMBER_H_
#define _MINIM_NUMBER_H_

#include <gmp.h>
#include "env.h"

#define MINIM_NUMBER_TO_UINT(obj)      mpz_get_ui(mpq_numref(MINIM_EXACT(obj)))

// Internals

bool minim_zerop(MinimObject *num);
bool minim_positivep(MinimObject *num);
bool minim_negativep(MinimObject *num);
bool minim_evenp(MinimObject *num);
bool minim_oddp(MinimObject *num);
bool minim_integerp(MinimObject *thing);
bool minim_exact_integerp(MinimObject *thing);
bool minim_exact_nonneg_intp(MinimObject *thing);

MinimObject *int_to_minim_number(long int x);
MinimObject *uint_to_minim_number(size_t x);
MinimObject *float_to_minim_number(double x);

int minim_number_cmp(MinimObject *a, MinimObject *b);

bool assert_numerical_args(MinimObject **args, size_t argc, MinimObject **res, const char *name);

#endif