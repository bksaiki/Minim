#ifndef _MINIM_PARSER_H_
#define _MINIM_PARSER_H_

#include "object.h"

#define READ_FLAG_WAIT          0x1     // behavior upon encountering end of inpt

MinimObject *minim_parse_port(MinimObject *port, MinimObject **perr, uint8_t flags);

#endif
