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
			tuple(tuple&& other) : node(std::move(other)), children(std::move(other.children)) {}
			tuple(std::vector<std::unique_ptr<node>> children, types::type t) : node(t), children(std::move(children)) {}

			std::vector<std::unique_ptr<node>> children;
		};

		struct identifier : public node
		{
			identifier(std::vector<std::string>&& name, types::type t) : node(t), name(std::move(name)) {}
			identifier(std::string&& name, types::type t) : node(t), name({ name }) {}
			identifier(identifier&& other) : node(std::move(other)), name(std::move(other.name)) {}

			std::vector<std::string> name;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, std::unique_ptr<node>&& val) 
				: node(types::void_type()), id(std::move(id)), value(std::move(val)) {}

			identifier id;
			std::unique_ptr<node> value;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, tuple&& params, types::type t)
				: node(t), id(std::move(id)), params(std::move(params)) {}

			identifier id;
			tuple params;
		};

		// Value nodes

		struct integer : public node
		{
			integer(values::integer val) : node(types::integer_type()), value(val) {}
			values::integer value;
		};

		struct string : public node
		{
			string(values::string val) : node(types::string_type()), value(val) {}
			values::string value;
		};

		struct function : public node
		{
			function(values::function&& fun, types::type t) : node(t), fun(std::move(fun)) {}
			values::function fun;
		};
	}
}