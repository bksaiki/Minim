#include "object.h"

int print_minim_object(MinimObject *obj)
{
    if (obj->type == MINIM_OBJ_NUM)
    {
        printf("%i", *((int*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_SYM)
    {
        printf("%s", ((char*)obj->data));
    }
    else if (obj->type == MINIM_OBJ_STR)
    {
        printf("%s", ((char*)obj->data));
    }
    else
    {
        printf("<!error type!>");
        return 1;
    }

    return 0;
}