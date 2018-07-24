#pragma once
#include "fe/data/module.h"

namespace fe::stdlib::io
{
	static module load()
	{
		ext_ast::name_scope se;
		ext_ast::type_scope te;
		constants_store constants;

		{
			using namespace types;
			auto& print_ref = constants.get<plain_identifier>(constants.create<plain_identifier>());
			print_ref.full = "print";

			se.declare_variable(print_ref.full);
			se.define_variable(print_ref.full);
			te.set_type(print_ref.full, unique_type(
				new function_type(unique_type(new types::str()), unique_type(new unset()))));

			auto& println_ref = constants.get<plain_identifier>(constants.create<plain_identifier>());
			println_ref.full = "println";

			se.declare_variable(println_ref.full);
			se.define_variable(println_ref.full);
			te.set_type(println_ref.full, unique_type(
				new function_type(unique_type(new types::str()), unique_type(new unset()))));
		}

		{
			using namespace types;
			auto& get_ref = constants.get<plain_identifier>(constants.create<plain_identifier>());
			get_ref.full = "get";

			se.declare_variable(get_ref.full);
			se.define_variable(get_ref.full);
			te.set_type(get_ref.full, unique_type(
				new function_type(unique_type(new product_type()), unique_type(new types::i32()))));

			auto& time_ref = constants.get<plain_identifier>(constants.create<plain_identifier>());
			time_ref.full = "time";
			se.declare_variable(time_ref.full);
			se.define_variable(time_ref.full);
			te.set_type(time_ref.full, unique_type(new function_type(unique_type(new product_type()), unique_type(new types::i64))));
		}

		return module{
			{"std", "io"},
			std::move(te),
			std::move(se),
			std::move(constants)
		};
	}
}