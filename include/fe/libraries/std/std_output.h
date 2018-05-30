#pragma once
#include "fe/data/scope.h"

namespace fe::stdlib::output
{
	static scope load()
	{
		ext_ast::name_scope se;
		ext_ast::type_scope te;
		value_scope re;

		{
			using namespace types;
			using namespace values;
			se.declare_variable("print");
			se.define_variable("print");
			te.set_type("print", unique_type(
				new function_type(unique_type(new types::str()), unique_type(new unset()))));
			re.set_value("print", unique_value(new native_function([](unique_value in) -> unique_value {
				auto to_print = dynamic_cast<values::str*>(in.get())->val;
				std::cout << to_print;
				return unique_value(new void_value());
			})));

			se.declare_variable("println");
			se.define_variable("println");
			te.set_type("println", unique_type(
				new function_type(unique_type(new types::str()), unique_type(new unset()))));
			re.set_value("println", unique_value(new native_function([](unique_value in) -> unique_value {
				auto to_print = dynamic_cast<values::str*>(in.get())->val;
				std::cout << to_print << std::endl;
				return unique_value(new void_value());
			})));
		}

		return scope(re, te, se);
	}
}