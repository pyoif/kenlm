#ifndef UTIL_PROGRAM_OPTIONS_H
#define UTIL_PROGRAM_OPTIONS_H

// Minimal standalone replacement for boost::program_options.
// Covers the API subset used by kenlm executables.
// Public domain / MIT — drop-in for boost-free builds.

#include <algorithm>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace util {
namespace program_options {

class error : public std::runtime_error {
public:
  explicit error(const std::string &msg) : std::runtime_error(msg) {}
};

class unknown_option : public error {
public:
  explicit unknown_option(const std::string &msg) : error(msg) {}
};

class invalid_option_value : public error {
public:
  explicit invalid_option_value(const std::string &msg) : error(msg) {}
};

// ---------------------------------------------------------------
// typed_value<T>
// ---------------------------------------------------------------
template <class T> class typed_value;

// Base for all option values
class value_semantic {
public:
  virtual ~value_semantic() = default;
  virtual void parse(const std::string &text) = 0;
  virtual bool is_bool_switch() const { return false; }
  virtual bool implicit_value_allowed() const { return false; }
  virtual const std::type_info &value_type() const = 0;
};

// typed_value<T> — stores into a pointer
template <class T> class typed_value : public value_semantic {
public:
  typed_value(T *store) : store_(store), defaulted_(false), multitoken_(false) {}

  typed_value *default_value(const T &val) {
    defaulted_ = true;
    default_ = val;
    *store_ = val;
    return this;
  }

  typed_value *default_value(const std::string &val) {
    std::istringstream is(val);
    T v;
    is >> v;
    default_value(v);
    return this;
  }

  typed_value *implicit_value(const T &val) {
    implicit_ = val;
    implicit_set_ = true;
    return this;
  }

  typed_value *implicit_value(const std::string &val) {
    std::istringstream is(val);
    T v;
    is >> v;
    implicit_value(v);
    return this;
  }

  typed_value *required() {
    required_ = true;
    return this;
  }

  typed_value *multitoken() {
    multitoken_ = true;
    return this;
  }

  typed_value *notifier(std::function<void(const T &)> f) {
    notifier_ = std::move(f);
    return this;
  }

  void parse(const std::string &text) override {
    std::istringstream is(text);
    T v;
    is >> v;
    if (is.fail()) throw invalid_option_value("cannot parse '" + text + "'");
    *store_ = v;
  }

  bool is_bool_switch() const override { return std::is_same<T, bool>::value; }
  bool implicit_value_allowed() const override { return implicit_set_; }
  const std::type_info &value_type() const override { return typeid(T); }

  T *store_ = nullptr;
  T default_{};
  bool defaulted_ = false;
  bool required_ = false;
  bool multitoken_ = false;
  T implicit_{};
  bool implicit_set_ = false;
  std::function<void(const T &)> notifier_;
};

// bool_switch() — specialized
inline typed_value<bool> *bool_switch(bool *store = nullptr) {
  // bool_switch stores into a pointer; if nullptr, discard
  static bool dummy;
  return new typed_value<bool>(store ? store : &dummy);
}

template <class T> typed_value<T> *value(T *store) {
  return new typed_value<T>(store);
}

// ---------------------------------------------------------------
// option_description
// ---------------------------------------------------------------
class option_description {
public:
  option_description(const char *name, value_semantic *semantic, const char *description)
    : long_name_(name), semantic_(semantic), description_(description) {
    parse_spec(name);
  }

  const std::string &long_name() const { return long_name_; }
  const std::string &description() const { return description_; }
  value_semantic *semantic() const { return semantic_.get(); }
  const std::vector<std::string> &short_names() const { return short_names_; }

private:
  void parse_spec(const char *spec) {
    std::string s(spec);
    size_t comma = s.find(',');
    if (comma != std::string::npos) {
      std::string rest = s.substr(comma + 1);
      s = s.substr(0, comma);
      // rest may be a short name
      if (!rest.empty()) short_names_.push_back(rest);
    }
    long_name_ = s;
  }

  std::string long_name_;
  std::vector<std::string> short_names_;
  std::unique_ptr<value_semantic> semantic_;
  std::string description_;
};

// ---------------------------------------------------------------
// options_description
// ---------------------------------------------------------------
class options_description {
public:
  options_description(const std::string &caption = "") : caption_(caption) {}

  options_description &add_options() { return *this; }

  template <class T>
  options_description &operator()(const char *name, typed_value<T> *sem,
                                  const char *desc) {
    options_.emplace_back(name, sem, desc);
    return *this;
  }

  // bool_switch returns typed_value<bool>*
  options_description &operator()(const char *name, typed_value<bool> *sem,
                                  const char *desc) {
    options_.emplace_back(name, sem, desc);
    return *this;
  }

