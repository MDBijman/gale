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
					se.declare_variable("get");
					se.define_variable("get");
					te.set_type("get", types::unique_type(new types::function_type(
						types::unique_type(new types::product_type()), types::unique_type(new types::i32())
					)));
					re.set_value("get", values::unique_value(new values::native_function(
						[](values::unique_value t) -> values::unique_value {
							return values::unique_value(new values::i32(std::cin.get()));
						}
					)));
				}

				return scope(re, te, se);
			}
		}
	}
}