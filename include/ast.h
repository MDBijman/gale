#pragma once
#include <vector>
#include <string>
#include <memory>

#include "values.h"

namespace fe
{
	namespace ast
	{
		struct node 
		{
			virtual ~node() {}
			node(types::type* type) : type(type) {}
			node(std::unique_ptr<types::type>&& type) : type(std::move(type)) {}
			node() : type(new types::void_type()) {}

			std::unique_ptr<types::type> type;
		};

		// Derivatives

		struct node_list : public node
		{
			std::vector<std::unique_ptr<node>> children;
		};

		struct identifier : public node
		{
			identifier(const std::string& name) :name(name) {}
			identifier(std::string&& name) : name(std::move(name)) {}
			identifier(identifier&& other) : name(std::move(other.name)) {}

			std::string name;
		};

		struct type_identifier : public node
		{
			type_identifier(const std::string& name) : name(name) {}
			std::string name;
		};

		struct assignment : public node
		{
			assignment(std::unique_ptr<identifier>&& id, std::unique_ptr<identifier>&& type, std::unique_ptr<node>&& val) 
				: id(std::move(id)), value(std::move(val)), type(std::move(type)) {}

			std::unique_ptr<identifier> id;
			std::unique_ptr<identifier> type;
			std::unique_ptr<node> value;
		};

		struct function_call : public node
		{
			function_call(identifier id, std::vector<std::unique_ptr<node>> params) : id(std::move(id)), params(std::move(params)) {}
			identifier id;
			std::vector<std::unique_ptr<node>> params;
		};

		// Value nodes

		struct tuple : public node
		{
			std::vector<std::unique_ptr<node>> children;
		};

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