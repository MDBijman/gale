#pragma once
#include "module.h"
#include "runtime_environment.h"
#include "typecheck_environment.h"

namespace fe
{
	namespace stdlib
	{
		namespace output
		{
			static native_module load()
			{
				runtime_environment std_re;
				std_re.name = "std";
				typecheck_environment std_te;
				std_te.name = "std";

				runtime_environment re;
				re.name = "io";
				typecheck_environment te;
				te.name = "io";

				using namespace types;
				using namespace values;
				{
					te.set_type("print", unique_type(new function_type(
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
				std_te.add_module(std::move(te));
				return native_module{ "std.io", std_re, std_te };
			}
		}
	}
}