#include <string.h>
#include "read.h"

void valid_path(Buffer *valid, const char *maybe)
{
#ifdef MINIM_WINDOWS
    size_t len = strlen(maybe);
    bool first = (len > 0 && maybe[0] == '/');

    for (size_t i = 0; i < len; ++i)
    {
        if (maybe[i] == '/')
        {
            if (i != 0)
            {
                if (first) 
                {
                    writec_buffer(valid, ':');
                    first = false;
                }
                
                writes_buffer(valid, "\\\\");
            }
        }
        else
        {
            writec_buffer(valid, maybe[i]);
        }
    }
#else
    writes_buffer(valid, maybe);
#endif
}