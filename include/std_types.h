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

				e.set("i32", 
					fe::types::meta_type(), 
					fe::values::type(fe::types::integer_type())
				);

				e.set("str", 
					fe::types::meta_type(), 
					fe::values::type(fe::types::string_type())
				);

				return e;
			}
		}
	}
}