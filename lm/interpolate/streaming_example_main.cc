#include "../common/compare.hh"
#include "../common/model_buffer.hh"
#include "../common/ngram.hh"
#include "../../util/stream/chain.hh"
#include "../../util/stream/multi_stream.hh"
#include "../../util/stream/sort.hh"
#include "split_worker.hh"

#include <CLI/CLI.hpp>

#if defined(_WIN32) || defined(_WIN64)

// Windows doesn't define <unistd.h>
//
// So we define what we need here instead:
//
#define STDIN_FILENO = 0
#define STDOUT_FILENO = 1
#else // Huzzah for POSIX!
#include <unistd.h>
#endif

/*
 * This is a simple example program that takes in intermediate
 * suffix-sorted ngram files and outputs two sets of files: one for backoff
 * probability values (raw numbers, in suffix order) and one for
 * probability values (ngram id and probability, in *context* order)
 */
int main(int argc, char *argv[]) {
  using namespace lm::interpolate;

  const std::size_t ONE_GB = 1 << 30;
  const std::size_t SIXTY_FOUR_MB = 1 << 26;
  const std::size_t NUMBER_OF_BLOCKS = 2;

  std::string FILE_NAME = "ngrams";
  std::string CONTEXT_SORTED_FILENAME = "csorted-ngrams";
  std::string BACKOFF_FILENAME = "backoffs";
  std::string TMP_DIR = "/tmp/";

  try {
    CLI::App app{"canhazinterp Pass-3 options"};
    bool help = false;

    app.add_flag("-h,--help", help, "Show this help message");
    app.add_option("-n,--ngrams", FILE_NAME, "ngrams file");
    app.add_option("-c,--csortngrams", CONTEXT_SORTED_FILENAME, "context sorted ngrams file");
    app.add_option("-b,--backoffs", BACKOFF_FILENAME, "backoffs file");
    app.add_option("-t,--tmpdir", TMP_DIR, "tmp dir");

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      return app.exit(e);
    }

    // Display help
    if (help) {
      std::cerr << "Usage: " << app.help() << std::endl;
      return 1;
    }
  }
  catch(const std::exception &e) {

    std::cerr << e.what() << std::endl;
    return 1;

  }

  // The basic strategy here is to have three chains:
  // - The first reads the ngram order inputs using ModelBuffer. Those are
  //   then stripped of their backoff values and fed into the third chain;
  //   the backoff values *themselves* are written to the second chain.
  //
  // - The second chain takes the backoff values and writes them out to a
  //   file (one for each order).
  //
  // - The third chain takes just the probability values and ngrams and
  //   writes them out, sorted in context-order, to a file (one for each
  //   order).

  // This will be used to read in the binary intermediate files. There is
  // one file per order (e.g. ngrams.1, ngrams.2, ...)
  lm::ModelBuffer buffer(FILE_NAME);

  // Create a separate chains for each ngram order for:
  // - Input from the intermediate files
  // - Output to the backoff file
  // - Output to the (context-sorted) probability file
  util::stream::Chains ngram_inputs(buffer.Order());
  util::stream::Chains backoff_chains(buffer.Order());
  util::stream::Chains prob_chains(buffer.Order());
  for (std::size_t i = 0; i < buffer.Order(); ++i) {
    ngram_inputs.push_back(util::stream::ChainConfig(
        lm::NGram<lm::ProbBackoff>::TotalSize(i + 1), NUMBER_OF_BLOCKS, ONE_GB));

    backoff_chains.push_back(
        util::stream::ChainConfig(sizeof(float), NUMBER_OF_BLOCKS, ONE_GB));

    prob_chains.push_back(util::stream::ChainConfig(
        sizeof(lm::WordIndex) * (i + 1) + sizeof(float), NUMBER_OF_BLOCKS,
        ONE_GB));
  }

  // This sets the input for each of the ngram order chains to the
  // appropriate file
  buffer.Source(ngram_inputs);

  util::FixedArray<util::scoped_ptr<SplitWorker> > workers(buffer.Order());
  for (std::size_t i = 0; i < buffer.Order(); ++i) {
    // Attach a SplitWorker to each of the ngram input chains, writing to the
    // corresponding order's backoff and probability chains
    workers.push_back(
        new SplitWorker(i + 1, backoff_chains[i], prob_chains[i]));
    ngram_inputs[i] >> std::ref(*workers.back());
  }

  util::stream::SortConfig sort_cfg;
  sort_cfg.temp_prefix = TMP_DIR;
  sort_cfg.buffer_size = SIXTY_FOUR_MB;
  sort_cfg.total_memory = ONE_GB;

  // This will parallel merge sort the individual order files, putting
  // them in context-order instead of suffix-order.
  //
  // Two new threads will be running, each owned by the chains[i] object.
  // - The first executes BlockSorter.Run() to sort the n-gram entries
  // - The second executes WriteAndRecycle.Run() to write each sorted
  //   block to disk as a temporary file
  util::stream::Sorts<lm::ContextOrder> sorts(buffer.Order());
  for (std::size_t i = 0; i < prob_chains.size(); ++i) {
    sorts.push_back(prob_chains[i], sort_cfg, lm::ContextOrder(i + 1));
  }

  // Set the sort output to be on the same chain
  for (std::size_t i = 0; i < prob_chains.size(); ++i) {
    prob_chains[i].Wait();
    sorts[i].Output(prob_chains[i]);
  }

  // Create another model buffer for our output on e.g. csorted-ngrams.1,
  // csorted-ngrams.2, ...
  lm::ModelBuffer output_buf(CONTEXT_SORTED_FILENAME, true, false);
  output_buf.Sink(prob_chains, buffer.Counts());

  // Create a third model buffer for our backoff output on e.g. backoff.1,
  // backoff.2, ...
  lm::ModelBuffer boff_buf(BACKOFF_FILENAME, true, false);
  boff_buf.Sink(backoff_chains, buffer.Counts());

  // Joins all threads that chains owns,
  //    and does a for loop over each chain object in chains,
  //    calling chain.Wait() on each such chain object
  ngram_inputs.Wait(true);
  backoff_chains.Wait(true);
  prob_chains.Wait(true);

  return 0;
}
