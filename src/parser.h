#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "common/common.h"
#include "ast.h"

// Parses a single expression. Returns null on failure.
int parse_str(const char* str, MinimAst** syn);

#endif