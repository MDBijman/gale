#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/data/scope.h"
#include <chrono>

namespace fe::stdlib::input
{
	static scope load()
	{
		ext_ast::name_scope se{};
		ext_ast::type_scope te{};
		value_scope re{};

		{
			using namespace values;
			using namespace types;
			se.declare_variable("get");
			se.define_variable("get");
			te.set_type("get", unique_type(
				new function_type(unique_type(new product_type()), unique_type(new types::i32()))));
			re.set_value("get", unique_value(new native_function([](unique_value t) -> unique_value {
				return unique_value(new values::i32(std::cin.get()));
			})));

			se.declare_variable("time");
			se.define_variable("time");
			te.set_type("time", unique_type(new function_type(unique_type(new product_type()), unique_type(new types::i64))));
			re.set_value("time", unique_value(new native_function([](unique_value t) -> unique_value {
				return unique_value(new values::i64(std::chrono::system_clock::now().time_since_epoch().count()));
			})));
		}

		return scope(re, te, se);
	}
}