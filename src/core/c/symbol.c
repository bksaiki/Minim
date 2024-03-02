/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.

    Symbol/string interner
*/

#include "../minim.h"

// copied from ChezScheme
size_t bucket_sizes[] = {
    13,
    29,
    59,
    113,
    257,
    509,
    1031,
    2053,
    4099,
    8209,           // default intern table size
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
#define start_size_ptr                     (&bucket_sizes[9])
#define load_factor(s, a)                  ((double)(s) / (double)(a))

#define resize_if_needed(i) {                                                   \
    if (load_factor((i)->size, (i)->alloc) > MINIM_INTERN_TABLE_LOAD_FACTOR)    \
        intern_table_resize(i);                                                 \
}

#define new_bucket(b, s, n) {                       \
    (b) = GC_alloc(sizeof(intern_table_bucket));    \
    (b)->sym = (s);                                 \
    (b)->next = (n);                                \
    (n) = (b);                                      \
}

static uint32_t hash_bytes(const void* data, size_t len) {
    uint32_t hash = 5381;
    const char *str = (const char*) data;

    for (size_t i = 0; i < len; ++i)
        hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */

    return hash;
}

static void intern_table_resize(intern_table *itab) {
    intern_table_bucket **new_buckets;
    size_t *new_alloc_ptr = ++itab->alloc_ptr;
    size_t new_alloc = *new_alloc_ptr;

    // setup new buckets
    new_buckets = GC_alloc(new_alloc * sizeof(intern_table_bucket*));
    for (size_t i = 0; i < new_alloc; ++i)
        new_buckets[i] = NULL;

    // move entries to new buckets
    for (size_t i = 0; i < itab->alloc; ++i) {
        for (intern_table_bucket *b = itab->buckets[i]; b; b = b->next) {
            intern_table_bucket *nb;
            size_t n, h, idx;

            n = strlen(minim_symbol(b->sym));
            h = hash_bytes(minim_symbol(b->sym), n);
            idx = h % new_alloc;
            new_bucket(nb, b->sym, new_buckets[idx]);
        }
    }

    // replace
    itab->buckets = new_buckets;
    itab->alloc = new_alloc;
}

static void intern_table_insert(intern_table *itab, size_t idx, mobj sym) {
    intern_table_bucket *b;
    new_bucket(b, sym, itab->buckets[idx]);
    ++itab->size;
}

intern_table *make_intern_table() {
    intern_table *itab = GC_alloc(sizeof(intern_table));
    itab->alloc_ptr = start_size_ptr;
    itab->alloc = *itab->alloc_ptr;
    itab->buckets = GC_calloc(itab->alloc, sizeof(intern_table_bucket*));
    itab->size = 0;
    return itab;
}

mobj intern_symbol(intern_table *itab, const char *sym) {
    intern_table_bucket *b;
    mobj obj;
    char *isym;
    size_t n, h, i, idx;

    n = strlen(sym);
    h = hash_bytes(sym, n);
    idx = h % itab->alloc;
    b = itab->buckets[idx];
    while (b != NULL) {
        isym = minim_symbol(b->sym);
        if (isym == sym)
            return b->sym;

        if (strlen(isym) == n) {
            for (i = 0; i < n; ++i) {
                if (isym[i] != sym[i])
                    break;
            }

            if (i == n)
                return b->sym;
        }

        b = b->next;
    }

    // intern symbol
    obj = Msymbol(sym);

    // update table
    resize_if_needed(itab);
    intern_table_insert(itab, idx, obj);
    return obj;
}

mobj Msymbol(const char *s) {
    mobj o;
    int len;

    o = GC_alloc(minim_symbol_size);
    len = strlen(s);
    minim_type(o) = MINIM_OBJ_SYMBOL;
    minim_symbol(o) =  GC_alloc_atomic((len + 1) * sizeof(char));
    strncpy(minim_symbol(o), s, len + 1);
    return o;
}

//
//  Primitives
//

mobj is_symbol_proc(int argc, mobj *args) {
    // (-> any boolean)
    return minim_symbolp(args[0]) ? minim_true : minim_false;
}
