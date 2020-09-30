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

void copy_minim_number(MinimNumber **pnum, MinimNumber *src);
void free_minim_number(MinimNumber *num);

char *minim_number_to_str(MinimNumber *num);

// Arithmetic functions

void minim_number_neg(MinimNumber *res, MinimNumber *a);

void minim_number_add(MinimNumber *res, MinimNumber *a, MinimNumber *b);
void minim_number_sub(MinimNumber *res, MinimNumber *a, MinimNumber *b);
void minim_number_mul(MinimNumber *res, MinimNumber *a, MinimNumber *b);
void minim_number_div(MinimNumber *res, MinimNumber *a, MinimNumber *b);

// Predicates

bool minim_number_zerop(MinimNumber *num);

#endif