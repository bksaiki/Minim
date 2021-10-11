#ifndef _MINIM_STRING_H_
#define _MINIM_STRING_H_

#include "object.h"

// String predicates

bool is_rational(const char *str);
bool is_float(const char *str);
bool is_char(const char *str);
bool is_str(const char *str);

// String to number

MinimObject *str_to_number(const char *str, MinimObjectType type);

#endif
