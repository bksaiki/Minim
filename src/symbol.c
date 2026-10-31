// symbol.c: interned strings

#include "minim.h"

static uptr gensym_counter;

size_t bucket_sizes[] = {
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

#define MINIM_INTERN_TABLE_LOAD_FACTOR     0.75
#define start_size_ptr                     (&bucket_sizes[0])
#define load_factor(s, a)                  ((double)(s) / (double)(a))


static void table_resize(intern_table *tab) {
    obj *new_buckets, b;
    size_t *new_alloc_ptr;
    size_t new_alloc, i, n, h, j;

    // setup new buckets
    new_alloc_ptr = ++tab->alloc_ptr;
    new_alloc = *new_alloc_ptr;
    new_buckets = GC_malloc(tab->alloc * sizeof(obj));
    for (i = 0; i < new_alloc; i++)
        new_buckets[i] = Mnull;

    // move entries to new buckets
    for (i = 0; i < tab->alloc; ++i) {
        for (b = tab->buckets[i]; !Mnullp(b); b = Mcdr(b)) {
            // compute new index for symbol
            n = strlen(Msymbol_value(Mcar(b)));
            h = hash_bytes(Msymbol_value(Mcar(b)), n);
            j = h % new_alloc;
            new_buckets[j] = Mcons(Mcar(b), new_buckets[j]);
        }
    }

    // update table
    tab->buckets = new_buckets;
    tab->alloc = new_alloc;
}

static inline void resize_if_needed(intern_table *tab) {
    if (load_factor(tab->size, tab->alloc) > MINIM_INTERN_TABLE_LOAD_FACTOR)
        table_resize(tab);    
}

//
//  Public API
//

intern_table *make_intern_table(void) {
    intern_table *tab;
    size_t i;
    
    tab = GC_malloc(sizeof(intern_table));
    tab->alloc_ptr = start_size_ptr;
    tab->alloc = *tab->alloc_ptr;
    tab->buckets = GC_malloc(tab->alloc * sizeof(obj));
    tab->size = 0;

    for (i = 0; i < tab->alloc; i++)
        tab->buckets[i] = Mnull;
    
    return tab;
}

obj intern(intern_table *tab, const char *s) {
    obj b, x;
    char *is;
    size_t i, n, h;

    // compute index for string
    n = strlen(s);
    h = hash_bytes(s, n);
    i = h % tab->alloc;

    for (b = tab->buckets[i]; !Mnullp(b); b = Mcdr(b)) {
        // check if string is interned or equivalent to an interned string
        is = Msymbol_value(Mcar(b));
        if (is == s || strcmp(s, is) == 0)
            return Mcar(b);
    }

    // intern string
    x = Msymbol(s);
    
    // update table
    resize_if_needed(tab);
    tab->buckets[i] = Mcons(x, tab->buckets[i]);
    tab->size += 1;

    return x;
}

obj Mgensym(const char *s) {
    obj o;
    uptr n;
    
    n = snprintf(NULL, 0, "%s%ld\n", s, gensym_counter);
    o = GC_malloc(Msymbol_size);
    obj_type(o) = SYMBOL_OBJ_TYPE;
    Msymbol_value(o) = GC_malloc_atomic((n + 1) * sizeof(char));
    snprintf(Msymbol_value(o), n + 1, "%s%ld", s, gensym_counter);
    gensym_counter++;

    return o;
}
