#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "ast.h"
#include "object.h"

#define READ_FLAG_WAIT          0x1     // behavior upon encountering end of inpt

SyntaxNode *minim_parse_port(MinimObject *port, SyntaxNode **perr, uint8_t flags);

#endif
