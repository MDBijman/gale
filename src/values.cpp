#include "values.h"
#include "core_ast.h"

#define VALUE_NODE std::variant<void_value, string, integer, boolean, tuple, function, native_function, module>

namespace fe
{
	namespace values
	{
		// Function


		function::function(std::unique_ptr<core_ast::function> func) : func(std::move(func)) {}

		// Copy
		function::function(const function& other) : func(std::make_unique<core_ast::function>(*other.func)) {}
		function& function::operator=(const function& other)
		{
			this->func = std::make_unique<core_ast::function>(*other.func);
			return *this;
		}

		// Move
		function::function(function&& other) : func(std::move(other.func)) {}
		function& function::operator=(function&& other)
		{
			this->func = std::move(other.func);
			return *this;
		}

		std::string function::to_string()
		{
			return "function";
		}

		// Tuple


		tuple::tuple() {}
		tuple::tuple(std::vector<VALUE_NODE> values) : content(values) {}

		// Copy
		tuple::tuple(const tuple& other) : content(other.content) {}
		tuple& tuple::operator=(const tuple& other)
		{
			this->content = other.content;
			return *this;
		}

		// Move
		tuple::tuple(tuple&& other) : content(std::move(other.content)) {}
		tuple& tuple::operator=(tuple&& other)
		{
			this->content = std::move(other.content);
			return *this;
		}

		std::string tuple::to_string()
		{
			std::string r("tuple (");

			for (auto it = content.begin(); it != content.end(); it++)
			{
				r.append(std::visit(values::to_string, *it));
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

		std::string native_function::to_string()
		{
			return "native_function";
		}

	}	
}

#undef VALUE_NODE