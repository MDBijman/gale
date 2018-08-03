#pragma once
#include "fe/data/types.h"
#include "fe/data/module.h"

namespace fe::stdlib::typedefs
{
	static module load()
	{
		ext_ast::type_scope t;
		ext_ast::name_scope s;
		constants_store constants;

		{
			using namespace types;
			auto& i8_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			i8_name.full = "i8";
			s.define_type(i8_name.full, {});
			t.define_type(i8_name.full, unique_type(new types::i8()));

			auto& ui8_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			ui8_name.full = "ui8";
			s.define_type(ui8_name.full, {});
			t.define_type(ui8_name.full, unique_type(new types::ui8()));

			auto& i16_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			i16_name.full = "i16";
			s.define_type(i16_name.full, {});
			t.define_type(i16_name.full, unique_type(new types::i16()));

			auto& ui16_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			ui16_name.full = "ui16";
			s.define_type(ui16_name.full, {});
			t.define_type(ui16_name.full, unique_type(new types::ui16()));

			auto& i32_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			i32_name.full = "i32";
			s.define_type(i32_name.full, {});
			t.define_type(i32_name.full, unique_type(new types::i32()));

			auto& ui32_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			ui32_name.full = "ui32";
			s.define_type(ui32_name.full, {});
			t.define_type(ui32_name.full, unique_type(new types::ui32()));

			auto& i64_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			i64_name.full = "i64";
			s.define_type(i64_name.full, {});
			t.define_type(i64_name.full, unique_type(new types::i64()));

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
			s.declare_variable(to_string_name.full, 0);
			s.define_variable(to_string_name.full);
			t.set_type(to_string_name.full,
				unique_type(new function_type(unique_type(new any()), unique_type(new types::str()))));
		}

		{
			auto& to_f32_name = constants.get<plain_identifier>(constants.create<plain_identifier>());
			to_f32_name.full = "to_f32";
			s.declare_variable(to_f32_name.full, 0);
			s.define_variable(to_f32_name.full);
			t.set_type(to_f32_name.full, types::unique_type(new types::function_type(
				types::unique_type(new types::i64()), types::unique_type(new types::f32))));
		}

		return module{
			{"std"},
			std::move(t),
			std::move(s),
			std::move(constants)
		};
	}
}