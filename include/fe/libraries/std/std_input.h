#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/data/runtime_environment.h"
#include "fe/data/type_environment.h"

namespace fe
{
	namespace stdlib
	{
		namespace input
		{
			static std::tuple<type_environment, runtime_environment, scope_environment> load()
			{
				type_environment te{};
				runtime_environment re{};
				scope_environment se{};

				{
					using namespace fe::types;
					using namespace fe::values;
					se.declare(extended_ast::identifier({ "get" }), extended_ast::identifier({ "_function" }));
					se.define(extended_ast::identifier({ "get" }));
					te.set_type(extended_ast::identifier({ "get" }), unique_type(new function_type(
						unique_type(new product_type()), unique_type(new atom_type("std.i32"))
					)));
					re.set_value("get", unique_value(new native_function(
						[](unique_value t) -> unique_value {
							return unique_value(new integer(std::cin.get()));
						}
					)));
				}

				return { te, re, se };
			}
		}
	}
}