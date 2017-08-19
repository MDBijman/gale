#include "types.h"

namespace fe
{
	namespace types
	{

		// Sum

		sum_type::sum_type() {}
		sum_type::sum_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> sum) : sum(sum) {}

		// Move
		sum_type::sum_type(sum_type&& other)
		{
			sum = std::move(other.sum);
		}
		sum_type& sum_type::operator=(sum_type&& other)
		{
			this->sum = std::move(other.sum);
			return *this;
		}

		// Copy
		sum_type::sum_type(const sum_type& other)
		{
			sum = other.sum;
		}
		sum_type& sum_type::operator=(const sum_type& other)
		{
			this->sum = other.sum;
			return *this;
		}

		// Product

		product_type::product_type() {}
		product_type::product_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> product) : product(product) {}

		// Move
		product_type::product_type(product_type&& other)
		{
			product = std::move(other.product);
		}
		product_type& product_type::operator=(product_type&& other)
		{
			this->product = std::move(other.product);
			return *this;
		}

		// Copy
		product_type::product_type(const product_type& other)
		{
			product = other.product;
		}
		product_type& product_type::operator=(const product_type& other)
		{
			this->product = other.product;
			return *this;
		}

		// Function Type

		function_type::function_type(std::unique_ptr<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> f, std::unique_ptr<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>> t) : from(std::move(f)), to(std::move(t)) {}

		// Move
		function_type::function_type(function_type&& other)
		{
			from = std::move(other.from);
			to = std::move(other.to);
		}
		function_type& function_type::operator=(function_type&& other)
		{
			this->from = std::move(other.from);
			this->to = std::move(other.to);
			return *this;
		}

		// Copy
		function_type::function_type(const function_type& other)
		{
			from = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>>(*other.from.get());
			to = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>>(*other.to.get());
		}
		function_type& function_type::operator=(const function_type& other)
		{
			this->from = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>>(*other.from);
			this->to = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, meta_type, unset_type>>(*other.to);
			return *this;
		}

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

		bool operator==(const unset_type& one, const unset_type& two)
		{
			return true;
		}

		bool operator==(const meta_type& one, const meta_type& two)
		{
			return true;
		}

		bool operator==(const sum_type& one, const sum_type& two)
		{
			if (one.sum.size() != two.sum.size()) return false;

			for (unsigned int i = 0; i < one.sum.size(); i++)
			{
				if (!(one.sum.at(i) == two.sum.at(i))) return false;
			}

			return true;
		}

		bool operator==(const product_type& one, const product_type& two)
		{
			if (one.product.size() != two.product.size()) return false;

			for (unsigned int i = 0; i < one.product.size(); i++)
			{
				if (!(one.product.at(i) == two.product.at(i))) return false;
			}

			return true;
		}

		bool operator==(const function_type& one, const function_type& two)
		{
			return one.from.get() == two.from.get() && one.to.get() == two.to.get();
		}
	}
}