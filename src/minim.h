/*
    Public header file for Minim
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Object types

typedef enum {
    MINIM_BOOL,
    MINIM_SYMBOL,
    MINIM_FIXNUM,
    MINIM_CHAR,
    MINIM_STRING,
    MINIM_PAIR,
    MINIM_PRIM_PROC,
    MINIM_CLOSURE_PROC,
    MINIM_INPUT_PORT,
    MINIM_OUTPUT_PORT,
} minim_object_type;

// Object structs

typedef struct {
    minim_object_type type;
} minim_object;

typedef struct {
    minim_object_type type;
    int value;
} minim_bool_object;

typedef struct {
    minim_object_type type;
    char *value;
} minim_symbol_object;

typedef struct {
    minim_object_type type;
    long value; /* TODO: manage size of this */
} minim_fixnum_object;

typedef struct {
    minim_object_type type;
    int value;
} minim_string_object;

typedef struct {
    minim_object_type type;
    int value;
} minim_char_object;

typedef struct {
    minim_object_type type;
    minim_object *car;
    minim_object *cdr;
} minim_pair_object;

// Special objects


