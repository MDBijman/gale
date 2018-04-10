#pragma once
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/data/scope.h"

namespace fe
{
	namespace stdlib
	{
		namespace typedefs
		{
			static scope load()
			{
				runtime_environment r{};
				ext_ast::type_scope t{};
				ext_ast::name_scope s{};

				s.define_type("i32", {});
				t.define_type("i32", types::make_unique(types::i32()));
				s.define_type("i64", {});
				t.define_type("i64", types::make_unique(types::i64()));

				s.define_type("str", {});
				t.define_type("str", types::make_unique(types::str()));

				s.define_type("bool", {});
				t.define_type("bool", types::make_unique(types::boolean()));

				s.declare_variable("to_string");
				s.define_variable("to_string");
				t.set_type("to_string",
					types::unique_type(new types::function_type(types::unique_type(new types::any()), types::unique_type(new types::str()))));
				r.set_value("to_string", values::native_function([](values::unique_value val) -> values::unique_value {
					if (auto num = dynamic_cast<values::i32*>(val.get()))
					{
						return values::unique_value(new values::str(std::to_string(num->val)));
					}
					else if (auto num = dynamic_cast<values::i64*>(val.get()))
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

				return scope(std::move(r), std::move(t), std::move(s));
			}
		}
	}
}