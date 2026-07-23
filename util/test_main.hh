#ifndef UTIL_TEST_MAIN_HH
#define UTIL_TEST_MAIN_HH

// Provides test_argv/test_argc globals so doctest-based tests can access
// CMake TEST_ARGS (argv[1], argv[2], ...), matching the original
// boost::unit_test::framework::master_test_suite() pattern.

extern int test_argc;
extern char **test_argv;

// Define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN before including doctest,
// then include this header. Call REGISTER_TEST_MAIN() in exactly one
// source file (test_main.cc) to define the globals and the custom main().
//
// Files that only consume the globals just include this header.

#endif // UTIL_TEST_MAIN_HH
