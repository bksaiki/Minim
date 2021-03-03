#ifndef _MINIM_NUMBER_H_
#define _MINIM_NUMBER_H_

#include <gmp.h>
#include "env.h"

typedef enum MinimNumberType
{
    MINIM_NUMBER_EXACT,
    MINIM_NUMBER_INEXACT
} MinimNumberType;

typedef struct MinimNumber
{
    union
    {
        mpq_t rat;
        double fl;
    };
    MinimNumberType type;
} MinimNumber;

// Initialization & setters

void init_minim_number(MinimNumber **pnum, MinimNumberType type);
void str_to_minim_number(MinimNumber* num, const char *str);

void reinterpret_minim_number(MinimNumber *num, MinimNumberType type);
void copy_minim_number(MinimNumber **pnum, MinimNumber *src);
void free_minim_number(MinimNumber *num);

char *minim_number_to_str(MinimNumber *num);
int minim_number_cmp(MinimNumber *a, MinimNumber *b);

// Predicates

bool minim_number_zerop(MinimNumber *num);
bool minim_number_negativep(MinimNumber *num);
bool minim_number_positivep(MinimNumber *num);
bool minim_number_exactp(MinimNumber *num);
bool minim_number_inexactp(MinimNumber *num);
bool minim_number_exactintp(MinimNumber *num);
bool minim_number_exactnonnegintp(MinimNumber *num);

// Assertions

bool assert_number(MinimObject *arg, MinimObject **ret, const char *msg);
bool assert_exact_number(MinimObject *arg, MinimObject **ret, const char *msg);
bool assert_inexact_number(MinimObject *arg, MinimObject **ret, const char *msg);
bool assert_exact_int(MinimObject *arg, MinimObject **ret, const char *msg);
bool assert_exact_nonneg_int(MinimObject *arg, MinimObject **ret, const char *msg);

// Internals

bool minim_numberp(MinimObject *thing);
bool minim_exactp(MinimObject *thing);
bool minim_inexactp(MinimObject *thing);
void minim_number_to_bytes(MinimObject *obj, Buffer *bf);

size_t minim_number_to_uint(MinimObject *obj);

#endif