#pragma once
#include "core_ast.h"
#include "values.h"
#include "types.h"
#include "environment.h"

namespace fe
{
	namespace stdlib
	{
		namespace types
		{
			static environment load()
			{
				environment e{};

				e.set_type("i32", fe::types::meta_type());
				e.set_value("i32", fe::values::type(fe::types::integer_type()));

				e.set_type("str", fe::types::meta_type());
				e.set_value("str", fe::values::type(fe::types::string_type()));

				return e;
			}
		}
	}
}