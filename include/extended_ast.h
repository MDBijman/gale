#pragma once
#include <vector>
#include <string>
#include <memory>

#include "values.h"
#include "types.h"

namespace fe
{
	namespace extended_ast
	{
		struct node 
		{
			virtual ~node() {}
			node() : type(types::unset_type()) {}
			node(types::type type) : type(type) {}
			node(node&& other) : type(std::move(other.type)) {}

			types::type type;
		};

		// Value nodes

		struct integer : public node
		{
			integer(values::integer val) : value(val) {}
			integer(integer&& other) : node(std::move(other)), value(std::move(other.value)) {}
			integer& operator=(integer&& other)
			{
				type = other.type;
				value = std::move(other.value);
				return *this;
			}

			values::integer value;
		};

		struct string : public node
		{
			string(values::string val) : value(val) {}
			string(string&& other) : node(std::move(other)), value(std::move(other.value)) {}
			string& operator=(string&& other)
			{
				type = other.type;
				value = std::move(other.value);
				return *this;
			}

			values::string value;
		};

		struct identifier : public node
		{
			identifier(std::vector<std::string>&& name) : name(std::move(name)) {}
			identifier(identifier&& other) : node(std::move(other)), name(std::move(other.name)) {}
			identifier& operator=(identifier&& other)
			{
				type = other.type;
				name = std::move(other.name);
				return *this;
			}

			std::vector<std::string> name;
		};

		struct export_stmt : public node
		{
			export_stmt(identifier name) : name(std::move(name)) {}
			export_stmt(export_stmt&& other) : node(std::move(other)), name(std::move(other.name)) {}
			export_stmt& operator=(export_stmt&& other)
			{
				type = other.type;
				name = std::move(other.name);
				return *this;
			}

			identifier name;
		};

		struct assignment;
		struct function_call;
		struct type_declaration;
		struct value_tuple;

		struct type_tuple : public node
		{
			type_tuple(std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>>&& children) : node(types::unset_type()), children(std::move(children)) {}
			type_tuple(type_tuple&& other) : node(std::move(other)), children(std::move(other.children)), value(other.value) {}
			type_tuple& operator=(type_tuple&& other) 
			{
				type = other.type;
				value = other.value;
				children = std::move(other.children);
				return *this;
			}

			std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>> children;
			types::type value;
		};

		struct value_tuple : public node
		{
			value_tuple(std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>>&& children) : node(types::unset_type()), children(std::move(children)) {}
			value_tuple(value_tuple&& other) : node(std::move(other)), children(std::move(other.children)) {}
			value_tuple& operator=(value_tuple&& other) 
			{
				type = other.type;
				children = std::move(other.children);
				return *this;
			}

			std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>> children;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, value_tuple&& params) : id(std::move(id)), params(std::move(params)) {}
			function_call(function_call&& other) : node(std::move(other)), id(std::move(other.id)), params(std::move(other.params)) { }
			function_call& operator=(function_call&& other)
			{
				type = other.type;
				id = std::move(other.id);
				params = std::move(other.params);
				return *this;
			}

			identifier id;
			value_tuple params;
		};

		struct type_declaration : public node
		{
			type_declaration(identifier&& name, type_tuple&& types) : node(types::void_type{}), id(std::move(name)), types(std::move(types)) {}
			type_declaration(type_declaration&& other) : node(std::move(other)), id(std::move(other.id)), types(std::move(other.types)) {}
			type_declaration& operator=(type_declaration&& other)
			{
				type = other.type;
				id = std::move(other.id);
				types = std::move(other.types);
				return *this;
			}

			identifier id;
			type_tuple types;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>&& val) : node(types::unset_type()), id(std::move(id)), value(std::make_unique<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>>(std::move(val))) {}
			assignment(assignment&& other) : node(std::move(other)), id(std::move(other.id)), value(std::move(other.value)) {}
			assignment& operator=(assignment&& other)
			{
				type = other.type;
				id = std::move(other.id);
				value = std::move(other.value);
				return *this;
			}

			identifier id;
			std::unique_ptr<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>> value;
		};

		auto get_type = [](auto& node) {
			return node.type;
		};

		auto set_type = [](auto node, types::type t) {
			node.type = t;
		};

		using node_v = std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string>;
	}
}