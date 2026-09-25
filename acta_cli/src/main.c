/*
 * acta_cli — entry point (thin wrapper)
 *
 * The full CLI flow (parse_globals → early-exit → resolve DB path →
 * open DB → dispatch → close) lives in cli_main() in src/cli_main.c
 * so the test binaries (which link the app objects minus main.o) can
 * drive it in-process — see tests/db/db_test.c, "CLI bootstrap
 * wiring (cli_main)" scenarios.
 */

#include "cli.h"

int main(int argc, char **argv) {
    return cli_main(argc, argv);
}
