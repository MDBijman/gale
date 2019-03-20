#pragma once
#include <stdint.h>

namespace fe::vm
{
	using native_function_id = uint64_t;
	static constexpr native_function_id LOAD_DLL = 0;
	static constexpr native_function_id LOAD_FN = 1;
	static constexpr native_function_id PRINT = 2;
	static constexpr native_function_id PRINTLN = 3;
}