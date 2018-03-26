#pragma once
#include "fe/modes/module.h"
#include "fe/data/runtime_environment.h"
#include "fe/data/type_environment.h"

namespace fe
{
	namespace stdlib
	{
		namespace output
		{
			static native_module load()
			{
				resolution::scoped_node se;
				type_environment te;
				runtime_environment re;

				using namespace values;
				{
					se.declare_var_id("print", extended_ast::identifier("_function" ));
					se.define_var_id("print");
					te.set_type(extended_ast::identifier("print" ), types::unique_type(new types::function_type(
						types::unique_type(new types::str()),
						types::unique_type(new types::unset())
					)));
					re.set_value("print", unique_value(new native_function([](unique_value in) -> unique_value {
						auto to_print = dynamic_cast<values::str*>(in.get())->val;
						std::cout << to_print << std::endl;
						return unique_value(new void_value());
					})));
				}

				return native_module(module_name{ "std", "io" }, re, te, se);
			}
		}
	}
}