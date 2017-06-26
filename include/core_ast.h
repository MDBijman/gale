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
			node() : type(types::void_type()) {}


			types::type type;
		};

		// Derivatives

		struct tuple : public node
		{
			tuple(tuple&& other) : children(std::move(other.children)) {}
			tuple(std::vector<std::unique_ptr<node>> children) : children(std::move(children)) {}
			tuple() {}

			std::vector<std::unique_ptr<node>> children;
		};

		struct identifier : public node
		{
			identifier(std::string&& name) : name(std::move(name)) {}
			identifier(identifier&& other) : name(std::move(other.name)) {}

			std::string name;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, std::unique_ptr<node>&& val) 
				: id(std::move(id)), value(std::move(val)) {}

			identifier id;
			std::unique_ptr<node> value;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, tuple&& params) : id(std::move(id)), params(std::move(params)) {}

			identifier id;
			tuple params;
		};

		// Value nodes

		struct integer : public node
		{
			integer(values::integer val) : value(val) {}

			values::integer value;
		};

		struct string : public node
		{
			string(values::string val) : value(val) {}
			values::string value;
		};
	}
}