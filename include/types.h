#pragma once
#include <vector>
#include <variant>
#include <memory>

#define TYPE std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>
namespace fe
{
	namespace types
	{
		// Atomic types

		struct integer_type 
		{
			std::string to_string();
		};

		struct boolean_type 
		{
			std::string to_string();
		};

		struct string_type 
		{
			std::string to_string();
		};

		struct void_type 
		{
			std::string to_string();
		};
		
		struct unset_type
		{
			std::string to_string();
		};

		struct meta_type
		{
			std::string to_string();
		};

		// Composition types

		struct function_type;
		struct product_type;

		struct sum_type 
		{
			sum_type();
			sum_type(std::vector<TYPE> sum);

			// Move
			sum_type(sum_type&& other);
			sum_type& operator=(sum_type&& other);

			// Copy
			sum_type(const sum_type& other);
			sum_type& operator=(const sum_type& other);

			std::string to_string();

			std::vector<TYPE> sum;
		};

		struct product_type
		{
			product_type();
			product_type(std::vector<std::pair<std::string, TYPE>> product);

			// Move
			product_type(product_type&& other);
			product_type& operator=(product_type&& other);

			// Copy
			product_type(const product_type& other);
			product_type& operator=(const product_type& other);

			std::string to_string();

			std::vector<std::pair<std::string, TYPE>> product;
		};

		struct function_type
		{
			function_type(std::unique_ptr<TYPE> f, std::unique_ptr<TYPE> t);

			// Move
			function_type(function_type&& other);
			function_type& operator=(function_type&& other);

			// Copy
			function_type(const function_type& other);
			function_type& operator=(const function_type& other);

			std::string to_string();

			std::unique_ptr<TYPE> from, to;
		};

		// Variant

		using type = TYPE;
		using unique_type = std::unique_ptr<type>;

		// Helper methods

		const auto make_unique = [](auto x) {
			return std::make_unique<type>(x);
		};

		const auto to_string = [](auto& x) -> std::string {
			return x.to_string();
		};

		// Operators

		bool operator==(const integer_type& one, const integer_type& two);

		bool operator==(const boolean_type& one, const boolean_type& two);

		bool operator==(const string_type& one, const string_type& two);

		bool operator==(const void_type& one, const void_type& two);

		bool operator==(const unset_type& one, const unset_type& two);

		bool operator==(const meta_type& one, const meta_type& two);

		bool operator==(const sum_type& one, const sum_type& two);

		bool operator==(const product_type& one, const product_type& two);

		bool operator==(const function_type& one, const function_type& two);
	}
}
#undef TYPE