#ifndef _MINIM_PRINT_H_
#define _MINIM_PRINT_H_

#include "env.h"

#define MINIM_DEFAULT_ERR_LOC_LEN       30

typedef struct PrintParams
{
    size_t maxlen;
    bool quote;
    bool display;
    bool syntax;
} PrintParams;

// Stores custom print methods
extern MinimObject **custom_print_methods;

// Sets default params: quote off, display off, syntax offf
void set_default_print_params(PrintParams *pp);

// For debugging
void debug_print_minim_object(MinimObject *obj, MinimEnv *env);

// Writes a string representation of the object to stdout
int print_minim_object(MinimObject *obj, MinimEnv *env, PrintParams *pp);

// Writes a string to the buffer
int print_to_buffer(Buffer *bf, MinimObject* obj, MinimEnv *env, PrintParams *pp);

// Writes a string representation of the object to stream.
int print_to_port(MinimObject *obj, MinimEnv *env, PrintParams *pp, FILE *stream);

// Returns a string representation of the object.
char *print_to_string(MinimObject *obj, MinimEnv *env, PrintParams *pp);

#endif
