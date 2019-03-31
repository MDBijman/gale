#pragma once
#include "fe/data/module.h"
#include "fe/vm/runtime_info.h"

namespace fe::stdlib::io
{
	static module load()
	{
		using namespace types;
		return module_builder()
		  .set_name({ "std", "ffi" })
		  .add_native_function(
		    vm::LOAD_DLL, "load_dll",
		    make_unique(function_type(make_unique(ui64()), make_unique(ui64()))))
		  .add_native_function(
		    vm::LOAD_FN, "load_fn",
		    make_unique(function_type(make_unique(ui64()), make_unique(ui64()))))
		  .build();
	}
} // namespace fe::stdlib::io