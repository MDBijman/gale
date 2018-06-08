#include "fe/data/values.h"
#include "fe/data/core_ast.h"

namespace fe
{
	namespace values
	{
		// Function

		function::function(node_id func) : func(func) {}

		// Copy
		function::function(const function& other) : func(other.func) {}
		function& function::operator=(const function& other)
		{
			this->func = other.func;
			return *this;
		}

		// Move
		function::function(function&& other) : func(other.func) {}
		function& function::operator=(function&& other)
		{
			this->func = other.func;
			return *this;
		}

		std::string function::to_string() const
		{
			return "function";
		}

		// Tuple


		tuple::tuple() {}
		tuple::tuple(std::vector<unique_value> values) : val(std::move(values)) {}

		// Copy
		tuple::tuple(const tuple& other) 
		{
			for (decltype(auto) elem : other.val)
			{
				this->val.push_back(unique_value(elem->copy()));
			}
		}
		tuple& tuple::operator=(const tuple& other)
		{
			for (decltype(auto) elem : other.val)
			{
				this->val.push_back(unique_value(elem->copy()));
			}
			return *this;
		}

		// Move
		tuple::tuple(tuple&& other) : val(std::move(other.val)) {}
		tuple& tuple::operator=(tuple&& other)
		{
			this->val = std::move(other.val);
			return *this;
		}

		std::string tuple::to_string() const
		{
			std::string r("tuple (");

			for (auto it = val.begin(); it != val.end(); it++)
			{
				r.append(it->get()->to_string());
				if (it != val.end() - 1)
					r.append(", ");
			}

			r.append(")");
			return r;
		}

		// Native Function

		// Copy
		native_function::native_function(const native_function& other) = default;
		native_function& native_function::operator=(const native_function& other)
		{
			this->function = other.function;
			return *this;
		}

		// Move
		native_function::native_function(native_function&& other) : function(std::move(other.function)) {}
		native_function& native_function::operator=(native_function&& other)
		{
			this->function = std::move(other.function);
			return *this;
		}

		std::string native_function::to_string() const
		{
			return "native_function";
		}

	}
}