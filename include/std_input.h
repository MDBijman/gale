#pragma once
#include "core_ast.h"
#include "values.h"
#include "types.h"
#include "environment.h"

namespace fe
{
	namespace stdlib
	{
		namespace input
		{
			static environment load()
			{
				environment e{};
				e.set(
					"get",
					fe::types::function_type(fe::types::make_unique(fe::types::product_type()), fe::types::make_unique(fe::types::integer_type())),
					values::native_function([](values::value t) -> values::value {
						return values::integer(std::cin.get());
					}
				));

				return e;
			}
		}
	}
}