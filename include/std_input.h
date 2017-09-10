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
				te.set_type("get", fe::types::function_type(fe::types::make_unique(fe::types::product_type()), fe::types::make_unique(fe::types::name_type({"std","i32"}))));
				re.set_value("get", values::native_function([](values::value t) -> values::value {
					return values::integer(std::cin.get());
				}));

				return { te, re };
			}
		}
	}
}