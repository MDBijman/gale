#pragma once
#include "core_ast.h"
#include "values.h"
#include "types.h"
#include "runtime_environment.h"
#include "typecheck_environment.h"

namespace fe
{
	namespace stdlib
	{
		namespace input
		{
			static std::tuple<typecheck_environment, runtime_environment> load()
			{
				typecheck_environment te{};
				runtime_environment re{};

				{
					using namespace fe::types;
					using namespace fe::values;
					te.set_type("get", unique_type(new function_type(
						unique_type(new product_type()), unique_type(new atom_type("i32"))
					)));
					re.set_value("get", unique_value(new native_function(
						[](unique_value t) -> unique_value {
							return unique_value(new integer(std::cin.get()));
						}
					)));
				}

				return { te, re };
			}
		}
	}
}