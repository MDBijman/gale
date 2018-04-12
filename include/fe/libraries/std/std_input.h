#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/data/scope.h"

namespace fe
{
	namespace stdlib
	{
		namespace input
		{
			static scope load()
			{
				runtime_environment re{};
				re.push();
				ext_ast::name_scope se{};
				ext_ast::type_scope te{};

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
				}

				return scope(re, te, se);
			}
		}
	}
}