#include <catch2/catch.hpp>

#include "fe/pipeline/pipeline.h"
#include "fe/data/type_environment.h"
#include "fe/data/scope_environment.h"
#include "fe/data/runtime_environment.h"
#include "fe/language_definition.h"
#include "fe/libraries/std/std_types.h"
#include "fe/libraries/core/core_operations.h"

SCENARIO("while loop", "[language_feature while]")
{
	GIVEN("a new pipeline")
	{
		fe::pipeline p;

		WHEN("while loop is interpreted")
		{
			auto code =
R"code(
import [std std.io]

var x : std.i32 = 3;

while (x == 3) {
	x = x - 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::typedefs::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the variable values should be correct")
			{
				auto valueof_x = re.valueof(fe::core_ast::identifier({}, "x", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::i32*>(valueof_x.value()));
				REQUIRE(dynamic_cast<fe::values::i32*>(valueof_x.value())->val == 2);
			}
		}
	}
}