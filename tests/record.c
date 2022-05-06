#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
            "(record 1)",           "#<record>",
            "(record 1 2)",         "#<record>",
            "(record 1 2 3)",       "#<record>"
        };

        printf("Testing 'record'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 6;
        char strs[12][256] =
        {
            "(make-record 0)",      "#<record>",
            "(make-record 1)",      "#<record>",
            "(make-record 3)",      "#<record>",
            "(make-record 0 1)",    "#<record>",
            "(make-record 1 1)",    "#<record>",
            "(make-record 3 1)",    "#<record>"
        };

        printf("Testing 'make-record'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(record-length (make-record 0))",          "0",
            "(record-length (make-record 1))",          "1",
            "(record-length (make-record 3))",          "3"
        };

        printf("Testing 'record-length'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 3;
        char strs[6][256] =
        {
            "(record-ref (record 'type 1) 0)",        "1",
            "(record-ref (record 'type 1 2 3) 1)",    "2",
            "(record-ref (record 'type 1 2 3) 2)",    "3"
        };

        printf("Testing 'record-ref'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 1;
        char strs[2][256] =
        {
            "(let-values ([(r) (record 'type 1)]) (record-set! r 0 2) (record-ref r 0))",
            "2"
        };

        printf("Testing 'record-set!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    {
        const int COUNT = 1;
        char strs[2][256] =
        {
            "(let-values ([(r) (record 'type)]) (record-set-type! r 1) (record-type r))",
            "1"
        };

        printf("Testing 'record-type / record-set-type!'\n");
        for (int i = 0; i < COUNT; ++i)
            status &= run_test(strs[2 * i], strs[2 * i + 1]);
    }

    GC_finalize();

    return (int)(!status);
}
