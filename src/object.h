#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include <stdio.h>
#include "common.h"

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
    MINIM_OBJ_LAST
} MinimObjectType;

// Prints a minim object to stdout
int print_minim_object(MinimObject* obj);

#endif