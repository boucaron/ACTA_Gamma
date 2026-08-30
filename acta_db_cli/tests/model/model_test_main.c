#include "test_helpers.h"
#include <stdio.h>


extern int run_model_test_create();
extern int run_model_test_get();
extern int run_model_test_update();
extern int run_model_test_delete_restore();
extern int run_model_test_move();
extern int run_model_test_list_count();
extern int run_model_test_misc();
extern int run_model_test_error_contract();

int main(void)
{
    int f = 0;

    f += run_model_test_create();
    f += run_model_test_get();
    f += run_model_test_update();
    f += run_model_test_delete_restore();
    f += run_model_test_move();
    f += run_model_test_list_count();
    f += run_model_test_misc();
    f += run_model_test_error_contract();

    if (f == 0) {
        printf("PASS: all model tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
