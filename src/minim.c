#include <stdio.h>
#include <string.h>
#include "common/version.h"
#include "read.h"
#include "repl.h"

static int process_flags(int count, char **args, uint32_t *pflags)
{
    int i;
    
    for (i = 0; i < count; ++i)
    {
        if (strcmp(args[i], "-h") == 0)
        {
            printf("Minim v%s \n", MINIM_VERSION_STR);
            printf("Run \"minim\" to run Minim in a REPL\n");
            printf("Run \"minim <flags> ... <file>\" to run Minim on a file\n");
            printf("Flag:\n");
            printf(" -h\t\t\thelp\n");
            printf(" -v\t\t\tversion\n");
            printf(" --no-libs\t\tdon't load library files\n");

            *pflags |= MINIM_FLAG_NO_RUN;
        }
        else if (strcmp(args[i], "-v") == 0)
        {
            printf("Minim v%s \n", MINIM_VERSION_STR);
            *pflags |= MINIM_FLAG_NO_RUN;
        }
        else if (strcmp(args[i], "--no-libs") == 0)
        {
            *pflags |= MINIM_FLAG_LOAD_LIBS;
        }
        else if (args[i][0] == '-')
        {
            printf("Unrecognized flag: %s\n", args[i]);
            return -1;
        }
        else
        {
            break;
        }
    }

    return i;
}

int main(int argc, char** argv)
{
    uint32_t flags = 0;
    int status = 0, flagc = 0;

    flagc = process_flags(argc - 1, &argv[1], &flags);
    if (flagc == -1)
        return 1;

    if (flags & MINIM_FLAG_NO_RUN)
        return 0;

    GC_init(&flags);

    if (argc - flagc == 1)
        status = minim_repl(flags);
    else
        status = minim_run(argv[flagc + 1], flags);

    GC_finalize();

    return status;
}
