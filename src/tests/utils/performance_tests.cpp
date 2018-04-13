#include <catch2/catch.hpp>

#include "fe/pipeline/parser_stage.h"
#include "fe/language_definition.h"
#include "fe/pipeline/pipeline.h"
#include "utils/parsing/bnf_grammar.h"
#include "utils/reading/reader.h"

#include <chrono>

TEST_CASE("the entire language pipeline should be fast enough", "[performance][pipeline]")
{
	fe::pipeline p;

	SECTION("a first parse")
	{
		auto now = std::chrono::steady_clock::now();
		p.parse({ });
		auto then = std::chrono::steady_clock::now();

		auto time = std::chrono::duration<double, std::milli>(then - now).count();
		std::cout << "Parser construction + empty parse: " << time << "\n";

		now = std::chrono::steady_clock::now();
		p.parse({ });
		then = std::chrono::steady_clock::now();

		time = std::chrono::duration<double, std::milli>(then - now).count();
		std::cout << "Empty parse: " << time << "\n";

		SECTION("a file is parsed")
		{
			p.parse({});
			auto before = std::chrono::steady_clock::now();
			auto filename = "snippets/tests/performance_empty.fe";
			auto file_or_error = utils::files::read_file(filename);
			REQUIRE(!std::holds_alternative<std::exception>(file_or_error));
			auto code = std::get<std::string>(file_or_error);
			auto lex_output = p.lex(std::move(code));
			p.parse(std::move(lex_output));

			auto after = std::chrono::steady_clock::now();
			std::cout << "File parse in: " << std::chrono::duration<double, std::milli>(after - before).count() << " ms" << std::endl;
		}
	}
}
