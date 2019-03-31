#pragma once
#include <stdint.h>

/*
 * Exposes functions to bytecode for printing.
 */
namespace fe::vm
{
	extern "C" void print(uint8_t *, uint8_t, uint8_t);
	extern "C" void println(uint8_t *, uint8_t, uint8_t);
} // namespace fe::vm