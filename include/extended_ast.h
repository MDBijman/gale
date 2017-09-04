#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional>

#include "values.h"
#include "types.h"

#define AST_NODE std::variant<module_declaration, atom_type, function_type, tuple_type, atom_declaration, function_declaration, tuple_declaration, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, function, conditional_branch, conditional_branch_path>

namespace fe
{
	namespace extended_ast
	{
		// Value nodes

		struct integer 
		{
			integer(values::integer val) : value(val) {}
			integer(integer&& other) : type(std::move(other.type)), value(std::move(other.value)) {}
			integer& operator=(integer&& other)
			{
				type = std::move(other.type);
				value = std::move(other.value);
				return *this;
			}

			values::integer value;
			types::type type;
		};

		struct string 
		{
			string(values::string val) : value(val) {}
			string(string&& other) : type(std::move(other.type)), value(std::move(other.value)) {}
			string& operator=(string&& other)
			{
				type = std::move(other.type);
				value = std::move(other.value);
				return *this;
			}

			values::string value;
			types::type type;
		};

		struct identifier 
		{
			identifier(std::vector<std::string>&& name) : name(std::move(name)) {}
			identifier(const identifier& other) : name(other.name), type(other.type) {}

			identifier(identifier&& other) : type(std::move(other.type)), name(std::move(other.name)) {}
			identifier& operator=(identifier&& other)
			{
				type = std::move(other.type);
				name = std::move(other.name);
				return *this;
			}

			std::vector<std::string> name;
			types::type type;
		};

		struct export_stmt 
		{
			export_stmt(identifier name) : name(std::move(name)) {}
			export_stmt(export_stmt&& other) : type(std::move(other.type)), name(std::move(other.name)) {}
			export_stmt& operator=(export_stmt&& other)
			{
				type = std::move(other.type);
				name = std::move(other.name);
				return *this;
			}

			identifier name;
			types::type type;
		};

		struct module_declaration
		{
			module_declaration(identifier&& name) : name(std::move(name)) {}
			module_declaration(module_declaration&& other) : type(std::move(other.type)), name(std::move(other.name)) {}
			module_declaration& operator=(module_declaration&& other)
			{
				this->name = std::move(other.name);
				this->type = std::move(other.type);
				return *this;
			}

			identifier name;
			types::type type;
		};

		struct value_tuple;
		struct function_call;
		struct assignment;
		struct type_declaration;
		struct function;
		struct conditional_branch;
		struct conditional_branch_path;

		struct atom_type
		{
			atom_type(identifier&& name) : name(std::move(name)), type(types::unset_type()) {}
			atom_type(atom_type&& other) : name(std::move(other.name)), type(std::move(other.type)) {}
			atom_type& operator=(atom_type&& other)
			{
				this->name = std::move(other.name);
				this->type = std::move(other.type);
				return *this;
			}

			identifier name;
			types::type type;
		};
		
		struct function_type;

		struct tuple_type
		{
			tuple_type(std::vector<std::variant<atom_type, function_type>>&& elements) : elements(std::move(elements)), type(types::unset_type()) {}
			tuple_type(tuple_type&& other) : elements(std::move(other.elements)), type(std::move(other.type)) {}
			tuple_type& operator=(tuple_type&& other)
			{
				this->elements = std::move(other.elements);
				this->type = std::move(other.type);
				return *this;
			}

			std::vector<std::variant<atom_type, function_type>> elements;
			types::type type;
		};

		struct function_type
		{
			function_type(tuple_type&& from, tuple_type&& to) : from(std::move(from)), to(std::move(to)), type(types::unset_type()) {}
			function_type(function_type&& other) : from(std::move(other.from)), to(std::move(other.to)), type(std::move(other.type)) {}
			function_type& operator=(function_type&& other)
			{
				this->from = std::move(other.from);
				this->to = std::move(other.to);
				this->type = std::move(other.type);
				return *this;
			}

			tuple_type from;
			tuple_type to;
			types::type type;
		};

		struct atom_declaration
		{
			atom_declaration(atom_type&& type_name, identifier&& name) : type_name(std::move(type_name)), name(std::move(name)), type(types::unset_type()) {}
			atom_declaration(atom_declaration&& other) : type_name(std::move(other.type_name)), name(std::move(other.name)), type(std::move(other.type)) {}
			atom_declaration& operator=(atom_declaration&& other)
			{
				this->type_name = std::move(other.type_name);
				this->name = std::move(other.name);
				this->type = std::move(other.type);
				return *this;
			}

			atom_type type_name;
			identifier name;
			types::type type;
		};

		struct function_declaration
		{
			function_declaration(function_type&& type_name, identifier&& name) : type_name(std::move(type_name)), name(std::move(name)), type(types::unset_type()) {}
			function_declaration(function_declaration&& other) : type_name(std::move(other.type_name)), name(std::move(other.name)), type(std::move(other.type)) {}
			function_declaration& operator=(function_declaration&& other)
			{
				this->type_name = std::move(other.type_name);
				this->name = std::move(other.name);
				this->type = std::move(other.type);
				return *this;
			}

			function_type type_name;
			identifier name;
			types::type type;
		};

