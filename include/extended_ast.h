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

		enum class binary_operator
		{
			ADD, SUBTRACT, MULTIPLY, DIVIDE
		};

		struct type_tuple;
		struct value_tuple;
		struct function_call;
		struct assignment;
		struct type_declaration;

		struct bin_op 
		{
			bin_op(
				binary_operator operation,
				std::unique_ptr<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>>&& l,
				std::unique_ptr<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>>&& r
			) : left(std::move(l)), right(std::move(r)), operation(operation) {}
			bin_op(bin_op&& other) : type(std::move(other.type)), left(std::move(other.left)), right(std::move(other.right)), operation(other.operation) {}
			bin_op& operator=(bin_op&& other)
			{
				type = std::move(other.type);
				operation = other.operation;
				right = std::move(other.right);
				left = std::move(other.left);
				return *this;
			}

			binary_operator operation;
			std::unique_ptr<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>> left, right;
			types::type type;
		};

		struct type_tuple 
		{
			type_tuple(std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>>&& children) : type(types::unset_type()), children(std::move(children)) {}
			type_tuple(type_tuple&& other) : type(std::move(other.type)), children(std::move(other.children)), value(std::move(other.value)) {}
			type_tuple& operator=(type_tuple&& other) 
			{
				type = std::move(other.type);
				value = std::move(other.value);
				children = std::move(other.children);
				return *this;
			}

			std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>> children;
			types::type value;
			types::type type;
		};

		struct value_tuple
		{
			value_tuple(std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>>&& children) : type(types::unset_type()), children(std::move(children)) {}
			value_tuple(value_tuple&& other) : type(std::move(other.type)), children(std::move(other.children)) {}
			value_tuple& operator=(value_tuple&& other) 
			{
				type = std::move(other.type);
				children = std::move(other.children);
				return *this;
			}

			std::vector<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>> children;
			types::type type;
		};

		struct function_call 
		{
			function_call(identifier&& id, value_tuple&& params) : id(std::move(id)), params(std::move(params)) {}
			function_call(function_call&& other) : type(std::move(other.type)), id(std::move(other.id)), params(std::move(other.params)) { }
			function_call& operator=(function_call&& other)
			{
				type = std::move(other.type);
				id = std::move(other.id);
				params = std::move(other.params);
				return *this;
			}

			identifier id;
			value_tuple params;
			types::type type;
		};

		struct type_declaration 
		{
			type_declaration(identifier&& name, type_tuple&& types) : type(types::void_type{}), id(std::move(name)), types(std::move(types)) {}
			type_declaration(type_declaration&& other) : type(std::move(other.type)), id(std::move(other.id)), types(std::move(other.types)) {}
			type_declaration& operator=(type_declaration&& other)
			{
				type = std::move(other.type);
				id = std::move(other.id);
				types = std::move(other.types);
				return *this;
			}

			identifier id;
			type_tuple types;
			types::type type;
		};

		struct assignment 
		{
			assignment(identifier&& id, std::unique_ptr<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>>&& val) : type(types::unset_type()), id(std::move(id)), value(std::move(val)) {}
			assignment(assignment&& other) : type(std::move(other.type)), id(std::move(other.id)), value(std::move(other.value)) {}
			assignment& operator=(assignment&& other)
			{
				type = std::move(other.type);
				id = std::move(other.id);
				value = std::move(other.value);
				return *this;
			}

			identifier id;
			std::unique_ptr<std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>> value;
			types::type type;
		};

		auto get_type = [](auto& node) -> types::type {
			return node.type;
		};

		auto set_type = [](auto& node, types::type t) -> void {
			node.type = t;
		};

		using node = std::variant<type_tuple, value_tuple, identifier, assignment, function_call, type_declaration, export_stmt, integer, string, bin_op>;
		using unique_node = std::unique_ptr<node>;

		auto make_unique = [](auto&& x) {
			return std::make_unique<node>(std::move(x));
		};
	}
}