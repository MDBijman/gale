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
					se.declare(extended_ast::identifier("get"), extended_ast::identifier("_function"));
					se.define(extended_ast::identifier("get"));
					te.set_type(extended_ast::identifier("get"), types::unique_type(new types::function_type(
						types::unique_type(new types::product_type()), types::unique_type(new types::i32())
					)));
					re.set_value("get", values::unique_value(new values::native_function(
						[](values::unique_value t) -> values::unique_value {
							return values::unique_value(new values::i32(std::cin.get()));
						}
					)));
				}

				return { te, re, se };
			}
		}
	}
}