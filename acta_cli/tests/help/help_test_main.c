#include "test_helpers.h"
#include <stdio.h>

/* single entry point for the P0 per-action help suite */
extern int run_help_test_all(void);

int main(void)
{
    int f = run_help_test_all();

    if (f == 0) {
        printf("PASS: all per-action help (P0) tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
