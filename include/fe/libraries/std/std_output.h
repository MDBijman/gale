#pragma once
#include "fe/data/scope.h"

namespace fe::stdlib::output
{
	static scope load()
	{
		ext_ast::name_scope se;
		runtime_environment re;
		ext_ast::type_scope te;

		using namespace values;
		{
			se.declare_variable("print");
			se.define_variable("print");
			te.set_type("print", types::unique_type(new types::function_type(
				types::unique_type(new types::str()),
				types::unique_type(new types::unset())
			)));
			re.set_value("print", unique_value(new native_function([](unique_value in) -> unique_value {
				auto to_print = dynamic_cast<values::str*>(in.get())->val;
				std::cout << to_print << std::endl;
				return unique_value(new void_value());
			})));
		}

		return scope(re, te, se);
	}
}