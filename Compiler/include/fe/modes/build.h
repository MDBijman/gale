#pragma once
#include "fe/data/module.h"
#include "fe/pipeline/pipeline.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace fe
{
	struct build_settings
	{
		build_settings();
		build_settings(bool print_code, bool print_result, bool print_time, bool so);

		build_settings &set_input_files(std::vector<std::string> files);
		build_settings &set_output_file(std::string file);
		build_settings &set_available_modules(std::vector<std::string> modules);
		build_settings &set_main_module(const std::string &module);

		bool has_available_module(const std::string &name) const;

		std::vector<std::string> input_files;
		std::string output_file;

		std::vector<std::string> modules;
		std::string main_module;

		bool print_code, print_result, print_time, should_optimize;
	};

	class builder
	{
	      public:
		builder(build_settings settings);

		// Runs compilation based on the build settings.
		int run();

	      private:
		// Adds the given module to the builder to expose it to programs.
		void add_module(module m);

		// Contains the modules that can be imported by programs.
		std::unordered_map<std::string, module> modules;
		// Contains the build settings that are applied in various stages.
		build_settings settings;
		// The pipeline that exposes the compilation logic.
		fe::pipeline pl;
	};
} // namespace fe