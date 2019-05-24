#include "fe/modes/build.h"
#include "utils/argparsing/argument_parser.h"

#define CATCH_CONFIG_RUNNER
#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>

#include <iostream>
#include <vector>

// #todo Currently unused until unit testing strategy is devised
// End-to-end tests are run via Tests/main.py
int on_test(int argc, char **argv)
{
	std::vector<char *> commands;
	commands.push_back(argv[0]);
	for (int i = 2; i < argc; i++) commands.push_back(argv[i]);

	return Catch::Session().run(argc - 1, commands.data());
}

int on_build(const std::vector<std::string> &args)
{
	std::vector<std::string> input_files = parse_list_option(args, "i");
	std::string output_file = parse_atom_option(args, "o");
	std::string main_module = parse_atom_option(args, "e");

	auto settings = fe::build_settings(false, false, false, false);
	settings.set_input_files(input_files)
	  .set_output_file(output_file)
	  .set_available_modules({ "std.io", "std" })
	  .set_main_module(main_module);

	return fe::builder(settings).run();
}

int on_help()
{
	std::cout << "The Gale toolset v0.0.1\n"
		  << "Commands:\n"
		  << "gale build -i <files...> -e <main_module> -o <out_name>\n"
		  << "\tProcesses each of the files to build a single bytecode executable\n"
		  << std::endl;
	return 0;
}

int main(int argc, char **argv)
{
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++)
	{
		args.push_back(std::string(argv[i]));
	}

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