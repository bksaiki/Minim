#include "../src/test/test-common.h"

int main()
{
    bool status = true;
    
    GC_init(&status);
    setup_test_env();

    // sandbox
    {
        compile_test("(let-values ([(f) (lambda () 1)]) (f))", "1");
    }


    {
        const int COUNT = 2;
        char strs[4][256] =
        {
            "1",                    "1",
            "'z",                   "'z",
        };

        printf("Testing simple expressions\n");
        for (int i = 0; i < COUNT; ++i)
            status &= compile_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(let-values ([(x) 1]) x)",                 "1",
            "(let-values ([(x) 1])\
              (let-values ([(y) x])\
               y))",                                    "1",
            "(let-values ([(f) (lambda () 1)]) (f))",   "1"
        };

        printf("Testing let bindings\n");
        for (int i = 0; i < COUNT; ++i)
            status &= compile_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}

