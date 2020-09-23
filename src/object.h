#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include <stdio.h>
#include "base.h"

typedef struct _MinimObject
{
    int type;
    void* data;
} MinimObject;

typedef struct _MinimObjectWrapper
{
    MinimObject* obj;
} MinimObjectWrapper;

typedef enum _MinimObjectType
{
    MINIM_OBJ_NUM,
    MINIM_OBJ_SYM,
    MINIM_OBJ_PAIR,
    MINIM_OBJ_ERR,
    MINIM_OBJ_LAST
} MinimObjectType;

// Constructs a single minim object based on the type.
MinimObject *construct_minim_object(MinimObjectType type, ...);

// Returns a copy of the given object.
MinimObject *copy_minim_object(MinimObject* obj);

// Deletes a MinimObject
void free_minim_object(MinimObject *obj);

// Prints a minim object to stdout
int print_minim_object(MinimObject *obj);

#endif