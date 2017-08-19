#pragma once
#include "core_ast.h"
#include "values.h"
#include "types.h"
#include "environment.h"

namespace fe
{
	namespace stdlib
	{
		class input_module
		{
		public:
			environment load()
			{
				environment e{};
				e.set_type("get", types::function_type(types::make_unique(types::product_type()), types::make_unique(types::integer_type())));
				e.set_value("get", values::native_function([](values::value t) -> values::value {
					return values::integer(std::cin.get());
				}));

				return e;
			}
		};
	}
}