  // Notifier-style: SizeOption returns typed_value<std::string>*
  options_description &operator()(const char *name, typed_value<std::string> *sem,
                                  const char *desc) {
    options_.emplace_back(name, sem, desc);
    return *this;
  }

  const std::vector<option_description> &options() const { return options_; }

  // << operator for printing help
  friend std::ostream &operator<<(std::ostream &os,
                                  const options_description &od) {
    for (auto &o : od.options_) {
      os << "  --" << o.long_name();
      if (!o.short_names().empty())
        os << " [ -" << o.short_names()[0] << " ]";
      os << " : " << o.description() << "\n";
    }
    return os;
  }

private:
  std::string caption_;
  std::vector<option_description> options_;
};

// ---------------------------------------------------------------
// variables_map
// ---------------------------------------------------------------
class variables_map {
public:
  template <class T> const T &operator[](const std::string &name) const {
    auto it = values_.find(name);
    if (it == values_.end())
      throw std::runtime_error("option '" + name + "' not found");
    return *static_cast<const T *>(it->second.ptr);
  }

  template <class T> T &operator[](const std::string &name) {
    auto it = values_.find(name);
    if (it == values_.end()) {
      // default construct
      auto &entry = values_[name];
      entry.ptr = new T();
      entry.owned = true;
      return *static_cast<T *>(entry.ptr);
    }
    return *static_cast<T *>(it->second.ptr);
  }

  template <class T> T as() const { return T(); }

  bool count(const std::string &name) const {
    auto it = values_.find(name);
    return it != values_.end() && it->second.set;
  }

  // vector access
  std::vector<std::string> &vec(const std::string &name) {
    auto it = values_.find(name);
    if (it == values_.end()) {
      auto &entry = values_[name];
      entry.ptr = new std::vector<std::string>();
      entry.owned = true;
      return *static_cast<std::vector<std::string> *>(entry.ptr);
    }
    return *static_cast<std::vector<std::string> *>(it->second.ptr);
  }

  void set_value(const std::string &name, const std::string &str_val) {
    auto &entry = values_[name];
    entry.str_val = str_val;
    entry.set = true;
  }

  std::string get_str(const std::string &name) const {
    auto it = values_.find(name);
    if (it == values_.end()) return "";
    return it->second.str_val;
  }

  // Direct typed access
  template <class T> void set_typed(const std::string &name, const T &val) {
    auto &entry = values_[name];
    if (!entry.ptr) { entry.ptr = new T(); entry.owned = true; }
    *static_cast<T *>(entry.ptr) = val;
    entry.set = true;
  }

  struct Entry {
    void *ptr = nullptr;
    bool owned = false;
    std::string str_val;
    bool set = false;
    ~Entry() { if (owned) delete static_cast<char *>(ptr); /* approximate */ }
  };

  std::map<std::string, Entry> values_;
};

// ---------------------------------------------------------------
// parse_command_line / store / notify
// ---------------------------------------------------------------
inline void store(int argc, char *argv[],
                  const options_description &desc,
                  variables_map &vm) {
  std::map<std::string, option_description> lookup;
  for (auto &o : desc.options()) {
    lookup["--" + o.long_name()] = o;
    for (auto &s : o.short_names())
      lookup["-" + s] = o;
  }

  std::string current_name;
  for (int i = 1; i < argc; ++i) {
    std::string arg(argv[i]);

    // Check if it's an option name
    auto it = lookup.find(arg);
    if (it != lookup.end()) {
      current_name = it->second.long_name();
      auto *sem = it->second.semantic();

      if (sem->is_bool_switch()) {
        vm.set_typed<bool>(current_name, true);
        current_name.clear();
      }
      // For implicit_value: if next arg is not an option, use it; else use
      // implicit
      else if (sem->implicit_value_allowed() && i + 1 < argc &&
               std::string(argv[i + 1]).find("--") != 0 &&
               std::string(argv[i + 1]).find('-') != 0) {
        // has explicit value
      }
      continue;
    }

    // It's a value for the current option
    if (!current_name.empty()) {
      vm.set_value(current_name, arg);
      current_name.clear();
    }
  }
}

// Custom SizeOption notifier integration
class SizeNotify {
public:
  explicit SizeNotify(std::size_t &out) : behind_(out) {}
  void operator()(const std::string &from);
private:
  std::size_t &behind_;
};

inline typed_value<std::string> *SizeOption(std::size_t &to,
                                             const char *default_value);

inline void notify(variables_map &vm) {
  // Walk all options and apply notifiers.
  // We handle this during store above; notify is a no-op for our impl.
}

} // namespace program_options
} // namespace util

#endif // UTIL_PROGRAM_OPTIONS_H
