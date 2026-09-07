#include <stdio.h>

extern int run_context_test_create();
extern int run_context_test_get();
extern int run_context_test_list();
extern int run_context_test_count();
extern int run_context_test_misc();


int main(void)
{
    int f = 0;

    f += run_context_test_create();
    f += run_context_test_get();
    f += run_context_test_list();
    f += run_context_test_count();
    f += run_context_test_misc();

    if (f == 0) {
        printf("PASS: all context tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
