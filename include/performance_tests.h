#pragma once
#include "parser_stage.h"
#include "bnf_grammar.h"
#include "language_definition.h"
#include "reader.h"
#include <chrono>

namespace tests
{
	struct performance_tests
	{
		fe::pipeline p;

		performance_tests()
		{
		}

		void parse_generator()
		{
			auto now = std::chrono::steady_clock::now();
			p.parse({ {fe::terminals::module_keyword, ""}, {fe::terminals::identifier, "module"} });
			auto then = std::chrono::steady_clock::now();

			std::cout << "Initial minimal parse in: ";
			std::cout << std::chrono::duration<double, std::milli>(then - now).count() << " ms" << std::endl;
		}

		void file_parse()
		{
			auto now = std::chrono::steady_clock::now();
			auto filename = "snippets/modeling_module.fe";
			auto file_or_error = tools::files::read_file(filename);
			if (std::holds_alternative<std::exception>(file_or_error))
			{
				std::cout << "Test file not found\n";
				return;
			}
			auto code = std::get<std::string>(file_or_error);
			auto then = std::chrono::steady_clock::now();
			std::cout << "File load: ";
			std::cout << std::chrono::duration<double, std::milli>(then - now).count() << " ms" << std::endl;

			now = std::chrono::steady_clock::now();
			auto lex_output = p.lex(std::move(code));
			then = std::chrono::steady_clock::now();
			std::cout << "File lex: ";
			std::cout << std::chrono::duration<double, std::milli>(then - now).count() << " ms" << std::endl;

			now = std::chrono::steady_clock::now();
			p.parse(std::move(lex_output));
			then = std::chrono::steady_clock::now();
			std::cout << "File parse in: ";
			std::cout << std::chrono::duration<double, std::milli>(then - now).count() << " ms" << std::endl;
		}

		void run_all()
		{
			parse_generator();
			file_parse();
		}
	};
}