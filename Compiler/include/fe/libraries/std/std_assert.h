#pragma once
#include "fe/data/module.h"
#include "fe/pipeline/pipeline.h"

namespace fe::stdlib::assert
{
	static module load()
	{
		fe::pipeline p;
		
		return module_builder()
			.set_name({ "std","assert" })
			.add_function(fe::vm::function("assert", fe::vm::native_code([](fe::vm::machine_state& ms) {
				bool arg2 = ms.stack[ms.registers[fe::vm::fp_reg] - 17];
				assert(arg2);
				ms.ret(1);
			})),
			fe::types::unique_type(new fe::types::function_type(fe::types::unique_type(new fe::types::boolean()),
				fe::types::unique_type(new fe::types::voidt()))))
			.build();
	}
}