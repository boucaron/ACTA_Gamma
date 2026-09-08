#include "test_helpers.h"
#include <stdio.h>

/* single entry point for the cli_util suite (fuzzy matcher + dispatch
 * entity-error contract + T2 code/exit invariant contract) */
extern int run_cli_util_test_fuzzy(void);
extern int run_cli_util_test_dispatch(void);
extern int run_cli_util_test_error_contract(void);

int main(void)
{
    int f = 0;

    f += run_cli_util_test_fuzzy();
    f += run_cli_util_test_dispatch();
    f += run_cli_util_test_error_contract();

    if (f == 0) {
        printf("PASS: all cli_util tests passed\n");
        return 0;
    }
    printf("FAIL: %d assertion(s) failed\n", f);
    return 1;
}
