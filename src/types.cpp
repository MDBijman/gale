#include "types.h"

namespace fe
{
	namespace types
	{

		std::string integer_type::to_string()
		{
			return "integer_type";
		}

		std::string boolean_type::to_string()
		{
			return "boolean_type";
		}

		std::string string_type::to_string()
		{
			return "integer_type";
		}

		std::string void_type::to_string()
		{
			return "integer_type";
		}

		std::string unset_type::to_string()
		{
			return "integer_type";
		}

		std::string meta_type::to_string()
		{
			return "integer_type";
		}

		// Sum

		sum_type::sum_type() {}
		sum_type::sum_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>> sum) : sum(sum) {}

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

		std::string sum_type::to_string()
		{
			std::string r = "sum_type (";
			
			// Probably inefficient
			for (auto it = sum.begin(); it != sum.end(); ++it)
			{
				r.append(std::visit(types::to_string, *it));

				if (it != sum.end() - 1)
				{
					r.append(", ");
				}
			}

			r.append(")");
			return r;
		}

		// Product

		product_type::product_type() {}
		product_type::product_type(std::vector<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>> product) : product(product) {}

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

		std::string product_type::to_string()
		{
			std::string r = "product_type (";
			
			// Probably inefficient
			for (auto it = product.begin(); it != product.end(); ++it)
			{
				r.append(std::visit(types::to_string, *it));

				if (it != product.end() - 1)
				{
					r.append(", ");
				}
			}

			r.append(")");
			return r;
		}

		// Function Type

		function_type::function_type(std::unique_ptr<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>> f, std::unique_ptr<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>> t) : from(std::move(f)), to(std::move(t)) {}

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
			from = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>>(*other.from.get());
			to = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>>(*other.to.get());
		}
		function_type& function_type::operator=(const function_type& other)
		{
			this->from = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>>(*other.from);
			this->to = std::make_unique<std::variant<sum_type, product_type, integer_type, string_type, void_type, function_type, boolean_type, meta_type, unset_type>>(*other.to);
			return *this;
		}

		std::string function_type::to_string()
		{
			std::string r = "function_type (";
			
			r.append(std::visit(types::to_string, *from));
			r.append(", ");
			r.append(std::visit(types::to_string, *to));

			r.append(")");
			return r;
		}

		// Operators

		bool operator==(const integer_type& one, const integer_type& two)
		{
			return true;
		}

		bool operator==(const boolean_type & one, const boolean_type & two)
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
			return (*one.from == *two.from) && (*one.to == *two.to);
		}
}
}