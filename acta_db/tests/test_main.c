#include <stdio.h>

/* Each suite returns the number of failed assertions in that suite. */

int run_db_tests(void);

int run_execution_log_tests(void);
int run_integration_tests(void);
int run_model_tests(void);
int run_model_folder_tests(void);
int run_model_revision_tests(void);


int run_skill_count_tests(void);
int run_skill_crud_tests(void);
int run_skill_deleted_tests(void);
int run_skill_pagination_tests(void);
int run_skill_placement_tests(void);


int run_skill_folder_tests(void);
int run_skill_revision_tests(void);
int run_context_tests(void);
int run_context_deleted_tests(void);

int run_execution_create_tests(void);
int run_execution_lifecycle_tests(void);
int run_execution_list_tests(void);
int run_execution_deleted_tests(void);

int run_light_queries_tests(void);

int main(void) {
    int failures = 0;

    failures += run_db_tests();

    // Context
    failures += run_context_tests();
    failures += run_context_deleted_tests();

    // Model folder
    failures += run_model_folder_tests();

    // Model
    failures += run_model_tests();

    // Model Revision
    failures += run_model_revision_tests();


    // Skill Folder
    failures += run_skill_folder_tests();

    // Skill
    failures += run_skill_count_tests();
    failures += run_skill_crud_tests();
    failures += run_skill_deleted_tests();
    failures += run_skill_pagination_tests();
    failures += run_skill_placement_tests();

    // Skill Revision
    failures += run_skill_revision_tests();


    // Execution
    failures += run_execution_create_tests();
    failures += run_execution_lifecycle_tests();
    failures += run_execution_list_tests();
    failures += run_execution_deleted_tests();

    // Light-projection listers
    failures += run_light_queries_tests();

    // Execution logs
    failures += run_execution_log_tests();

    // Integration
    failures += run_integration_tests();

    if (failures == 0)
        printf("\n=== ALL TESTS PASSED ===\n");
    else
        printf("\n=== TESTS FAILED: %d assertion(s) failed ===\n", failures);
    return failures == 0 ? 0 : 1;
}
