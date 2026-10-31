/**
 * @file
 * Checks that the build and harness work.
 */

#include "minim.h"
#include "test.h"

int main(void) {
    CHECK(1 + 1 == 2);
    CHECK_EQ(1 + 1, 2);
    return test_result();
}
