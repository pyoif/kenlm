// Defines globals + custom main for doctest tests that need CMake TEST_ARGS.
// Link this into test targets that need test_argc/test_argv.
#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

int test_argc;
char **test_argv;

int main(int argc, char **argv) {
  test_argc = argc;
  test_argv = argv;
  doctest::Context ctx(argc, argv);
  return ctx.run();
}
