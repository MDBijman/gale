#pragma once
#include "fe/modes/module.h"

namespace fe::stdlib::c
{
	static native_module load()
	{
		runtime_environment std_re;
		type_environment std_te;
		scope_environment std_se;
		std_re.name = "std";

		runtime_environment re;
		type_environment te;
		scope_environment se;
		re.name = "c";

		{
			using namespace fe;
			re.set_value("load_dll", values::native_function([](values::unique_value v) -> values::unique_value {

			}));
			te.set_type(extended_ast::identifier({ "load_dll" }), types::unique_type(new types::function_type(
				types::unique_type(new types::atom_type("std.str")),
				types::unique_type(new types::atom_type("dll"))
			)));
			se.declare(extended_ast::identifier({ "load_dll" }), extended_ast::identifier({ "_func" }));
			se.define(extended_ast::identifier({ "load_dll" }));
		}

		std_re.add_module(std::move(re));
		std_se.add_module("c", std::move(se));
		std_te.add_module("c", std::move(te));

		native_module c_module("std.c", re, te, se);
	}
}