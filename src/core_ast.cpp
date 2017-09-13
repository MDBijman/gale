#include "core_ast.h"
#include <vector>

#define AST_NODE std::variant<identifier, no_op, set, tuple, block, function_call, integer, string, function, conditional_branch, conditional_branch_path>

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

		integer::integer(values::integer val) : type(types::atom_type("i32")), value(val) {}
		
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


		string::string(values::string val) : type(types::atom_type("str")), value(val) {}
		
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

		function::function(std::optional<identifier>&& name, std::variant<std::vector<identifier>, identifier>&& parameters, std::unique_ptr<AST_NODE>&& body, types::type t) : name(std::move(name)), parameters(std::move(parameters)), type(t), body(std::move(body)) {}
		
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


		// Get


		//get::get(identifier&& modules, std::string variable, accessor field_offsets, types::type t) : modules(std::move(modules)), variable(variable), field_offsets(field_offsets), type(t) {};

		//// Copy
		//get::get(const get& other) : modules(other.modules), variable(other.variable), field_offsets(other.field_offsets), type(other.type) {};
		//get& get::operator=(const get& other)
		//{
		//	this->modules = other.modules;
		//	this->variable = other.variable;
		//	this->field_offsets = other.field_offsets;
		//	this->type = other.type;
		//	return *this;
		//}

		//// Move
		//get::get(get&& other) : modules(std::move(other.modules)), variable(std::move(other.variable)), field_offsets(std::move(other.field_offsets)), type(std::move(other.type)) {};
		//get& get::operator=(get&& other)
		//{
		//	this->modules = std::move(other.modules);
		//	this->variable = std::move(other.variable);
		//	this->field_offsets = std::move(other.field_offsets);
		//	this->type = std::move(other.type);
		//	return *this;
		//}


		// Set

		set::set(identifier&& id, std::unique_ptr<AST_NODE>&& value, types::type t) : id(std::move(id)), value(std::move(value)), type(t) {};

		// Copy
		set::set(const set& other) : id(other.id), value(std::make_unique<AST_NODE>(*other.value)), type(other.type) {};
		set& set::operator=(const set& other)
		{
			this->id = other.id;
			this->type = other.type;
			this->value = std::make_unique<AST_NODE>(*other.value);
			return *this;
		}

		// Move
		set::set(set&& other) : id(std::move(other.id)), value(std::move(other.value)), type(std::move(other.type)) {};
		set& set::operator=(set&& other)
		{
			this->id = std::move(other.id);
			this->type = std::move(other.type);
			this->value = std::move(other.value);
			return *this;
		}


		// Block

		block::block(std::vector<AST_NODE> children, types::type t) : type(t), children(std::move(children)) {}

		// Copy
		block::block(const block& other) : children(other.children), type(other.type) {}
		block& block::operator=(const block& other)
		{
			this->children = other.children;
			this->type = other.type;
			return *this;
		}

		// Move
		block::block(block&& other) : type(std::move(other.type)), children(std::move(other.children)) {}
		block& block::operator=(block&& other)
		{
			this->type = std::move(other.type);
			this->children = std::move(other.children);
			return *this;
		}


		// Identifier


		identifier::identifier(std::vector<std::string>&& modules, std::string&& name, std::vector<int>&& offsets) : modules(std::move(modules)), variable_name(std::move(name)), offsets(std::move(offsets)) {};

		// Copy
		identifier::identifier(const identifier& other) : modules(other.modules), variable_name(other.variable_name), offsets(other.offsets), type(other.type) {}
		identifier& identifier::operator=(const identifier& other)
		{
			this->modules = other.modules;
			this->variable_name = other.variable_name;
			this->offsets = other.offsets;
			this->type = other.type;
			return *this;
		}

		// Move
		identifier::identifier(identifier&& other) : modules(std::move(other.modules)), variable_name(std::move(other.variable_name)), offsets(std::move(other.offsets)), type(std::move(other.type)) {}
		identifier& identifier::operator=(identifier&& other)
		{
			this->modules = std::move(other.modules);
			this->variable_name = std::move(other.variable_name);
			this->offsets = std::move(other.offsets);
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