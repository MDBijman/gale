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
				scope_environment std_se;
				type_environment std_te;
				runtime_environment std_re;
				std_re.name = "std";

				scope_environment se;
				type_environment te;
				runtime_environment re;
				re.name = "io";

				using namespace fe::types;
				using namespace values;
				{
					se.declare(extended_ast::identifier({ "print" }), extended_ast::identifier({ "_function" }));
					se.define(extended_ast::identifier({ "print" }));
					te.set_type(extended_ast::identifier({ "print" }), unique_type(new function_type(
						unique_type(new atom_type("std.str")),
						unique_type(new unset_type())
					)));
					re.set_value("print", unique_value(new native_function([](unique_value in) -> unique_value {
						auto to_print = dynamic_cast<string*>(in.get())->val;
						std::cout << to_print << std::endl;
						return unique_value(new void_value());
					})));
				}

				std_re.add_module(std::move(re));
				std_te.add_module("io", std::move(te));
				std_se.add_module("io", std::move(se));
				return native_module{ "std.io", std_re, std_te, std_se };
			}
		}
	}
}