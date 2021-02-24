#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "../common/common.h"
#include "../common/read.h"
#include "ast.h"

// Parses a single expression
int parse_str(const char* str, MinimAst** psyntax);

// Parses a single expression with syntax location
int parse_expr_loc(const char* str, MinimAst** psyntax, SyntaxLoc *loc);

#endif