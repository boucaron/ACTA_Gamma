#include "../skill/skill_test_helpers.h"
#include <stdio.h>

extern int run_model_folder_test_create();
extern int run_model_folder_test_get();
extern int run_model_folder_test_list_count();
extern int run_model_folder_test_rename();
extern int run_model_folder_test_delete_restore();
extern int run_model_folder_test_move();
extern int run_model_folder_test_misc();


int main(void)
{
    int f = 0;

    f += run_model_folder_test_create();
    f += run_model_folder_test_get();
    f += run_model_folder_test_list_count();
    f += run_model_folder_test_rename();
    f += run_model_folder_test_delete_restore();
    f += run_model_folder_test_move();
    f += run_model_folder_test_misc();

    if (f == 0) {
        printf("PASS: all model_folder tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
