#pragma once
#include "fe/pipeline/parser_stage.h"
#include "fe/language_definition.h"
#include "utils/parsing/bnf_grammar.h"
#include "utils/reading/reader.h"

#include <catch2/catch.hpp>
#include <chrono>

SCENARIO("the entire language pipeline should be fast enough", "[performance pipeline]")
{
	GIVEN("a new pipeline")
	{
		fe::pipeline p;

		WHEN("the first parse is performed")
		{
			auto now = std::chrono::steady_clock::now();
			p.parse({ });
			auto then = std::chrono::steady_clock::now();

			THEN("this parse (including table generation) should take less than 300ms")
			{
				auto time = std::chrono::duration<double, std::milli>(then - now).count();
				REQUIRE(time < 300);
				std::cout << time << "\n";

				AND_WHEN("a subsequent empty parse is performed")
				{
					now = std::chrono::steady_clock::now();
					p.parse({ });
					then = std::chrono::steady_clock::now();

					THEN("the parse should take less than a tenth of a ms")
					{
						time = std::chrono::duration<double, std::milli>(then - now).count();
						std::cout << time << "\n";
						REQUIRE(time < 0.1);
					}
				}
			}
		}

		WHEN("a file is parsed")
		{
			p.parse({});
			auto before = std::chrono::steady_clock::now();
			auto filename = "snippets/tests/performance_empty.fe";
			auto file_or_error = utils::files::read_file(filename);
			if (std::holds_alternative<std::exception>(file_or_error))
			{
				std::cout << "Test file not found\n";
				return;
			}
			auto code = std::get<std::string>(file_or_error);
			auto lex_output = p.lex(std::move(code));
			p.parse(std::move(lex_output));

			auto after = std::chrono::steady_clock::now();
			std::cout << "File parse in: " << std::chrono::duration<double, std::milli>(after - before).count() << " ms" << std::endl;
		}
	}
}
