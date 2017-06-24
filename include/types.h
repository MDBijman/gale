#pragma once
#include <vector>
#include <variant>
#include <memory>

namespace fe
{
	namespace types
	{
		// Atomic types

		struct integer_type {};

		struct string_type {};

		struct void_type {};

		// Composition types

		struct product_type;
		struct function_type;

		struct sum_type 
		{
			std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>> sum;
		};

		struct product_type 
		{
			std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>> product;
		};

		struct function_type 
		{
			function_type(product_type from, product_type to) : from(from), to(to) {}
			product_type from, to;
		};

		using type = std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>;
	}
}