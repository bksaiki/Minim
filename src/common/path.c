#include <string.h>
#include "common.h"
#include "path.h"

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

Buffer *get_directory(const char *file)
{
    Buffer *bf;
    size_t len, newlen;
    
    len = strlen(file);
    newlen = 0;
    for (size_t i = len - 1; i < len; --i)
    {
#ifdef MINIM_WINDOWS
        if (file[i] == '\\')
        {
            newlen = i + 1;
            break;
        }
#else
        if (file[i] == '/')
        {
            newlen = i + 1;
            break;
        }
#endif
    }

    init_buffer(&bf);
    for (size_t i = 0; i < newlen; ++i)
        writec_buffer(bf, file[i]);

    return bf;
}
