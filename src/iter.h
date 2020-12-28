#ifndef _MINIM_ITER_H_
#define _MINIM_ITER_H_

#include "env.h"

MinimObject *minim_iter_next(MinimObject *obj);
MinimObject *minim_iter_get(MinimObject *obj);
bool minim_iter_endp(MinimObject *obj);
bool minim_iterablep(MinimObject *obj);

#endif