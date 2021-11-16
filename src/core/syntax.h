#ifndef _MINIM_SYNTAX_H_
#define _MINIM_SYNTAX_H_

#include "env.h"

struct SyntaxLoc
{
    MinimObject *src;       // source object (usually a string)
    size_t row, col;        // row and col of source location the syntax object
    size_t pos;             // pos of the source location the syntax object
    size_t span;            // span (width) of the source location of the object
} typedef SyntaxLoc;

// Constructs a syntax location
SyntaxLoc *init_syntax_loc(MinimObject *src,
                           size_t row,
                           size_t col,
                           size_t pos,
                           size_t span);

// Returns the length of syntax list
size_t syntax_list_len(MinimObject *stx);

// Returns the length of syntax list. Return SIZE_MAX
// if the list is not proper.
size_t syntax_proper_list_len(MinimObject *stx);

// Throws an error if `ast` is ill-formatted syntax.
void check_syntax(MinimEnv *env, MinimObject *ast);

// Converts an object to syntax.
MinimObject *datum_to_syntax(MinimEnv *env, MinimObject *obj);

// Recursively unwraps a syntax object.
MinimObject *syntax_unwrap_rec(MinimEnv *env, MinimObject *stx);

// Recursively unwraps a syntax object.
// Allows evaluation for `unquote`.
MinimObject *syntax_unwrap_rec2(MinimEnv *env, MinimObject *stx);

#endif
