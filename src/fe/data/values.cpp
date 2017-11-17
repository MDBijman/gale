#include "fe/data/values.h"
#include "fe/data/core_ast.h"

namespace fe
{
	namespace values
	{
		// Function

		function::function(core_ast::unique_node func) : func(std::move(func)) {}

		// Copy
		function::function(const function& other) : func(core_ast::unique_node(other.func->copy())) {}
		function& function::operator=(const function& other)
		{
			this->func = core_ast::unique_node(other.func->copy());
			return *this;
		}

		// Move
		function::function(function&& other) : func(std::move(other.func)) {}
		function& function::operator=(function&& other)
		{
			this->func = std::move(other.func);
			return *this;
		}

		std::string function::to_string() const
		{
			return "function";
		}

		// Tuple


		tuple::tuple() {}
		tuple::tuple(std::vector<unique_value> values) : content(std::move(values)) {}

		// Copy
		tuple::tuple(const tuple& other) 
		{
			for (decltype(auto) elem : other.content)
			{
				this->content.push_back(unique_value(elem->copy()));
			}
		}
		tuple& tuple::operator=(const tuple& other)
		{
			for (decltype(auto) elem : other.content)
			{
				this->content.push_back(unique_value(elem->copy()));
			}
			return *this;
		}

		// Move
		tuple::tuple(tuple&& other) : content(std::move(other.content)) {}
		tuple& tuple::operator=(tuple&& other)
		{
			this->content = std::move(other.content);
			return *this;
		}

		std::string tuple::to_string() const
		{
			std::string r("tuple (");

			for (auto it = content.begin(); it != content.end(); it++)
			{
				r.append(it->get()->to_string());
				if (it != content.end() - 1)
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