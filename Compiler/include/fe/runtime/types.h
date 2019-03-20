#pragma once
#include "fe/data/types.h"
#include "fe/data/module.h"
#include "fe/data/bytecode.h"

namespace fe::stdlib::typedefs
{
static module load()
{
	using namespace types;
	return module_builder()
		.set_name({"std"})
		.add_type("i8", make_unique(i8()))
		.add_type("ui8", make_unique(ui8()))
		.add_type("i16", make_unique(i16()))
		.add_type("ui16", make_unique(ui16()))
		.add_type("i32", make_unique(i32()))
		.add_type("ui32", make_unique(ui32()))
		.add_type("i64", make_unique(i64()))
		.add_type("ui64", make_unique(ui64()))
		.add_type("str", make_unique(str()))
		.add_type("bool", make_unique(types::boolean()))
		.build();
}
} // namespace fe::stdlib::typedefs