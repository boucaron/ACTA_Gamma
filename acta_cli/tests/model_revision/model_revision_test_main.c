#include "test_helpers.h"
#include <stdio.h>

extern int run_model_revision_test_get();
extern int run_model_revision_test_get_latest();
extern int run_model_revision_test_list();
extern int run_model_revision_test_count();
extern int run_model_revision_test_misc();
extern int run_model_revision_test_deleted();

int main(void)
{
    int f = 0;

    f += run_model_revision_test_get();
    f += run_model_revision_test_get_latest();
    f += run_model_revision_test_list();
    f += run_model_revision_test_count();
    f += run_model_revision_test_misc();
    f += run_model_revision_test_deleted();


    if (f == 0) {
        printf("PASS: all model_revision tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
