#pragma once
#include "parser_stage.h"
#include "bnf_grammar.h"
#include "language_definition.h"
#include <chrono>

namespace tests
{
	struct performance_tests
	{
		performance_tests()
		{

		}

		void run_all()
		{
			fe::parsing_stage p;
			auto now = std::chrono::steady_clock::now();
			p.parse({ {fe::terminals::module_keyword, ""}, {fe::terminals::identifier, "module"} });
			auto then = std::chrono::steady_clock::now();

			std::cout << "Parser construction in: ";
			std::cout << std::chrono::duration<double, std::milli>(then - now).count() << " ms" << std::endl;
		}
	};
}