#include "core_ast.h"
#include <vector>

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

		function::function(std::optional<identifier>&& name, std::variant<std::vector<identifier>, identifier>&& parameters, std::unique_ptr<node>&& body, types::type t) : name(std::move(name)), parameters(std::move(parameters)), type(t), body(std::move(body)) {}
		
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


		tuple::tuple(std::vector<node> children, types::type t) : type(t), children(std::move(children)) {}
		
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

		// Set

		set::set(identifier&& id, std::unique_ptr<node>&& value, types::type t) : id(std::move(id)), value(std::move(value)), type(t) {};

		// Copy
		set::set(const set& other) : id(other.id), value(std::make_unique<node>(*other.value)), type(other.type) {};
		set& set::operator=(const set& other)
		{
			this->id = other.id;
			this->type = other.type;
			this->value = std::make_unique<node>(*other.value);
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

		block::block(std::vector<node> children, types::type t) : type(t), children(std::move(children)) {}

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


		function_call::function_call(identifier&& id, std::unique_ptr<node>&& parameter, types::type&& t)
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


		// Branch

		branch::branch(unique_node test, unique_node true_path, unique_node false_path) : test_path(std::move(test)), true_path(std::move(true_path)), false_path(std::move(false_path)) {}

		// Copy
		branch::branch(const branch& other) : 
			test_path(std::make_unique<node>(*other.test_path)), true_path(std::make_unique<node>(*other.true_path)), false_path(std::make_unique<node>(*other.false_path)), type(other.type) {}
		branch& branch::operator=(const branch& other)
		{
			this->test_path = std::make_unique<node>(*other.test_path);
			this->true_path = std::make_unique<node>(*other.true_path);
			this->false_path = std::make_unique<node>(*other.false_path);
			this->type = other.type;
			return *this;
		}

		// Move
		branch::branch(branch&& other) : test_path(std::move(other.test_path)), true_path(std::move(other.true_path)), false_path(std::move(other.false_path)), type(std::move(other.type)) {}
		branch& branch::operator=(branch&& other)
		{
			this->test_path = std::move(other.test_path);
			this->true_path = std::move(other.true_path);
			this->false_path = std::move(other.false_path);
			this->type = other.type;
			return *this;
		}
	}
}