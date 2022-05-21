#include <stdio.h>
#include <string.h>
#include "common/version.h"
#include "run.h"
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
            printf(" --no-cache\t\tdon't cache expanded Minim code");

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
        else if (strcmp(args[i], "--no-cache") == 0)
        {
            *pflags |= MINIM_FLAG_NO_CACHE;
        }
        else if (strcmp(args[i], "--compile") == 0)
        {
            *pflags |= MINIM_FLAG_COMPILE;
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

static void log_stats()
{
    if (environment_variable_existsp("MINIM_LOG"))
    {
        printf("Runtime statistics:\n");
        printf(" Expressions evaluated: %zu\n", global.stat_exprs);
        printf(" Functions called: %zu\n", global.stat_procs);
        printf(" Objects created: %zu\n", global.stat_objs);
    }
}

int main(int argc, char** argv)
{
    uint32_t flags = 0;
    int status = 0, flagc = 0;

    flagc = process_flags(argc - 1, &argv[1], &flags);
    flags |= MINIM_FLAG_NO_CACHE;
    if (flagc == -1)
        return 1;

    if (flags & MINIM_FLAG_NO_RUN)
        return 0;

    GC_init(&flags);
    status = (argc - flagc == 1) ?
             minim_repl(argv, flags) :
             minim_run(argv[flagc + 1], flags);
    log_stats();
    GC_finalize();

    return status;
}
