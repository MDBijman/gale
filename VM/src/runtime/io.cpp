#include "io.h"
#include <iostream>

namespace fe::vm
{
	uint64_t print(const uint64_t *sp)
	{
		std::cout << *sp;
		return 8;
	}

	uint64_t println(const uint64_t *sp)
	{
		std::cout << *sp << "\n";
		return 8;
	}
} // namespace fe::vm