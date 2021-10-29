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

#define PRINT_PARAMS_DEFAULT_MAXLEN     UINT_MAX
#define PRINT_PARAMS_DEFAULT_QUOTE      false
#define PRINT_PARAMS_DEFAULT_DISPLAY    false
#define PRINT_PARAMS_DEFAULT_SYNTAX     false

// Stores custom print methods
extern MinimObject **custom_print_methods;

// Sets the print param
void set_print_params(PrintParams *pp,
                      size_t maxlen,
                      bool quote,
                      bool display,
                      bool syntax);

// Sets print params to default
#define set_default_print_params(pp)                        \
    set_print_params(pp, PRINT_PARAMS_DEFAULT_MAXLEN,       \
                         PRINT_PARAMS_DEFAULT_QUOTE,        \
                         PRINT_PARAMS_DEFAULT_DISPLAY,      \
                         PRINT_PARAMS_DEFAULT_SYNTAX);       

// Sets print params for printing syntax
#define set_syntax_print_params(pp)                         \
    set_print_params(pp, PRINT_PARAMS_DEFAULT_MAXLEN,       \
                         PRINT_PARAMS_DEFAULT_QUOTE,        \
                         PRINT_PARAMS_DEFAULT_DISPLAY,      \
                         true);

// For debugging
void debug_print_minim_object(MinimObject *obj, MinimEnv *env);

// Writes a string representation of the object to stdout
int print_minim_object(MinimObject *obj, MinimEnv *env, PrintParams *pp);

// Writes a string representation of the object to the buffer
int print_to_buffer(Buffer *bf, MinimObject* obj, MinimEnv *env, PrintParams *pp);

// Writes a string representation of the object to stream.
int print_to_port(MinimObject *obj, MinimEnv *env, PrintParams *pp, FILE *stream);

// Returns a string representation of the object.
char *print_to_string(MinimObject *obj, MinimEnv *env, PrintParams *pp);


// Writes a string representation of syntax to the buffer
int print_syntax_to_buffer(Buffer *bf, MinimObject *stx);

// Writes a string representation of syntax to a port
int print_syntax_to_port(MinimObject *stx, FILE *f);

#endif
