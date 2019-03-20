#include "io.h"
#include <iostream>
#include <assert.h>

namespace fe::vm
{
	uint64_t print(const uint64_t *first, uint8_t count)
	{
		assert(count == 1);
		std::cout << *first;
		return 8;
	}

	uint64_t println(const uint64_t *first, uint8_t count)
	{
		assert(count == 1);
		std::cout << *first << "\n";
		return 8;
	}
} // namespace fe::vm