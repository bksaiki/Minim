#ifndef _MINIM_VECTOR_H_
#define _MINIM_VECTOR_H_

#include "common/buffer.h"
#include "env.h"

struct MinimVector
{
    MinimObject **arr;
    size_t size;
} typedef MinimVector;

void init_minim_vector(MinimVector **pvec, size_t size);
void copy_minim_vector(MinimVector **pvec, MinimVector *src);
void free_minim_vector(MinimVector *vec);
bool minim_vector_equalp(MinimVector *a, MinimVector *b);
void minim_vector_bytes(MinimVector *vec, Buffer *bf);

bool assert_vector(MinimObject *obj, MinimObject **err, const char *msg);
bool minim_vectorp(MinimObject *obj);

#endif