#pragma once
#include <bitset>

namespace fe
{
	namespace tags
	{
		enum tag
		{
			ref,
			array,
			NR_OF_TAGS
		};

		using tags = std::bitset<NR_OF_TAGS>;
	}
}