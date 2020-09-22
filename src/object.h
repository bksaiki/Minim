#ifndef _MINIM_OBJECT_H_
#define _MINIM_OBJECT_H_

#include <stdio.h>
#include "common.h"

typedef struct
{
    int type;
    void* data;
} MinimObject;

typedef enum
{
    MINIM_OBJ_NUM,
    MINIM_OBJ_SYM,
    MINIM_OBJ_STR,
    MINIM_OBJ_LAST
} MinimObjectType;

// Prints a minim object to stdout
int print_minim_object(MinimObject *obj);

#endif