#include "io.h"
#include <iostream>
#include <assert.h>

namespace fe::vm
{
	uint64_t print(const uint8_t *first, uint8_t count)
	{
		assert(count == 8);
		std::cout << *(uint64_t*)(first);
		return 8;
	}

	uint64_t println(const uint8_t *first, uint8_t count)
	{
		assert(count == 8);
		std::cout << *(uint64_t*)(first) << "\n";
		return 8;
	}
} // namespace fe::vm