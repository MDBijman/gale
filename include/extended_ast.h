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

		// Derivatives

		struct tuple : public node
		{
			tuple() : node(types::unset_type()) {}
			tuple(tuple&& other) : node(std::move(other)), children(std::move(other.children)) {}
			tuple& operator=(tuple&& other) 
			{
				this->children = std::move(other.children);
				this->type = other.type;
				return *this;
			}

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
				types::product_type new_type;
				for (decltype(auto) child : children)
				{
					new_type.product.push_back(child->type);
				}
				type = new_type;
			}

			std::vector<std::unique_ptr<node>> children;
		};

		struct identifier : public node
		{
			identifier(std::vector<std::string>&& name) : name(std::move(name)) {}
			identifier(identifier&& other) : node(std::move(other)), name(std::move(other.name)) {}

			std::vector<std::string> name;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, std::unique_ptr<node>&& val) : node(types::unset_type()), id(std::move(id)), value(std::move(val)) {}
			assignment(assignment&& other) : node(std::move(other)), id(std::move(other.id)), value(std::move(other.value)) {}

			identifier id;
			std::unique_ptr<node> value;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, tuple&& params) : id(std::move(id)), params(std::move(params)) {}
			function_call(function_call&& other) : node(std::move(other)), id(std::move(other.id)), params(std::move(other.params)) { }

			identifier id;
			tuple params;
		};

		struct type_declaration : public node
		{
			type_declaration(identifier&& name, tuple&& types) : id(std::move(name)), types(std::move(types)), node(types::void_type{}) {}
			type_declaration(type_declaration&& other) : node(std::move(other)), id(std::move(other.id)), types(std::move(other.types)) {}

			identifier id;
			tuple types;
		};

		struct export_stmt : public node
		{
			export_stmt(identifier name) : name(std::move(name)) {}
			export_stmt(export_stmt&& other) : node(std::move(other)), name(std::move(other.name)) {}

			identifier name;
		};

		// Value nodes

		struct integer : public node
		{
			integer(values::integer val) : value(val) {}
			integer(integer&& other) : node(std::move(other)), value(std::move(other.value)) {}

			values::integer value;
		};

		struct string : public node
		{
			string(values::string val) : value(val) {}
			string(string&& other) : node(std::move(other)), value(std::move(other.value)) {}

			values::string value;
		};

		using node_p = std::unique_ptr<node>;
	}
}