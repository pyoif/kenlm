# Fix left_test.cc
cat > /tmp/left_patch.sed << 'SED'
s|const char \*FileLocation() { return "test.arpa"; }|const char *FileLocation() {\n  if (test_argc < 2) return "test.arpa";\n  return test_argv[1];\n}|
SED
sed -i -f /tmp/left_patch.sed lm/left_test.cc
echo "Fixed left_test.cc"

# Fix model_test.cc
cat > /tmp/model_patch.sed << 'SED'
s|const char \*TestLocation() { return "test.arpa"; }|const char *TestLocation() {\n  if (test_argc < 3) return "test.arpa";\n  return test_argv[strstr(test_argv[1], "nounk") ? 2 : 1];\n}|
s|const char \*TestNoUnkLocation() { return "test_nounk.arpa"; }|const char *TestNoUnkLocation() {\n  if (test_argc < 3) return "test_nounk.arpa";\n  return test_argv[strstr(test_argv[1], "nounk") ? 1 : 2];\n}|
SED
sed -i -f /tmp/model_patch.sed lm/model_test.cc
echo "Fixed model_test.cc"

# Fix partial_test.cc
cat > /tmp/partial_patch.sed << 'SED'
s|const char \*TestLocation() { return "test.arpa"; }|const char *TestLocation() {\n  if (test_argc < 2) return "test.arpa";\n  return test_argv[1];\n}|
SED
sed -i -f /tmp/partial_patch.sed lm/partial_test.cc
echo "Fixed partial_test.cc"

# Fix model_buffer_test.cc
cat > /tmp/mb_patch.sed << 'SED'
s|std::string dir("test_data");|std::string dir((test_argc >= 2) ? test_argv[1] : "test_data");|
SED
sed -i -f /tmp/mb_patch.sed lm/common/model_buffer_test.cc
echo "Fixed model_buffer_test.cc"

# Fix tune_instances_test.cc
cat > /tmp/ti_patch.sed << 'SED'
s|std::string dir("../common/test_data");|std::string dir((test_argc == 2) ? test_argv[1] : "../common/test_data");|
SED
sed -i -f /tmp/ti_patch.sed lm/interpolate/tune_instances_test.cc
echo "Fixed tune_instances_test.cc"

# Fix file_piece_test.cc
cat > /tmp/fp_patch.sed << 'SED'
s|// When run via ctest, argv\[1\] provides the test data directory.\n.*return "file_piece.cc";|std::string FileLocation() {\n  if (test_argc < 2) return "file_piece.cc";\n  return test_argv[1];\n}|
SED
echo "Fixed file_piece_test.cc - manual edit needed"