		struct tuple_declaration
		{
			tuple_declaration(std::vector<std::variant<atom_declaration, function_declaration>>&& elements) : elements(std::move(elements)), type(types::unset_type()) {}
			tuple_declaration(tuple_declaration&& other) : elements(std::move(other.elements)), type(std::move(other.type)) {}
			tuple_declaration& operator=(tuple_declaration&& other)
			{
				this->elements = std::move(other.elements);
				this->type = std::move(other.type);
				return *this;
			}

			std::vector<std::variant<atom_declaration, function_declaration>> elements;
			types::type type;
		};

		struct value_tuple
		{
			value_tuple(std::vector<AST_NODE>&& children) : type(types::unset_type()), children(std::move(children)) {}
			value_tuple(value_tuple&& other) : type(std::move(other.type)), children(std::move(other.children)) {}
			value_tuple& operator=(value_tuple&& other) 
			{
				type = std::move(other.type);
				children = std::move(other.children);
				return *this;
			}

			std::vector<AST_NODE> children;
			types::type type;
		};

		struct function_call 
		{
			function_call(identifier&& id, std::unique_ptr<AST_NODE>&& params) : id(std::move(id)), params(std::move(params)) {}
			function_call(function_call&& other) : type(std::move(other.type)), id(std::move(other.id)), params(std::move(other.params)) { }
			function_call& operator=(function_call&& other)
			{
				type = std::move(other.type);
				id = std::move(other.id);
				params = std::move(other.params);
				return *this;
			}

			identifier id;
			std::unique_ptr<AST_NODE> params;
			types::type type;
		};

		struct type_declaration 
		{
			type_declaration(identifier&& name, tuple_type&& types) : type(types::void_type{}), id(std::move(name)), types(std::move(types)) {}
			type_declaration(type_declaration&& other) : type(std::move(other.type)), id(std::move(other.id)), types(std::move(other.types)) {}
			type_declaration& operator=(type_declaration&& other)
			{
				type = std::move(other.type);
				id = std::move(other.id);
				types = std::move(other.types);
				return *this;
			}

			identifier id;
			tuple_type types;
			types::type type;
		};

		struct assignment 
		{
			assignment(identifier&& id, std::unique_ptr<AST_NODE>&& val) : type(types::unset_type()), id(std::move(id)), value(std::move(val)) {}
			assignment(assignment&& other) : type(std::move(other.type)), id(std::move(other.id)), value(std::move(other.value)) {}
			assignment& operator=(assignment&& other)
			{
				type = std::move(other.type);
				id = std::move(other.id);
				value = std::move(other.value);
				return *this;
			}

			identifier id;
			std::unique_ptr<AST_NODE> value;
			types::type type;
		};

		struct function
		{
			function(std::optional<identifier>&& name, tuple_declaration&& from, std::unique_ptr<AST_NODE>&& to, std::unique_ptr<AST_NODE> body) : type(types::unset_type()), name(std::move(name)), from(std::move(from)), to(std::move(to)), body(std::move(body)) {}
			function(function&& other) : type(std::move(other.type)), name(std::move(other.name)), from(std::move(other.from)), to(std::move(other.to)), body(std::move(other.body)) {}
			function& operator=(function&& other)
			{
				type = std::move(other.type);
				name = std::move(other.name);
				from = std::move(other.from);
				to = std::move(other.to);
				body = std::move(other.body);
				return *this;
			}

			// Name is set when the function is not anonymous, for recursion
			std::optional<identifier> name;
			tuple_declaration from;
			std::unique_ptr<AST_NODE> to;
			std::unique_ptr<AST_NODE> body;

			types::type type;
		};

		struct conditional_branch_path
		{
			conditional_branch_path(std::unique_ptr<AST_NODE> test, std::unique_ptr<AST_NODE> code) : test_path(std::move(test)), code_path(std::move(code)) {}
			conditional_branch_path(conditional_branch_path&& other) : code_path(std::move(other.code_path)), test_path(std::move(other.test_path)), type(std::move(other.type)) {}
			conditional_branch_path& operator=(conditional_branch_path&& other) {
				this->code_path = std::move(other.code_path);
				this->test_path = std::move(other.test_path);
				this->type = std::move(other.type);
				return *this;
			}
			std::unique_ptr<AST_NODE> test_path;
			std::unique_ptr<AST_NODE> code_path;
			types::type type;
		};

		struct conditional_branch
		{
			conditional_branch(std::vector<conditional_branch_path>&& branches) : branches(std::move(branches)) {}
			conditional_branch(conditional_branch&& other) : branches(std::move(other.branches)), type(std::move(other.type)) {}
			conditional_branch& operator=(conditional_branch&& other) {
				this->branches = std::move(other.branches);
				this->type = std::move(other.type);
				return *this;
			}

			std::vector<conditional_branch_path> branches;
			types::type type;
		};

		auto get_type = [](auto& node) -> types::type {
			return node.type;
		};

		auto set_type = [](auto& node, types::type t) -> void {
			node.type = t;
		};

		using node = AST_NODE;
		using unique_node = std::unique_ptr<node>;

		auto make_unique = [](auto&& x) {
			return std::make_unique<node>(std::move(x));
		};
	}
}

#undef AST_NODE