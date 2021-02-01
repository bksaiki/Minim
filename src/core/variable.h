#ifndef _MINIM_VARIABLE_H_
#define _MINIM_VARIABLE_H_

#include "env.h"

// Internals

bool minim_symbolp(MinimObject *obj);
bool assert_symbol(MinimObject *obj, MinimObject **res, const char* msg);

#endif