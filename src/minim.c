#include <stdio.h>
#include <string.h>
#include "common/version.h"
#include "read.h"
#include "repl.h"

static int process_flags(int count, char **args)
{
    for (int i = 0; i < count; ++i)
    {
        if (strcmp(args[i], "-h") == 0)
        {
            printf("Minim v%s \n", MINIM_VERSION_STR);
            printf("Run \"minim\" to run Minim in a REPL\n");
            printf("Run \"minim <flags> ... <file>\" to run Minim on a file\n");
            printf("Flags:\n");
            printf(" -h\thelp\n");
            printf(" -v\tversion\n");
        }
        else if (strcmp(args[i], "-v") == 0)
        {
            printf("Minim v%s \n", MINIM_VERSION_STR);
        }
        else
        {
            return ((count - i == 1) ? 1 : 2);
        }
    }

    return 0;
}

int main(int argc, char** argv)
{
    int status = 0;

    if (argc == 1)
    {
        status = minim_repl();
    }
    else
    {
        status = process_flags(argc - 1, &argv[1]);
        if (status == 1)  // check for file argument
            status = minim_run_file(argv[argc - 1]);
    }

    return status;
}