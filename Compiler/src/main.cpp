#include "fe/pipeline/pipeline.h"
#include "fe/modes/project.h"
#include "fe/modes/repl.h"
#include "utils/memory/small_vector.h"

#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

#include <iostream>
#include <filesystem>

int main(int argc, char** argv)
{
	auto mode = argc > 1 ? std::string(argv[1]) : "help";

	auto possible_modes = { "test", "project", "help", "repl", "exit", "other" };

	if (argc == 2 && (std::find(possible_modes.begin(), possible_modes.end(), mode) == possible_modes.end()))
	{
		std::cout << "Unknown commandline argument: " << mode << std::endl;
		std::cin.get();
		return -1;
	}

	auto pipeline = fe::pipeline();

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
		try
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

			fe::project proj(std::move(pipeline));
			// core
			proj.add_module(fe::core::operations::load());
			// std io
			proj.add_module(fe::stdlib::io::load());
			// std ui
			proj.add_module(fe::stdlib::ui::load());
			// std types
			proj.add_module(fe::stdlib::typedefs::load());

			auto project_path = std::experimental::filesystem::path(argv[2]);
			std::cout << "Project folder: " << project_path << "\n";

			auto directory_it = std::experimental::filesystem::recursive_directory_iterator(argv[2]);
			for (auto& item : directory_it)
			{
				auto path = item.path();
				if (path.filename().extension() != ".fe") continue;

				std::cout << "\nInterpreting file " << path.filename() << "\n";

				auto file_or_error = utils::files::read_file(path.string());
				if (std::holds_alternative<std::exception>(file_or_error))
				{
					std::cout << "File not found\n";
					continue;
				}
				auto& code = std::get<std::string>(file_or_error);
				proj.add_module(proj.compile(code));
			}
		}
		catch (const lexing::error& e)
		{
			std::cout << "Lexing error:\n" << e.message << "\n";
		}
		catch (const fe::parse_error& e)
		{
			std::cout << "Parse error:\n" << e.message << "\n";
		}
		catch (const fe::typecheck_error& e)
		{
			std::cout << "Typechecking error:\n" << e.message << "\n";
		}
		catch (const fe::lower_error& e)
		{
			std::cout << "Lowering error:\n" << e.message << "\n";
		}
		catch (const fe::interp_error& e)
		{
			std::cout << "Interp error:\n" << e.message << "\n";
		}
		catch (const fe::resolution_error& e)
		{
			std::cout << "Resolution error:\n" << e.message << "\n";
		}
		catch (const fe::type_env_error& e)
		{
			std::cout << e.message << "\n" << std::endl;
		}
		catch (const fe::other_error& e)
		{
			std::cout << e.message << "\n" << std::endl;
		}
		catch (const std::runtime_error& e)
		{
			std::cout << e.what() << std::endl;
		}
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
	else if (mode == "other")
	{
		fe::pipeline p;
		//fe::lexing_stage lexer;

		// Init parse table
		std::string code =
			R"c(module statements
	import [std std.io]

	let x : std.i32 = 1;
	)c";

		for (int i = 0; i < 1000000; i++)
			code += "x = 2;\n";

		while (true)
		{
			auto now = std::chrono::steady_clock::now();
			p.parse(code);
			auto then = std::chrono::steady_clock::now();

			auto time = std::chrono::duration<double, std::milli>(then - now).count();
			std::cout << "Long parse: " << time << " ms" << "\n";
		}

		std::cin.get();
		return 0;
	}

	std::cin.get();
	return 0;
}