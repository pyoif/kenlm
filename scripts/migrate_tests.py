#!/usr/bin/env python3
"""Final migration: boost::test → doctest. Handles everything."""
import re, sys
from pathlib import Path

ROOT = Path(r"D:/kenlm-forks")

NEEDS_ARGS = {
    "lm/left_test.cc":           "../util/test_main.hh",
    "lm/model_test.cc":          "../util/test_main.hh",
    "lm/partial_test.cc":        "../util/test_main.hh",
    "lm/common/model_buffer_test.cc": "../../util/test_main.hh",
    "lm/interpolate/tune_instances_test.cc": "../../util/test_main.hh",
    "util/file_piece_test.cc":   "test_main.hh",
}

def migrate(fp, needs_args):
    t = fp.read_text('utf-8')

    # --- includes ---
    t = t.replace(
        "#include <boost/test/included/unit_test.hpp>",
        "#include <doctest/doctest.h>" + ("\n#include \"" + needs_args + "\"" if needs_args else ""))
    t = re.sub(r'#include <boost/test/floating_point_comparison\.hpp>\n', '', t)
    t = re.sub(r'#include <boost/thread/thread\.hpp>\n', '', t)
    t = re.sub(r'#include <boost/lexical_cast\.hpp>\n', '', t)
    t = re.sub(r'#include <boost/functional/hash\.hpp>\n', '#include <functional>\n', t)
    t = re.sub(r'#include <boost/scoped_array\.hpp>\n', '#include <memory>\n', t)
    t = re.sub(r'#include <boost/scoped_ptr\.hpp>\n', '#include <memory>\n', t)
    t = re.sub(r'#include <boost/unordered_map\.hpp>\n', '#include <unordered_map>\n', t)
    t = re.sub(r'#include <boost/unordered_set\.hpp>\n', '#include <unordered_set>\n', t)

    # --- BOOST_TEST_MODULE → DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN ---
    if needs_args:
        t = re.sub(r'#define BOOST_TEST_MODULE \w+\n', '', t)
    else:
        t = re.sub(r'#define BOOST_TEST_MODULE \w+',
                   '#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN', t)

    # --- simple macros ---
    macros = {
        'BOOST_AUTO_TEST_CASE':  'TEST_CASE',
        'BOOST_CHECK_EQUAL':     'CHECK_EQ',
        'BOOST_REQUIRE_EQUAL':   'REQUIRE_EQ',
        'BOOST_CHECK_THROW':     'CHECK_THROWS_AS',
        'BOOST_CHECK_GE':        'CHECK_GE',
        'BOOST_REQUIRE_GE':      'REQUIRE_GE',
        'BOOST_CHECK_NE':        'CHECK_NE',
        'BOOST_REQUIRE_NE':      'REQUIRE_NE',
        'BOOST_CHECK_MESSAGE':   'CHECK_MESSAGE',
        'BOOST_REQUIRE_MESSAGE': 'REQUIRE_MESSAGE',
    }
    for old, new in macros.items():
        t = re.sub(r'\b' + old + r'\b', new, t)

    t = re.sub(r'\bBOOST_REQUIRE\(', 'REQUIRE(', t)
    t = re.sub(r'\bBOOST_CHECK\(!', 'CHECK_FALSE(', t)
    t = re.sub(r'\bBOOST_CHECK\(', 'CHECK(', t)

    # --- quote TEST_CASE args ---
    t = re.sub(r'TEST_CASE\((\w+)\)', r'TEST_CASE("\1")', t)

    # --- BOOST_CHECK_CLOSE → Approx (multi-line safe) ---
    def replace_close(m):
        a, b, tol = m.group(1), m.group(2), m.group(3)
        return (f'CHECK(static_cast<double>({a}) == '
                f'doctest::Approx(static_cast<double>({b})).epsilon(static_cast<double>({tol}) / 100.0))')
    t = re.sub(r'BOOST_CHECK_CLOSE\s*\(\s*(.+?)\s*,\s*(.+?)\s*,\s*([^\s,]+)\s*\)',
               replace_close, t, flags=re.DOTALL)

    # --- BOOST_FIXTURE_TEST_SUITE → static GetModel() for partial_test ---
    if 'BOOST_FIXTURE_TEST_SUITE' in t:
        t = re.sub(r'struct ModelFixture \{[^}]*RestProbingModel m;\n\};?\n', '', t, flags=re.DOTALL)
        t = t.replace('BOOST_FIXTURE_TEST_SUITE(suite, ModelFixture)\n', '')
        t = t.replace('BOOST_AUTO_TEST_SUITE_END()\n', '')
        t = re.sub(r'(const char \*TestLocation\(\)[^}]*\})',
                   r'\1\n\nstatic RestProbingModel &GetModel() {\n  static RestProbingModel m(TestLocation(), SilentConfig());\n  return m;\n}', t)
        t = re.sub(r'(TEST_CASE\("[^"]+"\))\s*\{', r'\1 {\n  RestProbingModel &m = GetModel();', t)

    # --- VLA → vector in model_test ---
    t = re.sub(r'WordIndex context\[in\.length \+ 1\]',
               'std::vector<WordIndex> context(in.length + 1)', t)
    t = re.sub(r'model\.GetState\(context, context \+',
               'model.GetState(context.data(), context.data() +', t)

    # --- boost::unit_test argv → globals ---
    t = t.replace('boost::unit_test::framework::master_test_suite().argc', 'test_argc')
    t = t.replace('boost::unit_test::framework::master_test_suite().argv', 'test_argv')

    # --- boost::ref → std::ref ---
    t = t.replace('boost::ref(', 'std::ref(')

    # --- utility type replacements ---
    t = re.sub(r'boost::scoped_array<([^>]+)>', r'std::unique_ptr<\1[]>', t)
    t = re.sub(r'boost::scoped_ptr<([^>]+)>', r'std::unique_ptr<\1>', t)
    t = re.sub(r'boost::shared_ptr<', 'std::shared_ptr<', t)
    t = re.sub(r'boost::make_shared<', 'std::make_shared<', t)
    t = re.sub(r'boost::unordered_map<', 'std::unordered_map<', t)
    t = re.sub(r'boost::unordered_set<', 'std::unordered_set<', t)
    t = re.sub(r'boost::hash<([^>]+)>', r'std::hash<\1>', t)
    t = t.replace('boost::mt19937', 'std::mt19937')
    t = re.sub(r'boost::uniform_int<([^>]+)>', r'std::uniform_int_distribution<\1>', t)
    t = re.sub(r'boost::variate_generator<[^>]+> [^;]+;\n', '', t)
    t = re.sub(r'#include <boost/random/[^>]+>\n', '', t)

    # lexical_cast replacements
    t = re.sub(r'boost::lexical_cast<std::string>\(', 'std::to_string(', t)
    t = re.sub(r'boost::lexical_cast<double>\(', 'std::stod(', t)
    t = re.sub(r'boost::lexical_cast<float>\(', 'std::stof(', t)
    t = re.sub(r'boost::lexical_cast<unsigned int>\(', '(unsigned int)std::stoul(', t)
    t = re.sub(r'boost::lexical_cast<uint64_t>\(', 'std::stoull(', t)

    # Add <random> if mt19937 used
    if 'mt19937' in t and '#include <random>' not in t:
        t = t.replace('#include <doctest/doctest.h>',
                      '#include <doctest/doctest.h>\n#include <random>')

    # Clean duplicate empty lines
    t = re.sub(r'\n{3,}', '\n\n', t)

    fp.write_text(t, 'utf-8', newline='\n')

def main():
    for f in ROOT.glob('**/*_test.cc'):
        rel = str(f.relative_to(ROOT)).replace('\\', '/')
        if '.git' in rel:
            continue
        args_inc = NEEDS_ARGS.get(rel)
        migrate(f, args_inc)
        print(f"Migrated: {rel}" + (" (+argv)" if args_inc else ""))

if __name__ == '__main__':
    main()
