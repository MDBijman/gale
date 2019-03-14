#pragma once
#include <stdint.h>

/*
 * Exposes functions to bytecode for printing.
 */
namespace fe::vm
{
	extern "C" uint64_t print(const uint64_t *);
	extern "C" uint64_t println(const uint64_t *);
} // namespace fe::vm