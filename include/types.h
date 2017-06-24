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
			sum_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>> sum) : sum(sum) {}
			std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>> sum;
		};

		struct product_type
		{
			product_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>> product) : product(product) {}
			std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>> product;
		};

		struct function_type
		{
			function_type(product_type from, product_type to) : from(from), to(to) {}
			product_type from, to;
		};

		// Variant

		using type = std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type>;

		// Operators

		bool operator==(const integer_type& one, const integer_type& two)
		{
			return true;
		}


		bool operator==(const string_type& one, const string_type& two)
		{
			return true;
		}


		bool operator==(const void_type& one, const void_type& two) 
		{
			return true;
		}

		bool operator==(const sum_type& one, const sum_type& two)
		{
			if (one.sum.size() != two.sum.size()) return false;

			for (int i = 0; i < one.sum.size(); i++)
			{
				if (!(one.sum.at(i) == two.sum.at(i))) return false;
			}

			return true;
		}

		bool operator==(const product_type& one, const product_type& two)
		{
			if (one.product.size() != two.product.size()) return false;

			for (int i = 0; i < one.product.size(); i++)
			{
				if (!(one.product.at(i) == two.product.at(i))) return false;
			}

			return true;
		}

		bool operator==(const function_type& one, const function_type& two)
		{
			return one.from == two.from && one.to == two.to;
		}

	}
}