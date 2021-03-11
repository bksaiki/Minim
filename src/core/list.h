#ifndef _MINIM_LIST_H_
#define _MINIM_LIST_H_

#include "assert.h"
#include "env.h"

#define MINIM_CAAR(x)   MINIM_CAR(MINIM_CAR(x))
#define MINIM_CDAR(x)   MINIM_CDR(MINIM_CAR(x))
#define MINIM_CADR(x)   MINIM_CAR(MINIM_CDR(x))
#define MINIM_CDDR(x)   MINIM_CDR(MINIM_CDR(x))

#define MINIM_TAIL(dest, x)         \
{                                   \
    dest = x;                       \
    while (MINIM_CDR(dest))         \
        dest = MINIM_CDR(dest);     \
}

// Assertions
bool assert_pair(MinimObject *arg, MinimObject **ret, const char *msg);
bool assert_list(MinimObject *arg, MinimObject **ret, const char *msg);
bool assert_list_len(MinimObject *arg, MinimObject **ret, size_t len, const char *msg);
bool assert_listof(MinimObject *arg, MinimObject **ret, MinimPred pred, const char *msg);

// Internals

bool minim_consp(MinimObject* thing);
bool minim_listp(MinimObject* thing);
bool minim_nullp(MinimObject* thing);
bool minim_listof(MinimObject* list, MinimPred pred);
bool minim_cons_eqp(MinimObject *a, MinimObject *b);
void minim_cons_to_bytes(MinimObject *obj, Buffer *bf);

MinimObject *minim_list(MinimObject **args, size_t len);
size_t minim_list_length(MinimObject *list);

#endif