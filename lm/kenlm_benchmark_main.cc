#include "model.hh"
#include "../util/file_stream.hh"
#include "../util/file.hh"
#include "../util/file_piece.hh"
#include "../util/usage.hh"
#include "../util/thread_pool.hh"
#include "../util/range.hh"

#include <CLI/CLI.hpp>

#include <iostream>
#include <thread>
#include <cstring>

#include <stdint.h>

namespace {

template <class Model, class Width> void ConvertToBytes(const Model &model, int fd_in) {
  util::FilePiece in(fd_in);
  util::FileStream out(1);
  Width width;
  StringPiece word;
  const Width end_sentence = (Width)model.GetVocabulary().EndSentence();
  while (true) {
    while (in.ReadWordSameLine(word)) {
      width = (Width)model.GetVocabulary().Index(word);
      out.write(&width, sizeof(Width));
    }
    if (!in.ReadLineOrEOF(word)) break;
    out.write(&end_sentence, sizeof(Width));
  }
}

template <class Model, class Width> class Worker {
  public:
    explicit Worker(const Model &model, double &add_total) : model_(model), total_(0.0), add_total_(add_total) {}

    // Destructors happen in the main thread, so there's no race for add_total_.
    ~Worker() { add_total_ += total_; }

    typedef util::Range<Width *> Request;

    void operator()(Request request) {
      const lm::ngram::State *const begin_state = &model_.BeginSentenceState();
      const lm::ngram::State *next_state = begin_state;
      const Width kEOS = model_.GetVocabulary().EndSentence();
      float sum = 0.0;
      // Do even stuff first.
      const Width *even_end = request.begin() + (request.size() & ~1);
      // Alternating states
      const Width *i;
      for (i = request.begin(); i != even_end;) {
        sum += model_.FullScore(*next_state, *i, state_[1]).prob;
        next_state = (*i++ == kEOS) ? begin_state : &state_[1];
        sum += model_.FullScore(*next_state, *i, state_[0]).prob;
        next_state = (*i++ == kEOS) ? begin_state : &state_[0];
      }
      // Odd corner case.
      if (request.size() & 1) {
        sum += model_.FullScore(*next_state, *i, state_[2]).prob;
        next_state = (*i++ == kEOS) ? begin_state : &state_[2];
      }
      total_ += sum;
    }

  private:
    const Model &model_;
    double total_;
    double &add_total_;

    lm::ngram::State state_[3];
};

struct Config {
  int fd_in;
  std::size_t threads;
  std::size_t buf_per_thread;
  bool query;
};

template <class Model, class Width> void QueryFromBytes(const Model &model, const Config &config) {
  util::FileStream out(1);
  out << "Threads: " << config.threads << '\n';
  const Width kEOS = model.GetVocabulary().EndSentence();
  double total = 0.0;
  // Number of items to have in queue in addition to everything in flight.
  const std::size_t kInQueue = 3;
  std::size_t total_queue = config.threads + kInQueue;
  std::vector<Width> backing(config.buf_per_thread * total_queue);
  double loaded_cpu;
  double loaded_wall;
  uint64_t queries = 0;
  {
    util::RecyclingThreadPool<Worker<Model, Width> > pool(total_queue, config.threads, Worker<Model, Width>(model, total), util::Range<Width *>((Width*)0, (Width*)0));

    for (std::size_t i = 0; i < total_queue; ++i) {
      pool.PopulateRecycling(util::Range<Width *>(&backing[i * config.buf_per_thread], &backing[i * config.buf_per_thread]));
    }

    loaded_cpu = util::CPUTime();
    loaded_wall = util::WallTime();
    out << "To Load, CPU: " << loaded_cpu << " Wall: " << loaded_wall << '\n';
    util::Range<Width *> overhang((Width*)0, (Width*)0);
    while (true) {
      util::Range<Width *> buf = pool.Consume();
      std::memmove(buf.begin(), overhang.begin(), overhang.size() * sizeof(Width));
      std::size_t got = util::ReadOrEOF(config.fd_in, buf.begin() + overhang.size(), (config.buf_per_thread - overhang.size()) * sizeof(Width));
      if (!got && overhang.empty()) break;
      UTIL_THROW_IF2(got % sizeof(Width), "File size not a multiple of vocab id size " << sizeof(Width));
      Width *read_end = buf.begin() + overhang.size() + got / sizeof(Width);
      Width *last_eos;
      for (last_eos = read_end - 1; ; --last_eos) {
        UTIL_THROW_IF2(last_eos <= buf.begin(), "Encountered a sentence longer than the buffer size of " << config.buf_per_thread << " words.  Rerun with increased buffer size. TODO: adaptable buffer");
        if (*last_eos == kEOS) break;
      }
      buf = util::Range<Width*>(buf.begin(), last_eos + 1);
      overhang = util::Range<Width*>(last_eos + 1, read_end);
      queries += buf.size();
      pool.Produce(buf);
    }
  } // Drain pool.

  double after_cpu = util::CPUTime();
  double after_wall = util::WallTime();
  util::FileStream(2, 70) << "Probability sum: " << total << '\n';
  out << "Queries: " << queries << '\n';
  out << "Excluding load, CPU: " << (after_cpu - loaded_cpu) << " Wall: " << (after_wall - loaded_wall) << '\n';
  double cpu_per_entry = ((after_cpu - loaded_cpu) / static_cast<double>(queries));
  double wall_per_entry = ((after_wall - loaded_wall) / static_cast<double>(queries));
  out << "Seconds per query excluding load, CPU: " << cpu_per_entry << " Wall: " << wall_per_entry << '\n';
  out << "Queries per second excluding load, CPU: " << (1.0/cpu_per_entry) << " Wall: " << (1.0/wall_per_entry) << '\n';
  out << "RSSMax: " << util::RSSMax() << '\n';
}

template <class Model, class Width> void DispatchFunction(const Model &model, const Config &config) {
  if (config.query) {
    QueryFromBytes<Model, Width>(model, config);
  } else {
    ConvertToBytes<Model, Width>(model, config.fd_in);
  }
}

template <class Model> void DispatchWidth(const char *file, const Config &config) {
  lm::ngram::Config model_config;
  model_config.load_method = util::READ;
  Model model(file, model_config);
  uint64_t bound = model.GetVocabulary().Bound();
  if (bound <= 256) {
    DispatchFunction<Model, uint8_t>(model, config);
  } else if (bound <= 65536) {
    DispatchFunction<Model, uint16_t>(model, config);
  } else if (bound <= (1ULL << 32)) {
    DispatchFunction<Model, uint32_t>(model, config);
  } else {
    DispatchFunction<Model, uint64_t>(model, config);
  }
}

void Dispatch(const char *file, const Config &config) {
  using namespace lm::ngram;
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(file, model_type)) {
    switch(model_type) {
      case PROBING:
        DispatchWidth<lm::ngram::ProbingModel>(file, config);
        break;
      case REST_PROBING:
        DispatchWidth<lm::ngram::RestProbingModel>(file, config);
        break;
      case TRIE:
        DispatchWidth<lm::ngram::TrieModel>(file, config);
        break;
      case QUANT_TRIE:
        DispatchWidth<lm::ngram::QuantTrieModel>(file, config);
        break;
      case ARRAY_TRIE:
        DispatchWidth<lm::ngram::ArrayTrieModel>(file, config);
        break;
      case QUANT_ARRAY_TRIE:
        DispatchWidth<lm::ngram::QuantArrayTrieModel>(file, config);
        break;
      default:
        UTIL_THROW(util::Exception, "Unrecognized kenlm model type " << model_type);
    }
  } else {
    UTIL_THROW(util::Exception, "Binarize before running benchmarks.");
  }
}

} // namespace

