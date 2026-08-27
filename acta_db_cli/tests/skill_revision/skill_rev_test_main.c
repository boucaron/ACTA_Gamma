/* ── skill_rev_test_main.c ─────────────────────────────────────────── */
#include "../skill/skill_test_helpers.h"
#include <stdio.h>


extern int run_skill_rev_test_get();
extern int run_skill_rev_test_list();
extern int run_skill_rev_test_count();
extern int run_skill_rev_test_misc();

int main(void)
{
    int f = 0;

    f += run_skill_rev_test_get();
    f += run_skill_rev_test_list();
    f += run_skill_rev_test_count();
    f += run_skill_rev_test_misc();

    if (f == 0) {
        printf("PASS: all skill_revision tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
