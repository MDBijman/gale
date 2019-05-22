#pragma once
#include "fe/data/module.h"
#include "fe/vm/runtime_info.h"

namespace fe::stdlib::io
{
	static module load()
	{
		using namespace types;
		return module_builder()
		  .set_name("std.io")
		  .add_native_function(
		    vm::PRINT, "print",
		    make_unique(function_type(make_unique(ui64()), make_unique(voidt()))))
		  .add_native_function(
		    vm::PRINTLN, "println",
		    make_unique(function_type(make_unique(ui64()), make_unique(voidt()))))
		  .build();
	}
} // namespace fe::stdlib::io