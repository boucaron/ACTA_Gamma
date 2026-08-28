/* ── execution_log_test_main.c ────────────────────────────────────── */
#include "../skill/skill_test_helpers.h"
#include <stdio.h>

extern int run_execution_log_test_create(void);
extern int run_execution_log_test_get(void);
extern int run_execution_log_test_list(void);
extern int run_execution_log_test_count(void);
extern int run_execution_log_test_misc(void);

int main(void)
{
    int f = 0;

    f += run_execution_log_test_create();
    f += run_execution_log_test_get();
    f += run_execution_log_test_list();
    f += run_execution_log_test_count();
    f += run_execution_log_test_misc();

    if (f == 0) {
        printf("PASS: all execution_log tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
