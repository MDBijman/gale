#include "types.h"

#define TYPE std::variant<sum_type, product_type, atom_type, function_type, unset_type>
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

		std::string atom_type::to_string()
		{
			return "atom_type " + name;
		}

		std::string unset_type::to_string()
		{
			return "unset_type";
		}

		// Sum

		sum_type::sum_type() {}
		sum_type::sum_type(std::vector<TYPE> sum) : sum(sum) {}

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
		product_type::product_type(std::vector<std::pair<std::string, TYPE>> product) : product(product) {}

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
				r.append(std::visit(types::to_string, it->second));

				if (it != product.end() - 1)
				{
					r.append(", ");
				}
			}

			r.append(")");
			return r;
		}

		// Function Type

		function_type::function_type(std::unique_ptr<TYPE> f, std::unique_ptr<TYPE> t) : from(std::move(f)), to(std::move(t)) {}

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
			from = std::make_unique<TYPE>(*other.from.get());
			to = std::make_unique<TYPE>(*other.to.get());
		}
		function_type& function_type::operator=(const function_type& other)
		{
			this->from = std::make_unique<TYPE>(*other.from);
			this->to = std::make_unique<TYPE>(*other.to);
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

		//// Named Type

		//name_type::name_type() {}
		//name_type::name_type(std::vector<std::string> modules, std::vector<std::string> variables) : module_names(modules), variables(variables) {}

		//// Move

		//name_type::name_type(name_type&& other) : module_names(std::move(other.module_names)), variables(std::move(other.variables)) {}

		//name_type& name_type::operator=(name_type&& other)
		//{
		//	this->module_names = std::move(other.module_names);
		//	this->variables = std::move(other.variables);
		//	return *this;
		//}

		//// Copy

		//name_type::name_type(const name_type& other) : module_names(other.module_names), variables(other.variables) {}

		//name_type& name_type::operator=(const name_type& other)
		//{
		//	this->module_names = other.module_names;
		//	this->variables = other.variables;
		//	return *this;
		//}

		//std::string name_type::to_string()
		//{
		//	std::string res;
		//	for (auto& s : module_names) res.append(s);
		//	for (auto& s : variables) res.append(s);
		//	return res;
		//}

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
				if (!(one.product.at(i).second == two.product.at(i).second)) return false;
			}

			return true;
		}

		bool operator==(const function_type& one, const function_type& two)
		{
			return (*one.from == *two.from) && (*one.to == *two.to);
		}

		//bool operator==(const name_type& one, const name_type& two)
		//{
		//	if (one.module_names.size() != two.module_names.size())
		//		return false;

		//	if (one.variables.size() != two.variables.size())
		//		return false;
		//	
		//	for (unsigned int i = 0; i < one.module_names.size(); i++)
		//	{
		//		if (one.module_names.at(i) != two.module_names.at(i))
		//			return false;
		//	}
		//	for (unsigned int i = 0; i < one.variables.size(); i++)
		//	{
		//		if (one.variables.at(i) != two.variables.at(i))
		//			return false;
		//	}

		//	return true;
		//}


	}
}
#undef TYPE