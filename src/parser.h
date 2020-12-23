#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "ast.h"
#include "base.h"

// Parses a single expression. Returns null on failure.
int parse_str(char* str, MinimAst** syn);

#endif