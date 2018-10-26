#pragma once
#include "fe/data/module.h"

extern "C" int fe_print(uint64_t* regs, uint8_t* stack);
extern "C" int fe_println(uint64_t* regs, uint8_t* stack);
extern "C" int fe_time(uint64_t* regs, uint8_t* stack);

namespace fe::stdlib::io
{
	static module load()
	{
		return module_builder()
			.set_name({ "std","io" })
			.add_function(
				// #todo ms.ret(n) can be calculated from the type and added automatically
				vm::function("print", fe_print),
				types::unique_type(new types::function_type(types::unique_type(new types::ui64()), types::unique_type(new types::voidt()))))
			.add_function(
				vm::function("println", fe_println),
				types::unique_type(new types::function_type(types::unique_type(new types::ui64()), types::unique_type(new types::voidt()))))
			.add_function(
				vm::function("time", fe_time),
				types::unique_type(new types::function_type(types::unique_type(new types::product_type()), types::unique_type(new types::i64()))))
			.build();
	}
}