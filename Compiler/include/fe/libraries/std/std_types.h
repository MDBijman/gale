#pragma once
#include "fe/data/types.h"
#include "fe/data/module.h"
#include "fe/data/bytecode.h"

namespace fe::stdlib::typedefs
{
	static module load()
	{
		return module_builder()
			.set_name({ "std" })
			.add_type("i8", types::unique_type(new types::i8()))
			.add_type("ui8", types::unique_type(new types::ui8()))
			.add_type("i16", types::unique_type(new types::i16()))
			.add_type("ui16", types::unique_type(new types::ui16()))
			.add_type("i32", types::unique_type(new types::i32()))
			.add_type("ui32", types::unique_type(new types::ui32()))
			.add_type("i64", types::unique_type(new types::i64()))
			.add_type("ui64", types::unique_type(new types::ui64()))
			.add_type("str", types::unique_type(new types::str()))
			.add_type("bool", types::unique_type(new types::boolean()))
			.build();
	}
}