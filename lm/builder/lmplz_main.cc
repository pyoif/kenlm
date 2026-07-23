#include "output.hh"
#include "pipeline.hh"
#include "../lm_exception.hh"
#include "../../util/file.hh"
#include "../../util/file_piece.hh"
#include "../../util/usage.hh"

#include <CLI/CLI.hpp>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Parse and validate pruning thresholds then return vector of threshold counts
// for each n-grams order.
std::vector<uint64_t> ParsePruning(const std::vector<std::string> &param, std::size_t order) {
  std::vector<uint64_t> prune_thresholds;
  prune_thresholds.reserve(order);
  for (std::vector<std::string>::const_iterator it(param.begin()); it != param.end(); ++it) {
    try {
      std::size_t pos;
      uint64_t val = std::stoull(*it, &pos);
      UTIL_THROW_IF(pos != it->size(), util::Exception, "Bad pruning threshold " << *it);
      prune_thresholds.push_back(val);
    } catch(const std::exception &) {
      UTIL_THROW(util::Exception, "Bad pruning threshold " << *it);
    }
  }

  if (prune_thresholds.empty()) {
    prune_thresholds.resize(order, 0);
    return prune_thresholds;
  }

  UTIL_THROW_IF(prune_thresholds.size() > order, util::Exception, "You specified pruning thresholds for orders 1 through " << prune_thresholds.size() << " but the model only has order " << order);

  uint64_t lower_threshold = 0;
  for (std::vector<uint64_t>::iterator it = prune_thresholds.begin(); it != prune_thresholds.end(); ++it) {
    UTIL_THROW_IF(lower_threshold > *it, util::Exception, "Pruning thresholds should be in non-decreasing order.  Otherwise substrings would be removed, which is bad for query-time data structures.");
    lower_threshold = *it;
  }

  prune_thresholds.resize(order, prune_thresholds.back());
  return prune_thresholds;
}

lm::builder::Discount ParseDiscountFallback(const std::vector<std::string> &param) {
  lm::builder::Discount ret;
  UTIL_THROW_IF(param.size() > 3, util::Exception, "Specify at most three fallback discounts: 1, 2, and 3+");
  UTIL_THROW_IF(param.empty(), util::Exception, "Fallback discounting enabled, but no discount specified");
  ret.amount[0] = 0.0;
  for (unsigned i = 0; i < 3; ++i) {
    float discount = std::stof(param[i < param.size() ? i : (param.size() - 1)]);
    UTIL_THROW_IF(discount < 0.0 || discount > static_cast<float>(i+1), util::Exception, "The discount for count " << (i+1) << " was parsed as " << discount << " which is not in the range [0, " << (i+1) << "].");
    ret.amount[i + 1] = discount;
  }
  return ret;
}

} // namespace

