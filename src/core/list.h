#ifndef _MINIM_LIST_H_
#define _MINIM_LIST_H_

#include "assert.h"
#include "env.h"

#define MINIM_TAIL(dest, x)                     \
{                                               \
    dest = x;                                   \
    while (!minim_nullp(MINIM_CDR(dest)))       \
        dest = MINIM_CDR(dest);                 \
}

// Internals

bool minim_consp(MinimObject* thing);
bool minim_listp(MinimObject* thing);
bool minim_listof(MinimObject* list, MinimPred pred);
bool minim_cons_eqp(MinimObject *a, MinimObject *b);
void minim_cons_to_bytes(MinimObject *obj, Buffer *bf);

MinimObject *minim_list(MinimObject **args, size_t len);
MinimObject *minim_list_ref(MinimObject *lst, size_t n);
size_t minim_list_length(MinimObject *list);

#endif
