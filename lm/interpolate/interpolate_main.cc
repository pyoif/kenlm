#include "../common/model_buffer.hh"
#include "pipeline.hh"
#include "tune_instances.hh"
#include "tune_weights.hh"
#include "../../util/fixed_array.hh"
#include "../../util/usage.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas" // Older gcc doesn't have "-Wunused-local-typedefs" and complains.
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <Eigen/Core>
#pragma GCC diagnostic pop

#include <CLI/CLI.hpp>

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
  try {
    Eigen::initParallel();
    lm::interpolate::Config pipe_config;
    lm::interpolate::InstancesConfig instances_config;
    std::vector<std::string> input_models;
    std::string tuning_file;
    std::string ram_str, sort_block_str;
    bool help = false;
    bool just_tune = false;

    CLI::App app{"Log-linear interpolation options"};

    app.add_flag("-h,--help", help, "Show this help message");
    app.add_option("-m,--model", input_models, "Models to interpolate, which must be in KenLM intermediate format.  The intermediate format can be generated using the --intermediate argument to lmplz.")
      ->required()
      ->expected(-1);
    app.add_option("-w,--weight", pipe_config.lambdas, "Interpolation weights")->expected(-1);
    app.add_option("-t,--tuning", tuning_file, "File to tune on: a text file with one sentence per line");
    app.add_flag("--just_tune", just_tune, "Tune and print weights then quit");
    app.add_option("-T,--temp_prefix", pipe_config.sort.temp_prefix, "Temporary file prefix")
      ->default_val("/tmp/lm");
    app.add_option("-S,--memory", ram_str, "Sorting memory: this is a very rough guide")
      ->default_val(util::GuessPhysicalMemory() ? "50%" : "1G");
    app.add_option("--sort_block", sort_block_str, "Block size")->default_val("64M");

    try {
      app.parse(argc, argv);
    } catch (const CLI::ParseError &e) {
      return app.exit(e);
    }

    if (help) {
      std::cerr << "Interpolate multiple models\n" << app.help() << std::endl;
      return 1;
    }

    pipe_config.sort.total_memory = util::ParseSize(ram_str);
    pipe_config.sort.buffer_size = util::ParseSize(sort_block_str);

    instances_config.sort = pipe_config.sort;
    instances_config.model_read_chain_mem = instances_config.sort.buffer_size;
    instances_config.extension_write_chain_mem = instances_config.sort.total_memory;
    instances_config.lazy_memory = instances_config.sort.total_memory;

    if (pipe_config.lambdas.empty() && tuning_file.empty()) {
      std::cerr << "Provide a tuning file with -t xor weights with -w." << std::endl;
      return 1;
    }
    if (!pipe_config.lambdas.empty() && !tuning_file.empty()) {
      std::cerr << "Provide weights xor a tuning file, not both." << std::endl;
      return 1;
    }

    if (!tuning_file.empty()) {
      // Tune weights
      std::vector<StringPiece> model_names;
      for (std::vector<std::string>::const_iterator i = input_models.begin(); i != input_models.end(); ++i) {
        model_names.push_back(*i);
      }
      lm::interpolate::TuneWeights(util::OpenReadOrThrow(tuning_file.c_str()), model_names, instances_config, pipe_config.lambdas);

      std::cerr << "Final weights:";
      std::ostream &to = just_tune ? std::cout : std::cerr;
      for (std::vector<float>::const_iterator i = pipe_config.lambdas.begin(); i != pipe_config.lambdas.end(); ++i) {
        to << ' ' << *i;
      }
      to << std::endl;
    }
    if (just_tune) {
      return 0;
    }

    if (pipe_config.lambdas.size() != input_models.size()) {
      std::cerr << "Number of models (" << input_models.size() << ") should match the number of weights (" << pipe_config.lambdas.size() << ")." << std::endl;
      return 1;
    }

    util::FixedArray<lm::ModelBuffer> models(input_models.size());
    for (std::size_t i = 0; i < input_models.size(); ++i) {
      models.push_back(input_models[i]);
    }
    lm::interpolate::Pipeline(models, pipe_config, 1);
  } catch (const std::exception &e) {
    std::cerr << e.what() <<std::endl;
    return 1;
  }
  return 0;
}
