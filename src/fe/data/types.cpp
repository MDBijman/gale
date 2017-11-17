#include "fe/data/types.h"

namespace fe
{
	namespace types
	{
		// Atom Type

		atom_type::atom_type(std::string name) : name(name) {}

		// Move
		atom_type::atom_type(atom_type&& other) : name(std::move(other.name)) {}
		atom_type& atom_type::operator=(atom_type&& other)
		{
			this->name = std::move(other.name);
			return *this;
		}

		// Copy
		atom_type::atom_type(const atom_type& other) : name(other.name) {}
		atom_type& atom_type::operator=(const atom_type& other)
		{
			this->name = other.name;
			return *this;
		}

		std::string atom_type::to_string() const
		{
			return "atom_type " + name;
		}

		// Unset
		std::string unset_type::to_string() const
		{
			return "unset_type";
		}

		// Array

		array_type::array_type() : element_type(new unset_type()) {}
		array_type::array_type(unique_type t) : element_type(std::move(t)) {}
		array_type::array_type(type& t) : element_type(t.copy()) {}

		// Move

		array_type::array_type(array_type&& other) : element_type(std::move(other.element_type)) {}
		array_type& array_type::operator=(array_type&& other)
		{
			this->element_type = std::move(other.element_type);
			return *this;
		}

		// Copy

		array_type::array_type(const array_type& other) : element_type(other.element_type->copy()) {}
		array_type& array_type::operator=(const array_type& other)
		{
			this->element_type.reset(other.element_type->copy());
			return *this;
		}

		std::string array_type::to_string() const
		{
			return "array_type (" + element_type->to_string() + ")";
		}


		// Reference

		reference_type::reference_type() : referred_type(new unset_type()) {}
		reference_type::reference_type(unique_type t) : referred_type(std::move(t)) {}
		reference_type::reference_type(type& t) : referred_type(t.copy()) {}

		// Move

		reference_type::reference_type(reference_type&& other) : referred_type(std::move(other.referred_type)) {}
		reference_type& reference_type::operator=(reference_type&& other)
		{
			this->referred_type = std::move(other.referred_type);
			return *this;
		}

		// Copy

		reference_type::reference_type(const reference_type& other) : referred_type(other.referred_type->copy()) {}
		reference_type& reference_type::operator=(const reference_type& other)
		{
			this->referred_type.reset(other.referred_type->copy());
			return *this;
		}

		std::string reference_type::to_string() const
		{
			return "reference_type (" + referred_type->to_string() + ")";
		}


		// Sum

		sum_type::sum_type() {}
		sum_type::sum_type(std::vector<unique_type> sum) : sum(std::move(sum)) {}

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
			for (const auto& elem : other.sum)
			{
				sum.push_back(std::unique_ptr<type>(elem->copy()));
			}
		}
		sum_type& sum_type::operator=(const sum_type& other)
		{
			for (const auto& elem : other.sum)
			{
				sum.push_back(std::unique_ptr<type>(elem->copy()));
			}
			return *this;
		}

		std::string sum_type::to_string() const
		{
			std::string r = "sum_type (";

			for (auto it = sum.begin(); it != sum.end(); ++it)
			{
				r.append((*it)->to_string());

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
		product_type::product_type(std::vector<std::pair<std::string, unique_type>> product) : product(std::move(product)) {}

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
			for (const auto& pair : other.product)
			{
				product.push_back({ pair.first, types::unique_type(pair.second->copy()) });
			}
		}
		product_type& product_type::operator=(const product_type& other)
		{
			for (const auto& pair : other.product)
			{
				product.push_back({ pair.first, types::unique_type(pair.second->copy()) });
			}
			return *this;
		}

		std::string product_type::to_string() const
		{
			std::string r = "product_type (";

			for (auto it = product.begin(); it != product.end(); ++it)
			{
				r.append(it->second->to_string());

				if (it != product.end() - 1)
				{
					r.append(", ");
				}
			}

			r.append(")");
			return r;
		}

		// Function Type

		function_type::function_type(unique_type f, unique_type t) : from(std::move(f)), to(std::move(t)) {}
		function_type::function_type(const type& f, const type & t) : from(f.copy()), to(t.copy()) {}

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
			from = types::make_unique(*other.from.get());
			to = types::make_unique(*other.to.get());
		}
		function_type& function_type::operator=(const function_type& other)
		{
			this->from = types::make_unique(*other.from.get());
			this->to = types::make_unique(*other.to.get());
			return *this;
		}

		std::string function_type::to_string() const
		{
			std::string r = "function_type (";

			r.append(from->to_string());
			r.append(", ");
			r.append(to->to_string());

			r.append(")");
			return r;
		}

		// Any Type

		any_type::any_type() {}

		any_type::any_type(any_type&& other) {}

		any_type& any_type::operator=(any_type&& other)
		{
			return *this;
		}

		any_type::any_type(const any_type& other) {}

		any_type& any_type::operator=(const any_type& other)
		{
			return *this;
		}

		std::string any_type::to_string() const
		{
			return "any_type";
		}

		// Operators
		bool operator==(const atom_type& one, const atom_type& two)
		{
			return one.name == two.name;
		}

		bool operator==(const unset_type& one, const unset_type& two)
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
				if (!(*(one.product.at(i).second) == two.product.at(i).second.get())) return false;
			}

			return true;
		}

		bool operator==(const function_type& one, const function_type& two)
		{
			return (*one.from == two.from.get()) && (*one.to == two.to.get());
		}

		bool operator==(const array_type& one, const array_type& two)
		{
			return one.element_type == two.element_type;
		}

		bool operator==(const reference_type& one, const reference_type& two)
		{
			return *one.referred_type == two.referred_type.get();
		}

		bool operator==(const any_type& one, const any_type& two)
		{
			return true;
		}
	}
}