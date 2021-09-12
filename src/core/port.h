#ifndef _MINIM_PORT_H_
#define _MINIM_PORT_H_

#include "object.h"

#define port_eof(p)         (MINIM_PORT_MODE(p) & MINIM_PORT_MODE_ALT_EOF ? '\n' : EOF)
#define next_ch(p)          (fgetc(MINIM_PORT_FILE(p)))
#define put_back(p, c)      (ungetc(c, MINIM_PORT_FILE(p)))

void update_port(MinimObject *port, char c);

#endif
