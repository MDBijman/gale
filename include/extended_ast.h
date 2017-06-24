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
			node(types::type type) : type(type) {}

			types::type type;
		};

		// Derivatives

		struct tuple : public node
		{
			tuple(tuple&& other) : node(other.type), children(std::move(other.children)) {}
			tuple() : node(types::product_type({})) {}

			void add(std::unique_ptr<node> child)
			{
				children.push_back(std::move(child));
				update_type();
			}

			std::vector<std::unique_ptr<node>>& get_children()
			{
				return children;
			}

		private:
			void update_type()
			{
				types::product_type new_type({});
				for (decltype(auto) child : children)
				{
					new_type.product.push_back(child->type);
				}
			}

			std::vector<std::unique_ptr<node>> children;
		};

		struct identifier : public node
		{
			identifier(std::string&& name, types::type type) : node(type), name(std::move(name)) {}
			identifier(identifier&& other) : node(other.type), name(std::move(other.name)) {}

			std::string name;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, std::unique_ptr<node>&& val) : node(val->type), id(std::move(id)), value(std::move(val)) {}

			identifier id;
			std::unique_ptr<node> value;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, tuple&& params, types::product_type return_type) : node{ types::function_type{ std::get<1>(params.type), return_type } }, id(std::move(id)), params(std::move(params)) {}
			function_call(function_call&& other) : node(std::move(other)), id(std::move(other.id)), params(std::move(other.params)) { }

			identifier id;
			tuple params;
		};

		// Value nodes

		struct constructor : public node
		{
			constructor(identifier&& type, tuple&& value) : node(value.type), id(std::move(type)), value(std::move(value)) {}

			identifier id;
			tuple value;
		};

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

		using node_p = std::unique_ptr<node>;
	}
}