int main(int argc, char *argv[]) {
  try {
    lm::builder::PipelineConfig pipeline;

    std::string text, intermediate, arpa;
    std::vector<std::string> pruning;
    std::vector<std::string> discount_fallback;
    std::string ram_str, minimum_block_str, sort_block_str;
    bool verbose_header;
    bool help = false;
    bool skip_symbols = false;

    CLI::App app{"Language model building options"};

    app.add_flag("-h,--help", help, "Show this help message");
    app.add_option("-o,--order", pipeline.order, "Order of the model")->required();
    app.add_option("--interpolate_unigrams", pipeline.initial_probs.interpolate_unigrams,
      "Interpolate the unigrams (default) as opposed to giving lots of mass to <unk> like SRI.  If you want SRI's behavior with a large <unk> and the old lmplz default, use --interpolate_unigrams 0.")
      ->default_val(true);
    app.add_flag("--skip_symbols", skip_symbols,
      "Treat <s>, </s>, and <unk> as whitespace instead of throwing an exception");
    app.add_option("-T,--temp_prefix", pipeline.sort.temp_prefix, "Temporary file prefix")
      ->default_val(util::DefaultTempDirectory());
    app.add_option("-S,--memory", ram_str, "Sorting memory")
      ->default_val(util::GuessPhysicalMemory() ? "80%" : "1G");
    app.add_option("--minimum_block", minimum_block_str, "Minimum block size to allow")
      ->default_val("8K");
    app.add_option("--sort_block", sort_block_str, "Size of IO operations for sort (determines arity)")
      ->default_val("64M");
    app.add_option("--block_count", pipeline.block_count, "Block count (per order)")->default_val(2);
    app.add_option("--vocab_estimate", pipeline.vocab_estimate,
      "Assume this vocabulary size for purposes of calculating memory in step 1 (corpus count) and pre-sizing the hash table")
      ->default_val(1000000);
    app.add_option("--vocab_pad", pipeline.vocab_size_for_unk,
      "If the vocabulary is smaller than this value, pad with <unk> to reach this size. Requires --interpolate_unigrams")
      ->default_val(0);
    app.add_flag("--verbose_header", verbose_header,
      "Add a verbose header to the ARPA file that includes information such as token count, smoothing type, etc.");
    app.add_option("--text", text, "Read text from a file instead of stdin");
    app.add_option("--arpa", arpa, "Write ARPA to a file instead of stdout");
    app.add_option("--intermediate", intermediate,
      "Write ngrams to intermediate files.  Turns off ARPA output (which can be reactivated by --arpa file).  Forces --renumber on.");
    app.add_flag("--renumber", pipeline.renumber_vocabulary,
      "Renumber the vocabulary identifiers so that they are monotone with the hash of each string.  This is consistent with the ordering used by the trie data structure.");
    app.add_flag("--collapse_values", pipeline.output_q,
      "Collapse probability and backoff into a single value, q that yields the same sentence-level probabilities.  See http://kheafield.com/professional/edinburgh/rest_paper.pdf for more details, including a proof.");
    app.add_option("--prune", pruning,
      "Prune n-grams with count less than or equal to the given threshold.  Specify one value for each order i.e. 0 0 1 to prune singleton trigrams and above.  The sequence of values must be non-decreasing and the last value applies to any remaining orders. Default is to not prune, which is equivalent to --prune 0.")
      ->expected(-1);
    app.add_option("--limit_vocab_file", pipeline.prune_vocab_file,
      "Read allowed vocabulary separated by whitespace. N-grams that contain vocabulary items not in this list will be pruned. Can be combined with --prune arg")
      ->default_val("");
    // Strings for discount_fallback to allow empty default
    app.add_option("--discount_fallback", discount_fallback,
      "The closed-form estimate for Kneser-Ney discounts does not work without singletons or doubletons.  It can also fail if these values are out of range.  This option falls back to user-specified discounts when the closed-form estimate fails.  Note that this option is generally a bad idea: you should deduplicate your corpus instead.  However, class-based models need custom discounts because they lack singleton unigrams.  Provide up to three discounts (for adjusted counts 1, 2, and 3+), which will be applied to all orders where the closed-form estimates fail.")
      ->expected(-1);

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      return app.exit(e);
    }

    if (help) {
      std::cerr <<
        "Builds unpruned language models with modified Kneser-Ney smoothing.\n\n"
        "Please cite:\n"
        "@inproceedings{Heafield-estimate,\n"
        "  author = {Kenneth Heafield and Ivan Pouzyrevsky and Jonathan H. Clark and Philipp Koehn},\n"
        "  title = {Scalable Modified {Kneser-Ney} Language Model Estimation},\n"
        "  year = {2013},\n"
        "  month = {8},\n"
        "  booktitle = {Proceedings of the 51st Annual Meeting of the Association for Computational Linguistics},\n"
        "  address = {Sofia, Bulgaria},\n"
        "  url = {http://kheafield.com/professional/edinburgh/estimate\\_paper.pdf},\n"
        "}\n\n"
        "Provide the corpus on stdin.  The ARPA file will be written to stdout.  Order of\n"
        "the model (-o) is the only mandatory option.  As this is an on-disk program,\n"
        "setting the temporary file location (-T) and sorting memory (-S) is recommended.\n\n"
        "Memory sizes are specified like GNU sort: a number followed by a unit character.\n"
        "Valid units are \% for percentage of memory (supported platforms only) and (in\n"
        "increasing powers of 1024): b, K, M, G, T, P, E, Z, Y.  Default is K (*1024).\n";
      uint64_t mem = util::GuessPhysicalMemory();
      if (mem) {
        std::cerr << "This machine has " << mem << " bytes of memory.\n\n";
      } else {
        std::cerr << "Unable to determine the amount of memory on this machine.\n\n";
      }
      std::cerr << app.help() << std::endl;
      return 1;
    }

    // Parse size strings after help check
    pipeline.sort.total_memory = util::ParseSize(ram_str);
    pipeline.minimum_block = util::ParseSize(minimum_block_str);
    pipeline.sort.buffer_size = util::ParseSize(sort_block_str);

    if (pipeline.vocab_size_for_unk && !pipeline.initial_probs.interpolate_unigrams) {
      std::cerr << "--vocab_pad requires --interpolate_unigrams be on" << std::endl;
      return 1;
    }

    if (skip_symbols) {
      pipeline.disallowed_symbol_action = lm::COMPLAIN;
    } else {
      pipeline.disallowed_symbol_action = lm::THROW_UP;
    }

    if (!discount_fallback.empty()) {
      pipeline.discount.fallback = ParseDiscountFallback(discount_fallback);
      pipeline.discount.bad_action = lm::COMPLAIN;
    } else {
      pipeline.discount.fallback = lm::builder::Discount();
      pipeline.discount.bad_action = lm::THROW_UP;
    }

    // parse pruning thresholds.  These depend on order, so it is not done as a notifier.
    pipeline.prune_thresholds = ParsePruning(pruning, pipeline.order);

    pipeline.prune_vocab = !pipeline.prune_vocab_file.empty();

    util::NormalizeTempPrefix(pipeline.sort.temp_prefix);

    lm::builder::InitialProbabilitiesConfig &initial = pipeline.initial_probs;
    // TODO: evaluate options for these.
    initial.adder_in.total_memory = 32768;
    initial.adder_in.block_count = 2;
    initial.adder_out.total_memory = 32768;
    initial.adder_out.block_count = 2;
    pipeline.read_backoffs = initial.adder_out;

    // Read from stdin, write to stdout by default
    util::scoped_fd in(0), out(1);
    if (!text.empty()) {
      in.reset(util::OpenReadOrThrow(text.c_str()));
    }
    if (!arpa.empty()) {
      out.reset(util::CreateOrThrow(arpa.c_str()));
    }

    try {
      bool writing_intermediate = !intermediate.empty();
      if (writing_intermediate) {
        pipeline.renumber_vocabulary = true;
      }
      lm::builder::Output output(writing_intermediate ? intermediate : pipeline.sort.temp_prefix, writing_intermediate, pipeline.output_q);
      if (!writing_intermediate || !arpa.empty()) {
        output.Add(new lm::builder::PrintHook(out.release(), verbose_header));
      }
      lm::builder::Pipeline(pipeline, in.release(), output);
    } catch (const util::MallocException &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << "Try rerunning with a more conservative -S setting than " << ram_str << std::endl;
      return 1;
    }
    util::PrintUsage(std::cerr);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
