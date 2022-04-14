#include "../src/test/test-common.h"

int main()
{
    bool status = true;
    
    GC_init(&status);
    setup_test_env();

    {
        const int COUNT = 4;
        char strs[8][256] =
        {
            "1",                    "1",
            "'z",                   "'z",
            "(+ 1 2)",              "3",
            "(cos 0)",              "1",
            // "(+ (cos 0) 2)",        "3",
            // "(* (sin 2) 0)",        "0"
        };

        printf("Testing simple expressions\n");
        for (int i = 0; i < COUNT; ++i)
            status &= compile_test(strs[2 * i], strs[2 * i + 1]);
    }

    // {
    //     const int COUNT = 1;
    //     char strs[2][256] =
    //     {
    //         "(let-values ([(x) 1]) x)",     "1"
    //     };

    //     printf("Testing let bindings\n");
    //     for (int i = 0; i < COUNT; ++i)
    //         status &= compile_test(strs[2 * i], strs[2 * i + 1]);
    // }

    GC_finalize();

    return (int)(!status);
}

