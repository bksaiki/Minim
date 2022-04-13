#ifndef _MINIM_TEST_COMMON_H_
#define _MINIM_TEST_COMMON_H_

#include "../core/minim.h"

void setup_test_env();
bool compile_and_run_test(char *cexpr, char *iexpr, char *expected);
bool run_test(char *input, char *expected);
bool evaluate(char *input);

#endif
