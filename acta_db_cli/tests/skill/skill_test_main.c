#include "test_helpers.h"
#include <stdio.h>

int main(void)
{
    int f = 0;

    f += run_skill_test_create();
    f += run_skill_test_get();
    f += run_skill_test_update();
    f += run_skill_test_delete_restore();
    f += run_skill_test_move();
    f += run_skill_test_list_count();
    f += run_skill_test_misc();

    if (f == 0) {
        printf("PASS: all skill tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
