#pragma once
#include <stdint.h>

/*
 * Exposes functions to bytecode for loading dlls/functions
 */
namespace fe::vm
{
    struct dll;
    struct fn;

    extern "C" dll *load_dll(uint64_t);
    extern "C" fn *load_fn(dll *, const char *name);
} // namespace fe::vm