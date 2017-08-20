#include "values.h"
#include "core_ast.h"

namespace fe
{
	namespace values
	{
		// Function


		function::function(std::vector<core_ast::identifier> params, core_ast::unique_node body) : parameters(std::move(params)), body(std::move(body)) {}

		// Copy
		function::function(const function& other) : parameters(other.parameters), body(std::make_unique<core_ast::node>(*other.body)) {}
		function& function::operator=(const function& other)
		{
			this->parameters = other.parameters;
			this->body = std::make_unique<core_ast::node>(*other.body);
			return *this;
		}

		// Move
		function::function(function&& other) : parameters(std::move(other.parameters)), body(std::move(other.body)) {}
		function& function::operator=(function&& other)
		{
			this->parameters = std::move(other.parameters);
			this->body = std::move(other.body);
			return *this;
		}

		std::string function::to_string()
		{
			return "function";
		}

		// Tuple


		tuple::tuple() {}
		tuple::tuple(std::vector<std::variant<string, integer, tuple, function, native_function, module, type>> values) : content(values) {}

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