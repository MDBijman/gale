#pragma once
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/data/scope.h"

namespace fe::stdlib::typedefs
{
	static scope load()
	{
		ext_ast::type_scope t;
		ext_ast::name_scope s;
		value_scope r;

		{
			using namespace values;
			using namespace types;
			s.define_type("i32", {});
			t.define_type("i32", unique_type(new types::i32()));
			s.define_type("i64", {});
			t.define_type("i64", unique_type(new types::i64()));
			s.define_type("ui32", {});
			t.define_type("ui32", unique_type(new types::ui32()));
			s.define_type("ui64", {});
			t.define_type("ui64", unique_type(new types::ui64()));

			s.define_type("str", {});
			t.define_type("str", unique_type(new types::str()));

			s.define_type("bool", {});
			t.define_type("bool", unique_type(new types::boolean()));

			s.declare_variable("to_string");
			s.define_variable("to_string");
			t.set_type("to_string",
				unique_type(new function_type(unique_type(new any()), unique_type(new types::str()))));
			r.set_value("to_string", unique_value(new native_function([](unique_value val) -> unique_value {
				if (auto num = dynamic_cast<values::i32*>(val.get()))
				{
					return unique_value(new values::str(std::to_string(num->val)));
				}
				else if (auto num = dynamic_cast<values::i64*>(val.get()))
				{
					return unique_value(new values::str(std::to_string(num->val)));
				}
				else if (auto str = dynamic_cast<values::str*>(val.get()))
				{
					return unique_value(new values::str(str->val));
				}
				else if (auto b = dynamic_cast<values::boolean*>(val.get()))
				{
					return unique_value(new values::str(b->to_string()));
				}
				else if (auto f = dynamic_cast<values::f32*>(val.get()))
				{
					return unique_value(new values::str(std::to_string(f->val)));
				}
				throw interp_error{ "Cannot cast this value to string" };
			})));
		}

		{
			s.declare_variable("to_f32");
			s.define_variable("to_f32");
			t.set_type("to_f32", types::unique_type(new types::function_type(
				types::unique_type(new types::i64()), types::unique_type(new types::f32))));
			r.set_value("to_f32", values::unique_value(new values::native_function([](values::unique_value t) -> values::unique_value {
				values::i64* v = static_cast<values::i64*>(t.get());
				return values::unique_value(new values::f32(float(v->val)));
			})));
		}

		return scope(std::move(r), std::move(t), std::move(s));
	}
}