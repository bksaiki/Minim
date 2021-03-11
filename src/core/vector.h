#ifndef _MINIM_VECTOR_H_
#define _MINIM_VECTOR_H_

#include "../common/buffer.h"
#include "env.h"

bool assert_vector(MinimObject *obj, MinimObject **err, const char *msg);
bool minim_vectorp(MinimObject *obj);

bool minim_vector_equalp(MinimObject *a, MinimObject *b);
void minim_vector_bytes(MinimObject *vec, Buffer *bf);

#endif