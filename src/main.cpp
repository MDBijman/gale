#include "fe/pipeline/pipeline.h"
#include "fe/modes/project.h"
#include "fe/modes/repl.h"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <iostream>
#include <filesystem>

int main(int argc, char** argv)
{
	auto mode = argc > 1 ? std::string(argv[1]) : "help";

	auto possible_modes = { "test", "project", "help", "repl" };

	if (argc == 2 && (std::find(possible_modes.begin(), possible_modes.end(), mode) == possible_modes.end()))
	{
		std::cout << "Unknown commandline argument: " << mode << std::endl;
		std::cin.get();
		return -1;
	}

	auto pipeline = fe::pipeline();

	try
	{
		if (mode == "repl")
		{
			auto repl = fe::repl(std::move(pipeline));
			repl.run();
			return 0;
		}
		else if (mode == "test")
		{
			std::vector<char*> commands;
			commands.push_back(argv[0]);
			for (int i = 2; i < argc; i++)
				commands.push_back(argv[i]);

			return Catch::Session().run(argc - 1, commands.data());
		}
		else if (mode == "project")
		{
			if (argc == 2)
			{
				std::cout << "Missing project location" << std::endl;
				std::cin.get();
				return -1;
			}
			else if (argc == 3)
			{
				std::cout << "Missing main file name" << std::endl;
				std::cin.get();
				return -1;
			}

			fe::project proj(argv[2], argv[3], std::move(pipeline));
			auto[te, re, se] = proj.interp();
			std::cout << te.to_string() << "\n";
			std::cout << re.to_string() << std::endl;
		}
		else if (mode == "help")
		{
			std::cout
				<< "The {language} toolset v0.0.1\n"
				<< "Commands:\n"
				<< "{language} project {project folder} {main module}\n"
				<< "\tInterprets each module in the project folder, taking the main module as root\n"
				<< "{language} test\n"
				<< "\tRuns the toolset tests\n"
				<< "{language} repl\n"
				<< "\tStarts a REPL session\n"
				<< std::endl;
			std::cin.get();
			return 0;
		}
	}
	catch (const utils::lexing::error& e)
	{
		std::cout << "Lexing error:\n" << e.message;
	}
	catch (const fe::lex_to_parse_error& e)
	{
		std::cout << "Lex to parse error:\n" << e.message;
	}
	catch (const utils::ebnfe::error& e)
	{
		std::cout << "Parse error:\n" << e.message;
	}
	catch (const fe::cst_to_ast_error& e)
	{
		std::cout << "CST to AST conversion error:\n" << e.message;
	}
	catch (const fe::typecheck_error& e)
	{
		std::cout << "Typechecking error:\n" << e.message;
	}
	catch (const fe::lower_error& e)
	{
		std::cout << "Lowering error:\n" << e.message;
	}
	catch (const fe::interp_error& e)
	{
		std::cout << "Interp error:\n" << e.message;
	}
	catch (const fe::type_env_error& e)
	{
		std::cout << e.message << std::endl;
	}
	catch (const std::runtime_error& e)
	{
		std::cout << e.what() << std::endl;
	}
	std::cin.get();
	return 0;
}