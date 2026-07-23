#ifndef LM_FILTER_FORMAT_H
#define LM_FILTER_FORMAT_H

#include "arpa_io.hh"
#include "count_io.hh"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <iosfwd>

namespace lm {

template <class Single> class MultipleOutput {
  private:
    typedef std::vector<std::unique_ptr<Single>> Singles;
    typedef typename Singles::iterator SinglesIterator;

  public:
    MultipleOutput(const char *prefix, size_t number) {
      files_.reserve(number);
      std::string tmp;
      for (unsigned int i = 0; i < number; ++i) {
        tmp = prefix;
        tmp += std::to_string(i);
        files_.push_back(std::unique_ptr<Single>(new Single(tmp.c_str())));
      }
    }

    void AddNGram(const StringPiece &line) {
      for (auto i = files_.begin(); i != files_.end(); ++i)
        (*i)->AddNGram(line);
    }

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const StringPiece &line) {
      for (auto i = files_.begin(); i != files_.end(); ++i)
        (*i)->AddNGram(begin, end, line);
    }

    void SingleAddNGram(size_t offset, const StringPiece &line) {
      files_[offset]->AddNGram(line);
    }

    template <class Iterator> void SingleAddNGram(size_t offset, const Iterator &begin, const Iterator &end, const StringPiece &line) {
      files_[offset]->AddNGram(begin, end, line);
    }

  protected:
    Singles files_;
};

class MultipleARPAOutput : public MultipleOutput<ARPAOutput> {
  public:
    MultipleARPAOutput(const char *prefix, size_t number) : MultipleOutput<ARPAOutput>(prefix, number) {}

    void ReserveForCounts(std::streampos reserve) {
      for (auto i = files_.begin(); i != files_.end(); ++i)
        (*i)->ReserveForCounts(reserve);
    }

    void BeginLength(unsigned int length) {
      for (auto i = files_.begin(); i != files_.end(); ++i)
        (*i)->BeginLength(length);
    }

    void EndLength(unsigned int length) {
      for (auto i = files_.begin(); i != files_.end(); ++i)
        (*i)->EndLength(length);
    }

    void Finish() {
      for (auto i = files_.begin(); i != files_.end(); ++i)
        (*i)->Finish();
    }
};

template <class Filter, class Output> class DispatchInput {
  public:
    DispatchInput(Filter &filter, Output &output) : filter_(filter), output_(output) {}

/*    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const StringPiece &line) {
      filter_.AddNGram(begin, end, line, output_);
    }*/

    void AddNGram(const StringPiece &ngram, const StringPiece &line) {
      filter_.AddNGram(ngram, line, output_);
    }

  protected:
    Filter &filter_;
    Output &output_;
};

template <class Filter, class Output> class DispatchARPAInput : public DispatchInput<Filter, Output> {
  private:
    typedef DispatchInput<Filter, Output> B;

  public:
    DispatchARPAInput(Filter &filter, Output &output) : B(filter, output) {}

    void ReserveForCounts(std::streampos reserve) { B::output_.ReserveForCounts(reserve); }
    void BeginLength(unsigned int length) { B::output_.BeginLength(length); }

    void EndLength(unsigned int length) {
      B::filter_.Flush();
      B::output_.EndLength(length);
    }
    void Finish() { B::output_.Finish(); }
};

struct ARPAFormat {
  typedef ARPAOutput Output;
  typedef MultipleARPAOutput Multiple;
  static void Copy(util::FilePiece &in, Output &out) {
    ReadARPA(in, out);
  }
  template <class Filter, class Out> static void RunFilter(util::FilePiece &in, Filter &filter, Out &output) {
    DispatchARPAInput<Filter, Out> dispatcher(filter, output);
    ReadARPA(in, dispatcher);
  }
};

struct CountFormat {
  typedef CountOutput Output;
  typedef MultipleOutput<Output> Multiple;
  static void Copy(util::FilePiece &in, Output &out) {
    ReadCount(in, out);
  }
  template <class Filter, class Out> static void RunFilter(util::FilePiece &in, Filter &filter, Out &output) {
    DispatchInput<Filter, Out> dispatcher(filter, output);
    ReadCount(in, dispatcher);
  }
};

/* For multithreading, the buffer classes hold batches of filter inputs and
 * outputs in memory.  The strings get reused a lot, so keep them around
 * instead of clearing each time.
 */
class InputBuffer {
  public:
    InputBuffer() : actual_(0) {}

    void Reserve(size_t size) { lines_.reserve(size); }

    template <class Output> void AddNGram(const StringPiece &ngram, const StringPiece &line, Output &output) {
      if (lines_.size() == actual_) lines_.resize(lines_.size() + 1);
      // TODO avoid this copy.
      std::string &copied = lines_[actual_].line;
      copied.assign(line.data(), line.size());
      lines_[actual_].ngram.set(copied.data() + (ngram.data() - line.data()), ngram.size());
      ++actual_;
    }

    template <class Filter, class Output> void CallFilter(Filter &filter, Output &output) const {
      for (std::vector<Line>::const_iterator i = lines_.begin(); i != lines_.begin() + actual_; ++i) {
        filter.AddNGram(i->ngram, i->line, output);
      }
    }

    void Clear() { actual_ = 0; }
    bool Empty() { return actual_ == 0; }
    size_t Size() { return actual_; }

  private:
    struct Line {
      std::string line;
      StringPiece ngram;
    };

    size_t actual_;

    std::vector<Line> lines_;
};

class BinaryOutputBuffer {
  public:
    BinaryOutputBuffer() {}

    void Reserve(size_t size) {
      lines_.reserve(size);
    }

    void AddNGram(const StringPiece &line) {
      lines_.push_back(line);
    }

    template <class Output> void Flush(Output &output) {
      for (std::vector<StringPiece>::const_iterator i = lines_.begin(); i != lines_.end(); ++i) {
        output.AddNGram(*i);
      }
      lines_.clear();
    }

  private:
    std::vector<StringPiece> lines_;
};

class MultipleOutputBuffer {
  public:
    MultipleOutputBuffer() : last_(NULL) {}

    void Reserve(size_t size) {
      annotated_.reserve(size);
    }

    void AddNGram(const StringPiece &line) {
      annotated_.resize(annotated_.size() + 1);
      annotated_.back().line = line;
    }

    void SingleAddNGram(size_t offset, const StringPiece &line) {
      if ((line.data() == last_.data()) && (line.length() == last_.length())) {
        annotated_.back().systems.push_back(offset);
      } else {
        annotated_.resize(annotated_.size() + 1);
        annotated_.back().systems.push_back(offset);
        annotated_.back().line = line;
        last_ = line;
      }
    }

    template <class Output> void Flush(Output &output) {
      for (std::vector<Annotated>::const_iterator i = annotated_.begin(); i != annotated_.end(); ++i) {
        if (i->systems.empty()) {
          output.AddNGram(i->line);
        } else {
          for (std::vector<size_t>::const_iterator j = i->systems.begin(); j != i->systems.end(); ++j) {
            output.SingleAddNGram(*j, i->line);
          }
        }
      }
      annotated_.clear();
    }

  private:
    struct Annotated {
      // If this is empty, send to all systems.
      // A filter should never send to all systems and send to a single one.
      std::vector<size_t> systems;
      StringPiece line;
    };

    StringPiece last_;

    std::vector<Annotated> annotated_;
};

} // namespace lm

#endif // LM_FILTER_FORMAT_H
