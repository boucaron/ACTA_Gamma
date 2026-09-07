/* ─────────────────────────────────────────────────────────────────────
 * execution_test_main.c
 * Top-level runner for all execution unit tests.
 * ───────────────────────────────────────────────────────────────────── */
#include "test_helpers.h"
#include <stdio.h>


extern int run_execution_test_create();
extern int run_execution_test_get();
extern int run_execution_test_lifecycle();
extern int run_execution_test_list_count();
extern int run_execution_test_misc();

int main(void)
{
    int f = 0;

    f += run_execution_test_create();
    f += run_execution_test_get();
    f += run_execution_test_lifecycle();
    f += run_execution_test_list_count();
    f += run_execution_test_misc();

    if (f == 0) {
        printf("PASS: all execution tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
