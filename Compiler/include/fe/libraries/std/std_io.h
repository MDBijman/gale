#pragma once
#include "fe/data/module.h"

namespace fe::stdlib::io
{
	static module load()
	{
		fe::pipeline p{};
		
		return module_builder()
			.set_name({"std","io"})
			.add_function(
				// #todo ms.ret(n) can be calculated from the type and added automatically
				vm::function("print", [](vm::machine_state& ms) { 
					uint8_t* arg = &ms.stack[ms.registers[vm::fp_reg] - 24];
					std::cout << vm::read_i64(arg); 
					ms.ret(8); 
				}),
				types::unique_type(new types::function_type(types::unique_type(new types::i64()), types::unique_type(new types::voidt()))))
			.add_function(
				vm::function("println", [](vm::machine_state& ms) { 
					uint8_t* arg = &ms.stack[ms.registers[vm::fp_reg] - 24];
					std::cout << vm::read_i64(arg) << std::endl; 
					ms.ret(8); 
				}),
				types::unique_type(new types::function_type(types::unique_type(new types::i64()), types::unique_type(new types::voidt()))))
			.add_function(
				vm::function("time", [](vm::machine_state& ms) { ms.registers[vm::ret_reg] = std::chrono::high_resolution_clock::now().time_since_epoch().count(); ms.ret(0); }),
				types::unique_type(new types::function_type(types::unique_type(new types::product_type()), types::unique_type(new types::i64()))))
			.build();
	}
}