#include <string.h>

#include "hash.h"
#include "intern.h"

#define MINIM_INTERN_TABLE_LOAD_FACTOR     0.75

// copied from ChezScheme
static size_t bucket_sizes[54] = {
    1031,
    2053,
    4099,
    8209,
    16411,
    32771,
    65537,
    131101,
    262147,
    524309,
    1048583,
    2097169,
    4194319,
    8388617,
    16777259,
    33554467,
    67108879,
    134217757,
    268435459,
    536870923,
    1073741827,
    2147483659,
    4294967311,
    8589934609,
    17179869209,
    34359738421,
    68719476767,
    137438953481,
    274877906951,
    549755813911,
    1099511627791,
    2199023255579,
    4398046511119,
    8796093022237,
    17592186044423,
    35184372088891,
    70368744177679,
    140737488355333,
    281474976710677,
    562949953421381,
    1125899906842679,
    2251799813685269,
    4503599627370517,
    9007199254740997,
    18014398509482143,
    36028797018963971,
    72057594037928017,
    144115188075855881,
    288230376151711813,
    576460752303423619,
    1152921504606847009,
    2305843009213693967,
    4611686018427388039,
    0
};

#define load_factor(s, a)   ((double)(s) / (double)(a))

#define resize_if_needed(i)                                                     \
{                                                                               \
    if (load_factor((i)->size, (i)->alloc) > MINIM_INTERN_TABLE_LOAD_FACTOR)    \
        intern_table_resize(i);                                                 \
}


#define new_bucket(b, s, n)                         \
{                                                   \
    (b) = GC_alloc(sizeof(InternTableBucket));      \
    (b)->sym = (s);                                 \
    (b)->next = (n);                                \
    (n) = (b);                                      \
}

InternTable *init_intern_table()
{
    InternTable *itab = GC_alloc(sizeof(InternTable));
    itab->alloc_ptr = &bucket_sizes[3];
    itab->alloc = *itab->alloc_ptr;
    itab->buckets = GC_alloc(itab->alloc * sizeof(InternTableBucket*));
    itab->size = 0;

    for (size_t i = 0; i < itab->alloc; ++i)
        itab->buckets[i] = NULL;
    return itab;
}

static void intern_table_resize(InternTable *itab)
{
    InternTableBucket **new_buckets;
    size_t *new_alloc_ptr = ++itab->alloc_ptr;
    size_t new_alloc = *new_alloc_ptr;

    new_buckets = GC_alloc(new_alloc * sizeof(InternTableBucket*));
    for (size_t i = 0; i < itab->alloc; ++i)
    {
        for (InternTableBucket *b = itab->buckets[i]; b; b = b->next)
        {
            InternTableBucket *nb;
            size_t n, h, idx;

            n = strlen(MINIM_SYMBOL(b->sym));
            h = hash_bytes(MINIM_SYMBOL(b->sym), n);
            idx = h % new_alloc;
            new_bucket(nb, b->sym, new_buckets[idx]);
        }
    }

    itab->buckets = new_buckets;
    itab->alloc = new_alloc;
}

static void intern_table_insert(InternTable *itab, size_t idx, MinimObject *sym)
{
    InternTableBucket *b;
    
    new_bucket(b, sym, itab->buckets[idx]);
    ++itab->size;
}

MinimObject *intern_symbol(InternTable *itab, const char *sym)
{
    InternTableBucket *b;
    MinimObject *obj;
    size_t n = strlen(sym);
    size_t h = hash_bytes(sym, n);
    size_t idx = h % itab->alloc;
    char *isym, *str;

    b = itab->buckets[idx];
    while (b != NULL)
    {
        isym = MINIM_SYMBOL(b->sym);
        if (strlen(isym) == n)
        {
            size_t i;
            for (i = 0; i < n; ++i)
            {
                if (isym[i] == sym[i])
                    break;
            }

            if (i == n)
                return b->sym;
        }

        b = b->next;
    }

    str = GC_alloc_atomic((n + 1) * sizeof(char));
    for (size_t i = 0; i < n; ++i) str[i] = sym[i];
    str[n] = '\0';

    obj = minim_symbol(str);
    resize_if_needed(itab);
    intern_table_insert(itab, idx, obj);
    return obj;
}
