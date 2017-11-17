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

				using namespace fe::types;
				// TODO fix typenames in namespaces
				e.set_type("i32", make_unique(atom_type("std.i32")));
				e.set_type("str", make_unique(atom_type("std.str")));
				e.name = "std";

				return e;
			}

			static native_module* load_as_module()
			{
				auto te  = load();
				return new native_module("std.types", runtime_environment{}, std::move(te));
			}
		}
	}
}