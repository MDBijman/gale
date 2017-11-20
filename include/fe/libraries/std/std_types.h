#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/values.h"
#include "fe/data/types.h"

namespace fe
{
	namespace stdlib
	{
		namespace types
		{
			static std::pair<typecheck_environment, runtime_environment> load()
			{
				typecheck_environment t{};
				runtime_environment r{};

				using namespace fe::types;
				// TODO fix typenames in namespaces
				t.set_type("i32", make_unique(atom_type("std.i32")));
				t.set_type("str", make_unique(atom_type("std.str")));
				t.name = "std";
				r.name = "std";

				using namespace fe::values;

				t.set_type("to_string", unique_type(new function_type(unique_type(new any_type()), unique_type(new atom_type("std.str")))));
				r.set_value("to_string", native_function([](unique_value val) -> unique_value {
					if (auto num = dynamic_cast<integer*>(val.get()))
					{
						return unique_value(new string(std::to_string(num->val)));
					}
					else if (auto str = dynamic_cast<string*>(val.get()))
					{
						return unique_value(new string(str->val));
					}
				}));

				return {t, r};
			}

			static native_module* load_as_module()
			{
				auto [te, re] = load();
				return new native_module("std.types", std::move(re), std::move(te));
			}
		}
	}
}