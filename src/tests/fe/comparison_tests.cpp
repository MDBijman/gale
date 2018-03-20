#include <catch2/catch.hpp>

#include "fe/pipeline/pipeline.h"
#include "fe/data/type_environment.h"
#include "fe/data/scope_environment.h"
#include "fe/data/runtime_environment.h"
#include "fe/language_definition.h"
#include "fe/libraries/std/std_types.h"
#include "fe/libraries/core/core_operations.h"

SCENARIO("comparison operators", "[language_feature comparison operators]")
{
	GIVEN("a new pipeline")
	{
		fe::pipeline p;

		WHEN("an equals comparison is interpreted for a true expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 3;
var y : std.i32 = 0;

if (x == 3) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 1);
			}
		}

		WHEN("an equals comparison is interpreted for a false expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 3;
var y : std.i32 = 0;

if (x == 4) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should not be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 0);
			}
		}

		WHEN("a greater than comparison is interpreted for a true expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x > 4) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 1);
			}
		}

		WHEN("a greater than comparison is interpreted for a false expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x > 6) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should not be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 0);
			}
		}

		WHEN("a greater than or eq comparison is interpreted for a true expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x >= 4) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 1);
			}
		}

		WHEN("a greater than or eq comparison is interpreted for a false expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x >= 6) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should not be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 0);
			}
		}

		WHEN("a smaller than comparison is interpreted for a true expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x < 6) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 1);
			}
		}

		WHEN("a smaller than comparison is interpreted for a false expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x < 4) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should not be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 0);
			}
		}

		WHEN("a smaller than or eq comparison is interpreted for a true expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x <= 6) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 1);
			}
		}

		WHEN("a smaller than or eq comparison is interpreted for a false expression")
		{
			auto code =
				R"code(
import [std std.io]

var x : std.i32 = 5;
var y : std.i32 = 0;

if (x <= 4) {
	y = 1;
};
)code";
			auto lexed = p.lex(std::move(code));
			auto parsed = p.parse(std::move(lexed));

			fe::code_module cm(fe::module_name{ "" }, std::move(parsed));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::stdlib::types::load_as_module()));
			cm.imports.push_back(std::shared_ptr<fe::native_module>(fe::core::operations::load_as_module()));

			auto[te, re, se] = cm.interp(p);

			THEN("the body should not be executed")
			{
				auto res = re.valueof(fe::core_ast::identifier({}, "y", {}, 0, nullptr));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value()));
				REQUIRE(dynamic_cast<fe::values::integer*>(res.value())->val == 0);
			}
		}
	}
}