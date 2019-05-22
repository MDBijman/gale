#pragma once

extern "C" uint16_t *vm_init(void *[]);
extern "C" uint64_t *vm_interpret(const byte *first);