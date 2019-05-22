#include "io.h"
#include <assert.h>
#include <iostream>
#include <string>

namespace fe::vm
{
	void print(uint8_t *regs, uint8_t first_in, uint8_t first_out)
	{
		//std::cout << "executing print " << std::to_string(first_in) << "\n";
		std::cout << *(uint64_t *)(regs + first_in - 7);
	}

	void println(uint8_t *regs, uint8_t first_in, uint8_t first_out)
	{
		std::cout << *(uint64_t *)(regs + first_in - 7) << "\n";
	}
} // namespace fe::vm