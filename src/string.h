#ifndef _MINIM_STRING_H_
#define _MINIM_STRING_H_

#include "env.h"

// Internals

bool minim_stringp(MinimObject *thing);
bool assert_string(MinimObject *thing, MinimObject **res, const char *msg);

#endif