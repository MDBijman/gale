#pragma once
#include "types.h"
#include "values.h"
#include <vector>

namespace fe
{
	namespace core_ast
	{
		struct node 
		{
			virtual ~node() {}
			node(types::type type) : type(type) {}
			node(node&& other) : type(std::move(other.type)) {}

			types::type type;
		};

		// Derivatives

		struct tuple : public node
		{
			tuple(tuple&& other) : children(std::move(other.children)), node(std::move(other)) {}
			tuple(std::vector<std::unique_ptr<node>> children) : children(std::move(children)), node(types::void_type()) {}

			std::vector<std::unique_ptr<node>> children;
		};

		struct identifier : public node
		{
			identifier(std::string&& name, types::type t) : node(t), name(std::move(name)) {}
			identifier(identifier&& other) : name(std::move(other.name)), node(std::move(other)) {}

			std::string name;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, std::unique_ptr<node>&& val) 
				: id(std::move(id)), value(std::move(val)), node(types::void_type()) {}

			identifier id;
			std::unique_ptr<node> value;
		};
		struct function_call : public node
		{
			function_call(identifier&& id, tuple&& params, types::type t)
				: id(std::move(id)), params(std::move(params)), node(t) {}

			identifier id;
			tuple params;
		};

		struct export_stmt : public node
		{
			export_stmt(identifier&& name) : node(types::unset_type()), name(std::move(name)) {}

			identifier name;
		};

		// Value nodes

		struct integer : public node
		{
			integer(values::integer val) : value(val), node(types::integer_type()) {}

			values::integer value;
		};

		struct string : public node
		{
			string(values::string val) : value(val), node(types::string_type()) {}
			values::string value;
		};
	}
}