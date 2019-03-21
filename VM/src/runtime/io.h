#pragma once
#include <stdint.h>

/*
 * Exposes functions to bytecode for printing.
 */
namespace fe::vm
{
	extern "C" uint64_t print(const uint8_t *, uint8_t);
	extern "C" uint64_t println(const uint8_t *, uint8_t);
} // namespace fe::vm