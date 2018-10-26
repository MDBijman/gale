#pragma once
#include "fe/data/module.h"
#include "fe/pipeline/pipeline.h"

extern "C" int fe_assert(uint64_t* regs, uint8_t* stack);

namespace fe::stdlib::assert
{
	static module load()
	{
		fe::pipeline p;

		return module_builder()
			.set_name({ "std","assert" })
			.add_function(fe::vm::function("assert", fe_assert),
				fe::types::unique_type(new fe::types::function_type(fe::types::unique_type(new fe::types::boolean()),
					fe::types::unique_type(new fe::types::voidt()))))
			.build();
	}
}