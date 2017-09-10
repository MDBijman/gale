#pragma once
#include "core_ast.h"
#include "values.h"
#include "types.h"

namespace fe
{
	namespace stdlib
	{
		namespace types
		{
			static typecheck_environment load()
			{
				typecheck_environment e{};

				e.set_type("i32", fe::types::void_type());
				e.set_type("str", fe::types::void_type());
				e.name = "std";

				return e;
			}
		}
	}
}