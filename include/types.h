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
		struct unset_type {};
		struct meta_type {};

		// Composition types

		struct function_type;
		struct product_type;

		struct sum_type 
		{
			sum_type();
			sum_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> sum);

			// Move
			sum_type(sum_type&& other);
			sum_type& operator=(sum_type&& other);

			// Copy
			sum_type(const sum_type& other);
			sum_type& operator=(const sum_type& other);

			std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> sum;
		};

		struct product_type
		{
			product_type();
			product_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> product);

			// Move
			product_type(product_type&& other);
			product_type& operator=(product_type&& other);

			// Copy
			product_type(const product_type& other);
			product_type& operator=(const product_type& other);

			std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> product;
		};

		struct function_type
		{
			function_type(std::unique_ptr<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> f, std::unique_ptr<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> t);

			// Move
			function_type(function_type&& other);
			function_type& operator=(function_type&& other);

			// Copy
			function_type(const function_type& other);
			function_type& operator=(const function_type& other);

			std::unique_ptr<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> from, to;
		};

		// Variant

		using type = std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>;
		using unique_type = std::unique_ptr<type>;

		// Helper methods

		const auto make_unique = [](auto x) {
			return std::make_unique<type>(x);
		};

		// Operators

		bool operator==(const integer_type& one, const integer_type& two);

		bool operator==(const string_type& one, const string_type& two);

		bool operator==(const void_type& one, const void_type& two);

		bool operator==(const unset_type& one, const unset_type& two);

		bool operator==(const meta_type& one, const meta_type& two);

		bool operator==(const sum_type& one, const sum_type& two);

		bool operator==(const product_type& one, const product_type& two);

		bool operator==(const function_type& one, const function_type& two);
	}
}