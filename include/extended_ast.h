#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional>

#include "values.h"
#include "types.h"
#include "tags.h"
#include "core_ast.h"

namespace fe
{
	class typecheck_environment;

	namespace extended_ast
	{
		struct node
		{
			node(types::type t) : type(t) {}
			virtual ~node()
			{

			}
			virtual node* copy() = 0;
			virtual void typecheck(typecheck_environment&) = 0;
			virtual core_ast::node lower() = 0;

			types::type get_type() const
			{
				return type;
			}

			void set_type(types::type t)
			{
				type = t;
			}

		private:
			types::type type;
		};

		struct type_expression
		{
			virtual types::type interpret() = 0;
		};

		using unique_node = std::unique_ptr<node>;

		// Value nodes

		struct integer : public node
		{
			integer(values::integer val) : node(types::unset_type()), value(val) {}

			node* copy() override
			{
				return new integer(*this);
			}

			void typecheck(typecheck_environment&) override;
			core_ast::node lower() override;

			// Copy
			integer(const integer& other) : node(other), value(other.value), tags(other.tags) {}

			// Move
			integer(integer&& other) : node(std::move(other)), value(std::move(other.value)), tags(std::move(other.tags)) {}
			integer& operator=(integer&& other)
			{
				set_type(other.get_type());
				value = std::move(other.value);
				tags = std::move(other.tags);
				return *this;
			}

			values::integer value;
			tags::tags tags;
		};

		struct string : public node
		{
			string(values::string val) : node(types::unset_type()), value(val) {}

			node* copy() override
			{
				return new string(*this);
			}

			void typecheck(typecheck_environment& t_env) override;
			core_ast::node lower() override;

			// Copy
			string(const string& other) : node(other), value(other.value), tags(other.tags) {}

			// Move
			string(string&& other) : node(std::move(other)), value(std::move(other.value)), tags(std::move(other.tags)) {}
			string& operator=(string&& other)
			{
				set_type(other.get_type());
				value = std::move(other.value);
				tags = std::move(other.tags);
				return *this;
			}

			values::string value;
			tags::tags tags;
		};

		struct identifier : public node
		{
			identifier(std::vector<std::string>&& modules) : node(types::unset_type()), segments(std::move(modules)) {}

