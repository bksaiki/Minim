// intern.c: symbol interner
//
// String and symbols are Unicode strings.

#include "minim.h"

mobj define_values_sym;
mobj letrec_values_sym;
mobj let_values_sym;
mobj values_sym;
mobj lambda_sym;
mobj begin_sym;
mobj if_sym;
mobj quote_sym;
mobj setb_sym;
mobj void_sym;
mobj define_sym;
mobj let_sym;
mobj letrec_sym;
mobj cond_sym;
mobj and_sym;
mobj or_sym;
mobj foreign_proc_sym;

// from ChezScheme
size_t bucket_sizes[] = {
    13,
    29,
    59,
    113,
    257,
    509,
    1031, // default intern table size
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

// hashing from Chez Scheme

#define multiplier 3

// static iptr hash(const mbyte* s, size_t n) {
//   iptr h = n + 401887359;
//   while (n--) h = h * multiplier + *s++;
//   return h;
// }

static iptr hash_mstr(const mchar* s, size_t n) {
  iptr h = n + 401887359;
  while (n--) h = h * multiplier + *s++;
  return h;
}

#define LOAD_FACTOR         0.75
#define init_alloc_ptr      (&bucket_sizes[6]) 
#define load_factor(s, a)   ((float)(s) / (float)(a))

#define new_bucket(b, s, n) {               \
    (b) = GC_alloc(sizeof(ibucket));        \
    (b)->sym = (s);                         \
    (b)->next = (n);                        \
    (n) = (b);                              \
}

static void intern_table_resize() {
    // use the next rung (assuming table grows by 1 each time)
    size_t *alloc_ptr = M_glob.oblist_len_ptr + 1;
    size_t new_alloc = *alloc_ptr;
    ibucket **buckets = GC_calloc(new_alloc, sizeof(ibucket*));

    // copy entries from old list
    size_t old_alloc = *M_glob.oblist_len_ptr;
    for (size_t i = 0; i < old_alloc; ++i) {
        for (ibucket *b = M_glob.oblist[i]; b; b = b->next) {
            ibucket *nb;
            size_t n, h, idx;

            n = mstrlen(minim_symbol(b->sym));
            h = hash_mstr(minim_symbol(b->sym), n);
            idx = h % new_alloc;
            new_bucket(nb, b->sym, buckets[idx]);
        }
    }

    // update table
    M_glob.oblist = buckets;
    M_glob.oblist_len_ptr = alloc_ptr;
}

static void intern_table_insert(mobj sym, size_t idx) {
    ibucket *b;
    new_bucket(b, sym, M_glob.oblist[idx]);
    ++M_glob.oblist_count;
}

static mobj *intern_symbol(const mchar *str) {
    ibucket *b;
    mobj *obj;
    size_t n, h, idx;

    // possibly need to resize
    if (load_factor(M_glob.oblist_count, *M_glob.oblist_len_ptr) > LOAD_FACTOR) {
        intern_table_resize();
    }

    // lookup index
    n = mstrlen(str);
    h = hash_mstr(str, n);
    idx = h % *M_glob.oblist_len_ptr;

    // search intern table for existing entry
    for (b = M_glob.oblist[idx]; b; b = b->next) {
        // we might be interning with the key
        if (minim_symbol(b->sym) == str) {
            return b->sym;
        }

        // otherwise, check if we have an equivalent string
        if (mstrcmp(minim_symbol(b->sym), str) == 0) {
            return b->sym;
        }
    }

    // otherwise, this string is new so intern it
    obj = Msymbol(str);
    intern_table_insert(obj, idx);
    return obj;
}

void intern_table_init() {
    M_glob.oblist_len_ptr = init_alloc_ptr;
    M_glob.oblist = GC_calloc(*init_alloc_ptr, sizeof(ibucket*));
    M_glob.oblist_count = 0;
}

mobj intern(const char *s) {
    return intern_str(mstr(s));
}

mobj intern_str(const mchar *s) {
    return intern_symbol(s);
}
