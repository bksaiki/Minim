#include "../src/test/test-common.h"

int main()
{
    bool status = true;
    
    GC_init(&status);
    setup_test_env();

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "1",            "1",
            "(+ 1 2)",      "3",
            "'z",           "'z"
        };

        printf("Testing single expressions\n");
        for (int i = 0; i < COUNT; ++i)
            status &= compile_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 1;
        char strs[2][256] =
        {
            "(let-values ([(x) 1]) x)",
             "1"
        };

        printf("Testing multi-expressions\n");
        for (int i = 0; i < COUNT; ++i)
            status &= compile_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

