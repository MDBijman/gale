#pragma once
#include <stdint.h>
#include "fe/data/bytecode.h"

extern "C" uint16_t *vm_init(void *[]);
extern "C" uint64_t *vm_interpret(const fe::vm::byte *first);