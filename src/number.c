#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "number.h"

// Visible functions

void init_exact_number(MinimNumber **pnum)
{
    MinimNumber *num = malloc(sizeof(MinimNumber));
    mpq_init(num->rat);
    num->type = MINIM_NUMBER_EXACT;
    *pnum = num;
}

void set_exact_number(MinimNumber* num, const char *str)
{
    if (num->type == MINIM_NUMBER_EXACT)     mpq_set_str(num->rat, str, 0);
    else                                     num->fl = strtod(str, NULL);
}

void copy_minim_number(MinimNumber **pnum, MinimNumber *src)
{
    MinimNumber *num = malloc(sizeof(MinimNumber));

    num->type = src->type;
    if (num->type == MINIM_NUMBER_EXACT)
    {
        mpq_init(num->rat);
        mpq_set(num->rat, src->rat);
    }
    else
    {
        num->fl = src->fl;
    }

    *pnum = num;
}

void free_minim_number(MinimNumber *num)
{
    if (num->type == MINIM_NUMBER_EXACT)    mpq_clear(num->rat);
    free(num);
}

char *minim_number_to_str(MinimNumber *num)
{
    char* str;

    if (num->type == MINIM_NUMBER_EXACT)
    {
        gmp_asprintf(&str, "%Qd", num->rat);
    }
    else
    {
        char *tmp = "Unimplemented: number->string";
        str = malloc((strlen(tmp) + 1) * sizeof(char));
        strcpy(str, tmp);
    }

    return str;
}

void minim_number_neg(MinimNumber *res, MinimNumber *a)
{
    if (a->type == MINIM_NUMBER_EXACT)
    {
        mpq_neg(res->rat, a->rat);
    }
    else
    {
        printf("Unimplemented");
    }
}

void minim_number_add(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_add(res->rat, a->rat, b->rat);
    }
    else
    {
        printf("Unimplemented");
    }
}

void minim_number_sub(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_sub(res->rat, a->rat, b->rat);
    }
    else
    {
        printf("Unimplemented");
    }
}

void minim_number_mul(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_mul(res->rat, a->rat, b->rat);
    }
    else
    {
        printf("Unimplemented");
    }
}

void minim_number_div(MinimNumber *res, MinimNumber *a, MinimNumber *b)
{
    if (a->type == MINIM_NUMBER_EXACT && b->type == MINIM_NUMBER_EXACT)
    {
        mpq_div(res->rat, a->rat, b->rat);
    }
    else
    {
        printf("Unimplemented");
    }
}

bool minim_number_zerop(MinimNumber *num)
{
    return ((num->type == MINIM_NUMBER_EXACT) ? mpq_cmp_si(num->rat, 0, 1) == 0 : num->fl == 0.0);
}