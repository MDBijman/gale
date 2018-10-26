#include "fe/libraries/std/std_assert.h"

extern "C" int fe_assert(uint64_t* regs, uint8_t* stack)
{
	assert(regs[0]);
	return 0;
}