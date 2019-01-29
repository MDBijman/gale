#pragma once
#include <stdint.h>

namespace fe::vm
{
	struct dll;
	struct fn;

	extern "C" dll* load_dll(uint64_t);
	extern "C" fn* load_fn(dll*, const char* name);
}