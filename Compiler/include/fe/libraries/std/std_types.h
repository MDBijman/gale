#pragma once
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/data/module.h"

namespace fe::stdlib::typedefs
{
	static module load()
	{
		ext_ast::type_scope t;
		ext_ast::name_scope s;
		core_ast::value_scope r;
		constants_store constants;

		{
			using namespace values;
			using namespace types;
			auto& i32_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			i32_name.full = "i32";
			s.define_type(i32_name.full, {});
			t.define_type(i32_name.full, unique_type(new types::i32()));

			auto& i64_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			i64_name.full = "i64";
			s.define_type(i64_name.full, {});
			t.define_type(i64_name.full, unique_type(new types::i64()));

			auto& ui32_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			ui32_name.full = "ui32";
			s.define_type(ui32_name.full, {});
			t.define_type(ui32_name.full, unique_type(new types::ui32()));

			auto& ui64_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			ui64_name.full = "ui64";
			s.define_type(ui64_name.full, {});
			t.define_type(ui64_name.full, unique_type(new types::ui64()));

			auto& str_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			str_name.full = "str";
			s.define_type(str_name.full, {});
			t.define_type(str_name.full, unique_type(new types::str()));

			auto& bool_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			bool_name.full = "bool";
			s.define_type(bool_name.full, {});
			t.define_type(bool_name.full, unique_type(new types::boolean()));

			auto& to_string_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			to_string_name.full = "to_string";
			s.declare_variable(to_string_name.full);
			s.define_variable(to_string_name.full);
			t.set_type(to_string_name.full,
				unique_type(new function_type(unique_type(new any()), unique_type(new types::str()))));
			r.set_value(to_string_name.full, unique_value(new native_function([](unique_value val) -> unique_value {
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
			auto& to_f32_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			to_f32_name.full = "to_f32";
			s.declare_variable(to_f32_name.full);
			s.define_variable(to_f32_name.full);
			t.set_type(to_f32_name.full, types::unique_type(new types::function_type(
				types::unique_type(new types::i64()), types::unique_type(new types::f32))));
			r.set_value(to_f32_name.full, values::unique_value(new values::native_function([](values::unique_value t) -> values::unique_value {
				values::i64* v = static_cast<values::i64*>(t.get());
				return values::unique_value(new values::f32(float(v->val)));
			})));
		}

		return module{
			{"std"},
			std::move(r),
			std::move(t),
			std::move(s),
			std::move(constants)
		};
	}
}