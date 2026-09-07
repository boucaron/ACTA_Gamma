#include "test_helpers.h"
#include <stdio.h>

/* single entry point for the db sub-command suite */
extern int run_db_test_all(void);

int main(void)
{
    int f = run_db_test_all();

    if (f == 0) {
        printf("PASS: all db tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
