#include "combine_counts.hh"
#include "corpus_count.hh"
#include "../common/compare.hh"
#include "../../util/stream/chain.hh"
#include "../../util/stream/io.hh"
#include "../../util/stream/sort.hh"
#include "../../util/file.hh"
#include "../../util/file_piece.hh"
#include "../../util/usage.hh"

#include <CLI/CLI.hpp>

#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  unsigned order;
  std::string ram_str;
  std::string temp_prefix, vocab_table, vocab_list;

  CLI::App app{"corpus count"};
  bool help = false;
  app.add_flag("-h,--help", help, "Show this help message");
  app.add_option("-o,--order", order, "Order")->required();
  app.add_option("-T,--temp_prefix", temp_prefix, "Temporary file prefix")->default_val(util::DefaultTempDirectory());
  app.add_option("-S,--memory", ram_str, "RAM")->default_val("80%");
  app.add_option("--read_vocab_table", vocab_table, "Vocabulary hash table to read.  This should be a probing hash table with size at the beginning.");
  app.add_option("--write_vocab_list", vocab_list, "Vocabulary list to write as null-delimited strings.");

  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError &e) {
    return app.exit(e);
  }

  if (help) {
    std::cerr << "Counts n-grams from standard input.\n" << app.help() << std::endl;
    return 1;
  }

  std::size_t ram = util::ParseSize(ram_str);

  if (!(vocab_table.empty() ^ vocab_list.empty())) {
    std::cerr << "Specify one of --read_vocab_table or --write_vocab_list for vocabulary handling." << std::endl;
    return 1;
  }

  util::NormalizeTempPrefix(temp_prefix);

  util::scoped_fd vocab_file(vocab_table.empty() ? util::CreateOrThrow(vocab_list.c_str()) : util::OpenReadOrThrow(vocab_table.c_str()));

  std::size_t blocks = 2;
  std::size_t remaining_size = ram - util::SizeOrThrow(vocab_file.get());

  std::size_t memory_for_chain =
    static_cast<float>(remaining_size) /
    (static_cast<float>(blocks) + lm::builder::CorpusCount::DedupeMultiplier(order)) *
    static_cast<float>(blocks);
  std::cerr << "Using " << memory_for_chain << " for chains." << std::endl;
  
  util::stream::Chain chain(util::stream::ChainConfig(lm::NGram<uint64_t>::TotalSize(order), blocks, memory_for_chain));
  util::FilePiece f(0, NULL, &std::cerr);
  uint64_t token_count = 0;
  lm::WordIndex type_count = 0;
  std::vector<bool> empty_prune;
  std::string empty_string;
  lm::builder::CorpusCount counter(f, vocab_file.get(), vocab_table.empty(), token_count, type_count, empty_prune, empty_string, chain.BlockSize() / chain.EntrySize(), lm::THROW_UP);
  chain >> std::ref(counter);

  util::stream::SortConfig sort_config;
  sort_config.temp_prefix = temp_prefix;
  sort_config.buffer_size = 64 * 1024 * 1024;
  sort_config.total_memory = remaining_size;
  util::stream::Sort<lm::SuffixOrder, lm::builder::CombineCounts> sorted(chain, sort_config, lm::SuffixOrder(order), lm::builder::CombineCounts());
  chain.Wait(true);
  util::stream::Chain chain2(util::stream::ChainConfig(lm::NGram<uint64_t>::TotalSize(order), blocks, sort_config.buffer_size));
  sorted.Output(chain2);
  chain2 >> util::stream::WriteAndRecycle(1);
  chain2.Wait(true);
  return 0;
}
