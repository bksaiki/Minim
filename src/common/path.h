#ifndef _MINIM_COMMON_PATH_H_
#define _MINIM_COMMON_PATH_H_

#include "buffer.h"

// *** Reading *** //

void valid_path(Buffer *valid, const char *maybe);

Buffer *get_directory(const char *file);

#endif
