#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/modes/module.h"

namespace fe
{
	namespace stdlib
	{
		namespace types
		{
			static std::tuple<type_environment, runtime_environment, scope_environment> load()
			{
				type_environment t{};
				runtime_environment r{};
				scope_environment s{};

				using namespace fe::types;
				// TODO fix typenames in namespaces
				s.define_type(extended_ast::identifier({ "i32" }), nested_type());
				t.define_type(extended_ast::identifier({ "i32" }), make_unique(atom_type("std.i32")));
				s.define_type(extended_ast::identifier({ "str" }), nested_type());
				t.define_type(extended_ast::identifier({ "str" }), make_unique(atom_type("std.str")));

				using namespace fe::values;

				s.declare(extended_ast::identifier({ "to_string" }), extended_ast::identifier({ "_function" }));
				s.define(extended_ast::identifier({ "to_string" }));
				t.set_type(extended_ast::identifier({ "to_string" }),
					unique_type(new function_type(unique_type(new any_type()), unique_type(new atom_type("std.str")))));
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

				return { t, r, s };
			}

			static native_module* load_as_module()
			{
				auto[te, re, se] = load();
				return new native_module("std", std::move(re), std::move(te), std::move(se));
			}
		}
	}
}