			node* copy() override
			{
				return new identifier(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			identifier(const identifier& other) : node(other), segments(other.segments), offsets(other.offsets), tags(other.tags) {}

			// Move
			identifier(identifier&& other) : node(std::move(other)), segments(std::move(other.segments)), offsets(std::move(other.offsets)), tags(std::move(other.tags)) {}
			identifier& operator=(identifier&& other)
			{
				set_type(other.get_type());
				segments = std::move(other.segments);
				offsets = std::move(other.offsets);
				tags = std::move(other.tags);
				return *this;
			}

			identifier without_first_segment() const
			{
				auto new_id = identifier{
					std::vector<std::string>{ segments.begin() + 1, segments.end() }
				};
				new_id.set_type(this->get_type());
				new_id.offsets = offsets;
				return new_id;
			}

			std::string to_string() const
			{
				std::string res;
				for (auto segment : segments)
					res.append(segment + ", ");
				res.erase(res.size() - 2, 2);
				return res;
			}

			std::vector<std::string> segments;
			std::vector<int> offsets;
			tags::tags tags;
		};

		struct export_stmt : public node
		{
			export_stmt(std::vector<identifier> name) : node(types::unset_type()), names(std::move(names)) {}

			node* copy() override
			{
				return new export_stmt(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			export_stmt(const export_stmt& other) : node(other), names(other.names), tags(other.tags) {}

			// Move
			export_stmt(export_stmt&& other) : node(std::move(other)), names(std::move(other.names)), tags(std::move(other.tags)) {}
			export_stmt& operator=(export_stmt&& other)
			{
				set_type(other.get_type());
				this->names = std::move(other.names);
				this->tags = std::move(other.tags);
				return *this;
			}

			std::vector<identifier> names;
			tags::tags tags;
		};

		struct module_declaration : public node
		{
			module_declaration(identifier&& name) : node(types::unset_type()), name(std::move(name)) {}

			node* copy() override
			{
				return new module_declaration(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			module_declaration(const module_declaration& other) : node(other), name(other.name), tags(other.tags) {}

			// Move
			module_declaration(module_declaration&& other) : node(std::move(other)), name(std::move(other.name)), tags(std::move(other.tags)) {}
			module_declaration& operator=(module_declaration&& other)
			{
				set_type(other.get_type());
				this->name = std::move(other.name);
				this->tags = std::move(other.tags);
				return *this;
			}

			identifier name;
			tags::tags tags;
		};

		struct tuple;
		struct block;
		struct function_call;
		struct assignment;
		struct type_declaration;
		struct function;
		struct conditional_branch;
		struct conditional_branch_path;
		struct heap_allocation;

		struct atom_type : public node
		{
			atom_type(identifier&& name) : node(name), name(std::move(name)) {}

			node* copy() override
			{
				return new atom_type(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			atom_type(const atom_type& other) : node(other), name(other.name), tags(other.tags) {}

			// Move
			atom_type(atom_type&& other) : node(std::move(other)), name(std::move(other.name)), tags(std::move(other.tags)) {}
			atom_type& operator=(atom_type&& other)
			{
				set_type(other.get_type());
				this->name = std::move(other.name);
				this->tags = std::move(other.tags);
				return *this;
			}

			identifier name;
			tags::tags tags;
		};

		struct function_type;
		struct atom_declaration;

		struct tuple_type : public node
		{
			tuple_type(std::vector<std::variant<atom_type, function_type>>&& elements) : node(types::unset_type()), elements(std::move(elements)) {}

			node* copy() override
			{
				return new tuple_type(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			tuple_type(const tuple_type& other) : node(other), elements(other.elements), tags(other.tags) {}

			// Move
			tuple_type(tuple_type&& other) : node(std::move(other)), elements(std::move(other.elements)), tags(std::move(other.tags)) {}
			tuple_type& operator=(tuple_type&& other)
			{
				set_type(other.get_type());
				this->elements = std::move(other.elements);
				this->tags = std::move(other.tags);
				return *this;
			}

			std::vector<std::variant<atom_type, function_type>> elements;
			tags::tags tags;
		};

		struct function_type : public node
		{
			function_type(tuple_type&& from, tuple_type&& to) : node(types::unset_type()), from(std::move(from)), to(std::move(to)) {}

			node* copy() override
			{
				return new function_type(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			function_type(const function_type& other) : node(other), from(other.from), to(other.to), tags(other.tags) {}

			// Move
			function_type(function_type&& other) : node(std::move(other)), from(std::move(other.from)), to(std::move(other.to)), tags(std::move(other.tags)) {}
			function_type& operator=(function_type&& other)
			{
				set_type(other.get_type());
				this->from = std::move(other.from);
				this->to = std::move(other.to);
				tags = std::move(other.tags);
				return *this;
			}

			tuple_type from;
			tuple_type to;
			tags::tags tags;
		};

		struct atom_declaration : public node
		{
			atom_declaration(atom_type&& type_name, identifier&& name) : node(types::unset_type()), type_name(std::move(type_name)), name(std::move(name)) {}

			node* copy() override
			{
				return new atom_declaration(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			atom_declaration(const atom_declaration& other) : node(other), type_name(other.type_name), name(other.name), tags(other.tags) {}

			// Move
			atom_declaration(atom_declaration&& other) : node(std::move(other)), type_name(std::move(other.type_name)), name(std::move(other.name)), tags(std::move(other.tags)) {}
			atom_declaration& operator=(atom_declaration&& other)
			{
				set_type(other.get_type());
				this->type_name = std::move(other.type_name);
				this->name = std::move(other.name);
				tags = std::move(other.tags);
				return *this;
			}

			atom_type type_name;
			identifier name;
			tags::tags tags;
		};

		struct function_declaration : public node
		{
			function_declaration(function_type&& type_name, identifier&& name) : node(types::unset_type()), type_name(std::move(type_name)), name(std::move(name)) {}

			node* copy() override
			{
				return new function_declaration(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			function_declaration(const function_declaration& other) : node(other), type_name(other.type_name), name(other.name), tags(other.tags) {}

			// Move
			function_declaration(function_declaration&& other) : node(std::move(other)), type_name(std::move(other.type_name)), name(std::move(other.name)), tags(std::move(other.tags)) {}
			function_declaration& operator=(function_declaration&& other)
			{
				set_type(other.get_type());
				this->type_name = std::move(other.type_name);
				this->name = std::move(other.name);
				tags = std::move(other.tags);
				return *this;
			}

			function_type type_name;
			identifier name;
			tags::tags tags;
		};

		struct tuple_declaration : public node
		{
			tuple_declaration(std::vector<std::variant<atom_declaration, function_declaration>>&& elements) : node(types::unset_type()), elements(std::move(elements)) {}

			node* copy() override
			{
				return new tuple_declaration(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			tuple_declaration(const tuple_declaration& other) : node(other), elements(other.elements), tags(other.tags) {}

			// Move
			tuple_declaration(tuple_declaration&& other) : node(std::move(other)), elements(std::move(other.elements)), tags(std::move(other.tags)) {}
			tuple_declaration& operator=(tuple_declaration&& other)
			{
				set_type(other.get_type());
				this->elements = std::move(other.elements);
				tags = std::move(other.tags);
				return *this;
			}

			std::vector<std::variant<atom_declaration, function_declaration>> elements;
			tags::tags tags;
		};

		struct tuple : public node
		{
			tuple(std::vector<unique_node>&& children) : node(types::unset_type()), children(std::move(children)) {}

			node* copy() override
			{
				return new tuple(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			tuple(const tuple& other) : node(other), tags(other.tags) 
			{
				for (decltype(auto) child : other.children)
				{
					this->children.push_back(unique_node(child->copy()));
				}
			}

			// Move
			tuple(tuple&& other) : node(std::move(other)), children(std::move(other.children)), tags(std::move(other.tags)) {}
			tuple& operator=(tuple&& other)
			{
				set_type(other.get_type());
				children = std::move(other.children);
				tags = std::move(other.tags);
				return *this;
			}

			std::vector<unique_node> children;
			tags::tags tags;
		};

		struct block : public node
		{
			block(std::vector<unique_node>&& children) : node(types::unset_type()), children(std::move(children)) {}

			node* copy() override
			{
				return new block(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			block(const block& other) : node(other), tags(other.tags)
			{
				for (decltype(auto) child : other.children)
				{
					this->children.push_back(unique_node(child->copy()));
				}
			}

			// Move
			block(block&& other) : node(std::move(other)), children(std::move(other.children)), tags(std::move(other.tags)) {}
			block& operator=(block&& other)
			{
				set_type(other.get_type());
				children = std::move(other.children);
				tags = std::move(other.tags);
				return *this;
			}

			std::vector<unique_node> children;
			tags::tags tags;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, unique_node params) : node(types::unset_type()), id(std::move(id)), params(std::move(params)) {}

			node* copy() override
			{
				return new function_call(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			function_call(const function_call& other);

			// Move
			function_call(function_call&& other) : node(std::move(other)), id(std::move(other.id)), params(std::move(other.params)), tags(std::move(other.tags)) { }
			function_call& operator=(function_call&& other)
			{
				set_type(other.get_type());
				id = std::move(other.id);
				params = std::move(other.params);
				tags = std::move(other.tags);
				return *this;
			}

			identifier id;
			unique_node params;
			tags::tags tags;
		};

		struct type_declaration : public node
		{
			type_declaration(identifier&& name, tuple_declaration&& types) : node(types::atom_type{ "void" }), id(std::move(name)), types(std::move(types)) {}

			node* copy() override
			{
				return new type_declaration(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			type_declaration(const type_declaration& other) : node(other), id(other.id), types(other.types), tags(other.tags) {}

			// Move
			type_declaration(type_declaration&& other) : node(std::move(other)), id(std::move(other.id)), types(std::move(other.types)), tags(std::move(other.tags)) {}
			type_declaration& operator=(type_declaration&& other)
			{
				set_type(other.get_type());
				id = std::move(other.id);
				types = std::move(other.types);
				tags = std::move(other.tags);
				return *this;
			}

			identifier id;
			tuple_declaration types;
			tags::tags tags;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, unique_node val) : node(types::unset_type()), id(std::move(id)), value(std::move(val)) {}

			node* copy() override
			{
				return new assignment(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			assignment(const assignment& other);

			// Move
			assignment(assignment&& other) : node(std::move(other)), id(std::move(other.id)), value(std::move(other.value)), tags(std::move(other.tags)) {}
			assignment& operator=(assignment&& other)
			{
				set_type(other.get_type());
				id = std::move(other.id);
				value = std::move(other.value);
				tags = std::move(other.tags);
				return *this;
			}

			identifier id;
			unique_node value;
			tags::tags tags;
		};

		struct function : public node
		{
			function(std::optional<identifier>&& name, tuple_declaration&& from, unique_node to, unique_node body) : node(types::unset_type()), name(std::move(name)), from(std::move(from)), to(std::move(to)), body(std::move(body)) {}

			node* copy() override
			{
				return new function(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			function(const function& other);

			// Move
			function(function&& other) : node(std::move(other)), name(std::move(other.name)), from(std::move(other.from)), to(std::move(other.to)), body(std::move(other.body)), tags(std::move(other.tags)) {}
			function& operator=(function&& other)
			{
				set_type(other.get_type());
				name = std::move(other.name);
				from = std::move(other.from);
				to = std::move(other.to);
				body = std::move(other.body);
				tags = std::move(other.tags);
				return *this;
			}

			// Name is set when the function is not anonymous, for recursion
			std::optional<identifier> name;
			tuple_declaration from;
			unique_node to;
			unique_node body;

			tags::tags tags;
		};

		struct conditional_branch_path : public node
		{
			conditional_branch_path(unique_node test, unique_node code) : node(types::unset_type()), test_path(std::move(test)), code_path(std::move(code)) {}

			node* copy() override
			{
				return new conditional_branch_path(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			conditional_branch_path(const conditional_branch_path& other);

			// Move
			conditional_branch_path(conditional_branch_path&& other) : node(std::move(other)), code_path(std::move(other.code_path)), test_path(std::move(other.test_path)), tags(std::move(other.tags)) {}
			conditional_branch_path& operator=(conditional_branch_path&& other) {
				set_type(other.get_type());
				this->code_path = std::move(other.code_path);
				this->test_path = std::move(other.test_path);
				tags = std::move(other.tags);
				return *this;
			}

			unique_node test_path;
			unique_node code_path;
			tags::tags tags;
		};

		struct conditional_branch : public node
		{
			conditional_branch(std::vector<conditional_branch_path>&& branches) : node(types::unset_type()), branches(std::move(branches)) {}

			node* copy() override
			{
				return new conditional_branch(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node lower() override;

			// Copy
			conditional_branch(const conditional_branch& other) : node(other), branches(other.branches), tags(other.tags) {}

			// Move
			conditional_branch(conditional_branch&& other) : node(std::move(other)), branches(std::move(other.branches)), tags(std::move(other.tags)) {}
			conditional_branch& operator=(conditional_branch&& other) {
				set_type(other.get_type());
				this->branches = std::move(other.branches);
				tags = std::move(other.tags);
				return *this;
			}

			std::vector<conditional_branch_path> branches;
			tags::tags tags;
		};
	}
}