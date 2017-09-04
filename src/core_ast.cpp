#include "core_ast.h"
#include <vector>

#define AST_NODE std::variant<no_op, tuple, identifier, assignment, function_call, integer, string, function, conditional_branch, conditional_branch_path>

namespace fe
{
	namespace core_ast
	{
		// No op

		no_op::no_op() : type(types::unset_type()) {}

		// Copy
		no_op::no_op(const no_op& other) : type(other.type) {}
		no_op& no_op::operator=(const no_op& other)
		{
			type = other.type;
			return *this;
		}

		// Move
		no_op::no_op(no_op&& other) : type(std::move(other.type)) {}
		no_op& no_op::operator=(no_op&& other)
		{
			this->type = std::move(other.type);
			return *this;
		}

		// Integer

		integer::integer(values::integer val) : type(types::integer_type()), value(val) {}
		
		// Copy
		integer::integer(const integer& other) : value(other.value), type(other.type) {}
		integer& integer::operator=(const integer& other)
		{
			this->value = other.value;
			this->type = other.type;
			return *this;
		}

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
		string& string::operator=(const string& other)
		{
			this->value = other.value;
			this->type = other.type;
			return *this;
		}


		// Move
		string::string(string&& other) : value(std::move(other.value)), type(std::move(other.type)) {}
		string& string::operator=(string&& other)
		{
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}

		// Function


		function::function(std::optional<identifier>&& name, std::vector<identifier>&& parameters, std::unique_ptr<AST_NODE>&& body, types::type t) : name(std::move(name)), parameters(std::move(parameters)), type(t), body(std::move(body)) {}
		
		// Copy
		function::function(const function& other) : name(other.name), parameters(other.parameters), body(core_ast::make_unique(*other.body)), type(other.type) {}
		function& function::operator=(const function& other)
		{
			this->name = other.name;
			this->parameters = other.parameters;
			this->body = make_unique(*other.body);
			this->type = other.type;
			return *this;
		}

		// Move
		function::function(function&& other) : name(std::move(other.name)), parameters(std::move(other.parameters)), body(std::move(other.body)), type(std::move(other.type)) {}
		function& function::operator=(function&& other)
		{
			this->name = std::move(other.name);
			this->parameters = std::move(other.parameters);
			this->body = std::move(other.body);
			this->type = std::move(other.type);
			return *this;
		}


		// Tuple


		tuple::tuple(std::vector<AST_NODE> children, types::type t) : type(t), children(std::move(children)) {}
		
		// Copy
		tuple::tuple(const tuple& other) : children(other.children), type(other.type) {}
		tuple& tuple::operator=(const tuple& other)
		{
			this->children = other.children;
			this->type = other.type;
			return *this;
		}

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


		assignment::assignment(identifier&& id, std::unique_ptr<AST_NODE>&& val)
			: type(types::void_type()), id(std::move(id)), value(std::move(val)) {}

		// Copy
		assignment::assignment(const assignment& other) : id(other.id), value(std::make_unique<AST_NODE>(*other.value)), type(other.type) {}
		assignment& assignment::operator=(const assignment& other)
		{
			this->id = other.id;
			this->value = make_unique(*other.value);
			this->type = other.type;
			return *this;
		}
		
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


		function_call::function_call(identifier&& id, std::unique_ptr<AST_NODE>&& parameter, types::type&& t)
			: type(std::move(t)), id(std::move(id)), parameter(std::move(parameter)) {}
		
		// Copy
		function_call::function_call(const function_call& other) : id(other.id), parameter(make_unique(*other.parameter)), type(other.type) {}
		function_call& function_call::operator=(const function_call& other)
		{
			this->id = other.id;
			this->parameter = make_unique(*other.parameter);
			this->type = other.type;
			return *this;
		}

		// Move
		function_call::function_call(function_call&& other) : id(std::move(other.id)), parameter(std::move(other.parameter)), type(std::move(other.type)) {}
		function_call& function_call::operator=(function_call&& other)
		{
			this->id = std::move(other.id);
			this->parameter = std::move(other.parameter);
			this->type = std::move(other.type);
			return *this;
		}


		// Conditional Branch Path

		conditional_branch_path::conditional_branch_path(std::unique_ptr<AST_NODE> test, std::unique_ptr<AST_NODE> code) : test_path(std::move(test)), code_path(std::move(code)) {}

		// Copy
		conditional_branch_path::conditional_branch_path(const conditional_branch_path& other) : code_path(make_unique(*other.code_path)), test_path(make_unique(*other.test_path)), type(other.type) {}
		conditional_branch_path& conditional_branch_path::operator=(const conditional_branch_path& other)
		{
			this->test_path = make_unique(*other.test_path);
			this->code_path = make_unique(*other.code_path);
			this->type = other.type;
			return *this;
		}

		// Move
		conditional_branch_path::conditional_branch_path(conditional_branch_path&& other) : code_path(std::move(other.code_path)), test_path(std::move(other.test_path)), type(std::move(other.type)) {}
		conditional_branch_path& conditional_branch_path::operator=(conditional_branch_path&& other) {
			this->code_path = std::move(other.code_path);
			this->test_path = std::move(other.test_path);
			this->type = std::move(other.type);
			return *this;
		}

		// Conditional Branch


		conditional_branch::conditional_branch(std::vector<conditional_branch_path>&& branches, types::type&& t) : branches(std::move(branches)), type(t) {}

		// Copy
		conditional_branch::conditional_branch(const conditional_branch& other) : branches(other.branches), type(other.type) {}
		conditional_branch& conditional_branch::operator=(const conditional_branch& other)
		{
			this->branches = other.branches;
			this->type = other.type;
			return *this;
		}

		// Move
		conditional_branch::conditional_branch(conditional_branch&& other) : branches(std::move(other.branches)), type(std::move(other.type)) {}
		conditional_branch& conditional_branch::operator=(conditional_branch&& other)
		{
			this->branches = std::move(other.branches);
			this->type = std::move(other.type);
			return *this;
		}
	}
}

#undef AST_NODE