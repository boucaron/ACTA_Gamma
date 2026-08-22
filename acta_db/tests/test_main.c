#include <stdio.h>

void run_db_tests(void);

void run_execution_log_tests(void);
void run_integration_tests(void);
void run_model_tests(void);
void run_model_folder_tests(void);
void run_model_revision_tests(void);

void run_skill_tests(void);
void run_skill_count_tests(void);
void run_skill_crud_tests(void);
void run_skill_pagination_tests(void);
void run_skill_placement_tests(void);


void run_skill_folder_tests(void);
void run_skill_revision_tests(void);
void run_context_tests(void);

void run_execution_create_tests(void);
void run_execution_lifecycle_tests(void);
void run_execution_list_tests(void);

int main(void) {
    run_db_tests();
    run_context_tests();       /* if test_context.c exists */
    run_model_folder_tests();
    run_model_tests();
    run_model_revision_tests();

    run_skill_folder_tests();
    
    run_skill_tests(); // Split
    run_skill_count_tests();
    run_skill_crud_tests();
    run_skill_pagination_tests();
    run_skill_placement_tests();

    run_skill_revision_tests(); 
    

    run_execution_create_tests();
    run_execution_lifecycle_tests();
    run_execution_list_tests();

    run_execution_log_tests();
    run_integration_tests();

    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
