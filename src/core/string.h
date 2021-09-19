#ifndef _MINIM_STRING_H_
#define _MINIM_STRING_H_

#include "object.h"

// String predicates

bool is_rational(char *str);
bool is_float(const char *str);
bool is_char(char *str);
bool is_str(char *str);

// String to number

MinimObject *str_to_number(const char *str, MinimObjectType type);

#endif
