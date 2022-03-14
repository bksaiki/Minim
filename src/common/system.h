#ifndef _MINIM_COMMON_SYSTEM_H_
#define _MINIM_COMMON_SYSTEM_H_

// need pid
#include "types.h"

// Limits

#ifndef PATH_MAX
#define PATH_MAX    4096
#endif

// System queries

pid_t get_current_pid();
char* get_current_dir();
bool environment_variable_existsp(const char *name);

// Memory

void* alloc_page(size_t size);
void free_page(void *page, size_t size);
int make_page_executable(void* page, size_t size);

#endif
