#include <catch2/catch.hpp>

#include "fe/pipeline/parser_stage.h"
#include "fe/language_definition.h"
#include "fe/pipeline/pipeline.h"
#include "utils/reading/reader.h"

#include <chrono>

TEST_CASE("the entire language pipeline should be fast enough", "[performance]")
{
	fe::pipeline p;

	SECTION("a first parse")
	{
		auto now = std::chrono::steady_clock::now();
		p.parse({ });
		auto then = std::chrono::steady_clock::now();

		auto time = std::chrono::duration<double, std::milli>(then - now).count();
		std::cout << "Parser construction + empty parse: " << time << " ms" << "\n";

		now = std::chrono::steady_clock::now();
		p.parse({ });
		then = std::chrono::steady_clock::now();

		time = std::chrono::duration<double, std::milli>(then - now).count();
		std::cout << "Empty parse: " << time << " ms" << "\n";

		now = std::chrono::steady_clock::now();
		auto file_or_error = utils::files::read_file("snippets/tests/performance_empty.fe");
		REQUIRE(!std::holds_alternative<std::exception>(file_or_error));
		auto code = std::get<std::string>(file_or_error);
		p.parse(code);
		then = std::chrono::steady_clock::now();

		time = std::chrono::duration<double, std::milli>(then - now).count();
		std::cout << "File parse in: " << time << " ms" << std::endl;
	}

	SECTION("a long file parse")
	{
		// Init parse table
		p.parse({ });

		std::string code = 
R"c(module statements
import [std std.io]

let x : std.i32 = 1;
)c";

		for (int i = 0; i < 10000; i++)
			code += "x = 2;\n";

		auto now = std::chrono::steady_clock::now();
		p.parse(code);
		auto then = std::chrono::steady_clock::now();

		auto time = std::chrono::duration<double, std::milli>(then - now).count();
		std::cout << "Long parse: " << time << " ms" << "\n";
	}
}
