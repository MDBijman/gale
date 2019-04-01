#include "fe/modes/build.h"

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>

#include <iostream>
#include <vector>

// #todo Currently unused until unit testing strategy is devised
int on_test(int argc, char **argv)
{
	std::vector<char *> commands;
	commands.push_back(argv[0]);
	for (int i = 2; i < argc; i++) commands.push_back(argv[i]);

	return Catch::Session().run(argc - 1, commands.data());
}

int on_build(const std::vector<std::string> &args)
{
	std::vector<std::string> input_files;
	auto begin_input_files = std::find(args.begin(), args.end(), "-i");
	auto end_input_files =
	  std::find_if(begin_input_files + 1, args.end(), [](auto &s) { return s[0] == '-'; });

	if (
	  // The -i flag must be present
	  begin_input_files == args.end()
	  // The flag cannot be at the end
	  || begin_input_files + 1 == args.end()
	  // There must be at least one file name
	  || end_input_files - begin_input_files == 1)
	{
		std::cerr << "Expected input files\n";
		return 1;
	}

	// We searched for '-i' so the actual first file is one further
	begin_input_files++;

	// Gather list of input files
	while (begin_input_files != end_input_files)
	{
		input_files.push_back(*begin_input_files);
		begin_input_files++;
	}

	auto begin_output_file = std::find(args.begin(), args.end(), "-o");
	if (
	  // The -o flag must be present
	  begin_output_file == args.end()
	  // There must be a file name
	  || begin_output_file + 1 == args.end()
	  // The next string cannot be a flag
	  || ((*(begin_output_file + 1))[0] == '-'))
	{
		std::cerr << "Expected output file name\n";
		return 1;
	}

	const std::string &output_file = *(begin_output_file + 1);

	fe::build_settings settings(false, false, false, false);
	settings.set_input_files(input_files)
	  .set_output_file(output_file)
	  .set_available_modules({ "std.io", "std" });

	return fe::builder(settings).run();
}

int on_help()
{
	std::cout << "The Gale toolset v0.0.1\n"
		  << "Commands:\n"
		  << "gale build -f <files...> -o <exec_name>\n"
		  << "\tProcesses each of the files to build a single bytecode executable\n"
		  << std::endl;
	return 0;
}

int main(int argc, char **argv)
{
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++) { args.push_back(std::string(argv[i])); }

	const std::string &mode = args.empty() ? "help" : args[0];
	auto possible_modes = { "build", "help" };

	if (std::find(possible_modes.begin(), possible_modes.end(), mode) == possible_modes.end())
	{
		std::cout << "Unknown commandline argument: " << mode << std::endl;
		return 1;
	}

	if (mode == "build")
		return on_build(args);
	else if (mode == "help")
		return on_help();
	else
		return on_help();
}