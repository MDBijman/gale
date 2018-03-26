#pragma once
#include "fe/data/core_ast.h"
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/modes/module.h"

namespace fe
{
	namespace stdlib
	{
		namespace typedefs
		{
			static std::tuple<type_environment, runtime_environment, resolution::scope_environment> load()
			{
				type_environment t{};
				runtime_environment r{};
				resolution::scoped_node s{};

				s.define_type("i32", resolution::nested_type());
				t.define_type(extended_ast::identifier("i32"), types::make_unique(types::i32()));
				s.define_type("str", resolution::nested_type());
				t.define_type(extended_ast::identifier("str"), types::make_unique(types::str()));
				s.define_type("bool", resolution::nested_type());
				t.define_type(extended_ast::identifier("bool"), types::make_unique(types::boolean()));

				s.declare_var_id("to_string", extended_ast::identifier("_function"));
				s.define_var_id("to_string");
				t.set_type(extended_ast::identifier("to_string"),
					types::unique_type(new types::function_type(types::unique_type(new types::any()), types::unique_type(new types::str()))));
				r.set_value("to_string", values::native_function([](values::unique_value val) -> values::unique_value {
					if (auto num = dynamic_cast<values::i32*>(val.get()))
					{
						return values::unique_value(new values::str(std::to_string(num->val)));
					}
					else if (auto str = dynamic_cast<values::str*>(val.get()))
					{
						return values::unique_value(new values::str(str->val));
					}
					else if (auto b = dynamic_cast<values::boolean*>(val.get()))
					{
						return values::unique_value(new values::str(b->to_string()));
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