#include "core_ast.h"
#include <vector>

namespace fe
{
	namespace core_ast
	{
		// Integer

		integer::integer(values::integer val) : type(types::integer_type()), value(val) {}
		
		// Copy
		integer::integer(const integer& other) : value(other.value), type(other.type) {}

		// Move
		integer::integer(integer&& other) : value(std::move(other.value)), type(std::move(other.type)) {}
		integer& integer::operator=(integer&& other)
		{
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}


		// String


		string::string(values::string val) : type(types::string_type()), value(val) {}
		
		// Copy
		string::string(const string& other) : value(other.value), type(other.type) {}

		// Move
		string::string(string&& other) : value(std::move(other.value)), type(std::move(other.type)) {}
		string& string::operator=(string&& other)
		{
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}

		// Function


		function::function(values::function&& fun, types::type t) : type(t), fun(std::move(fun)) {}
		
		// Copy
		function::function(const function& other) : fun(other.fun), type(other.type) {}

		// Move
		function::function(function&& other) : fun(std::move(other.fun)), type(std::move(other.type)) {}
		function& function::operator=(function&& other)
		{
			this->fun = std::move(other.fun);
			this->type = std::move(other.type);
			return *this;
		}


		// Tuple


		tuple::tuple(std::vector<std::variant<tuple, identifier, assignment, function_call, integer, string, function>> children, types::type t) : type(t), children(std::move(children)) {}
		
		// Copy
		tuple::tuple(const tuple& other) : children(other.children), type(other.type) {}

		// Move
		tuple::tuple(tuple&& other) : type(std::move(other.type)), children(std::move(other.children)) {}
		tuple& tuple::operator=(tuple&& other)
		{
			this->type = std::move(other.type);
			this->children = std::move(other.children);
			return *this;
		}


		// Identifier

		identifier::identifier(std::vector<std::string>&& name, types::type t) : type(t), name(std::move(name)) {}
		identifier::identifier(std::string&& name, types::type t) : type(t), name({ name }) {}

		// Copy
		identifier::identifier(const identifier& other) : name(other.name), type(other.type) {}
		identifier& identifier::operator=(const identifier& other)
		{
			this->name = other.name;
			this->type = other.type;
			return *this;
		}

		// Move
		identifier::identifier(identifier&& other) : type(std::move(other.type)), name(std::move(other.name)) {}
		identifier& identifier::operator=(identifier&& other)
		{
			this->name = std::move(other.name);
			this->type = std::move(other.type);
			return *this;
		}


		// Assignment


		assignment::assignment(identifier&& id, std::unique_ptr<std::variant<tuple, identifier, assignment, function_call, integer, string, function>>&& val)
			: type(types::void_type()), id(std::move(id)), value(std::move(val)) {}

		// Copy
		assignment::assignment(const assignment& other) : id(other.id), value(std::make_unique<std::variant<tuple, identifier, assignment, function_call, integer, string, function>>(*other.value)), type(other.type) {}

		// Move
		assignment::assignment(assignment&& other) : id(std::move(other.id)), value(std::move(other.value)), type(std::move(other.type)) {}
		assignment& assignment::operator=(assignment&& other)
		{
			this->id = std::move(other.id);
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}


		// Function Call


		function_call::function_call(identifier&& id, tuple&& params, types::type&& t)
			: type(std::move(t)), id(std::move(id)), params(std::move(params)) {}
		
		// Copy
		function_call::function_call(const function_call& other) : id(other.id), params(other.params), type(other.type) {}

		// Move
		function_call::function_call(function_call&& other) : id(std::move(other.id)), params(std::move(other.params)), type(std::move(other.type)) {}
		function_call& function_call::operator=(function_call&& other)
		{
			this->id = std::move(other.id);
			this->params = std::move(other.params);
			this->type = std::move(other.type);
			return *this;
		}

	}
}