int main(int argc, char *argv[]) {
  try {
    Config config;
    config.fd_in = 0;
    std::string model;

    CLI::App app{"Benchmark options"};
    bool help = false;
    bool vocab_mode = false;
    bool query_mode = false;
    app.add_flag("-h,--help", help, "Show help message");
    app.add_option("-m,--model", model, "Model to query or convert vocab ids")->required();
    app.add_option("-t,--threads", config.threads, "Threads to use (querying only; TODO vocab conversion)")
      ->default_val(std::thread::hardware_concurrency());
    app.add_option("-b,--buffer", config.buf_per_thread, "Number of words to buffer per task.")->default_val(4096);
    app.add_flag("-v,--vocab", vocab_mode, "Convert strings to vocab ids");
    app.add_flag("-q,--query", query_mode, "Query from vocab ids");

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      return app.exit(e);
    }

    if (help) {
      std::cerr << "Benchmark program for KenLM.  Intended usage:\n"
        << "#Convert text to vocabulary ids offline.  These ids are tied to a model.\n"
        << argv[0] << " -v -m $model <$text >$text.vocab\n"
        << "#Ensure files are in RAM.\n"
        << "cat $text.vocab $model >/dev/null\n"
        << "#Timed query against the model.\n"
        << argv[0] << " -q -m $model <$text.vocab\n";
      return 0;
    }

    if (!(vocab_mode ^ query_mode)) {
      std::cerr << "Specify exactly one of -v (vocab conversion) or -q (query)." << std::endl;
      return 0;
    }
    config.query = query_mode;
    if (!config.threads) {
      std::cerr << "Specify a non-zero number of threads with -t." << std::endl;
    }
    Dispatch(model.c_str(), config);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
