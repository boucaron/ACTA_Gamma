#include <stdio.h>

void run_db_tests(void);

void run_execution_log_tests(void);
void run_integration_tests(void);
void run_model_tests(void);
void run_model_folder_tests(void);
void run_model_revision_tests(void);


void run_skill_count_tests(void);
void run_skill_crud_tests(void);
void run_skill_deleted_tests(void);
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

    // Context
    run_context_tests();

    // Model folder
    run_model_folder_tests();

    // Model
    run_model_tests();

    // Model Revision
    run_model_revision_tests();


    // Skill Folder
    run_skill_folder_tests();
    
    // Skill
    run_skill_count_tests();
    run_skill_crud_tests();
    run_skill_deleted_tests();
    run_skill_pagination_tests();
    run_skill_placement_tests();

    // Skill Revision
    run_skill_revision_tests(); 
    

    // Execution
    run_execution_create_tests();
    run_execution_lifecycle_tests();
    run_execution_list_tests();

    // Execution logs
    run_execution_log_tests();

    // Integration
    run_integration_tests();

    printf("\n=== ALL TESTS PASSED ===\n");
    return 0;
}
