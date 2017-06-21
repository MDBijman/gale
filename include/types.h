#pragma once
#include <vector>
#include <variant>
#include <memory>

namespace fe
{
	namespace types
	{
		struct type {};

		// Composition types

		struct sum_type : type
		{
			std::vector<std::unique_ptr<type>> sum;
		};

		struct product_type : type
		{
			std::vector<std::unique_ptr<type>> product;
		};

		// Atomic types

		struct integer_type : type {};

		struct string_type : type {};

		struct void_type : type {};
	}


}