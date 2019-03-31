#pragma once
#include <stdint.h>

/*
 * Exposes functions to bytecode for loading dlls/functions
 */
namespace fe::vm
{
    struct dll;
    struct fn;

	extern "C" void load_dll(uint8_t *, uint8_t, uint8_t);
	extern "C" void load_fn(uint8_t *, uint8_t, uint8_t);
} // namespace fe::vm