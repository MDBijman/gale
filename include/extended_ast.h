#pragma once
#include <vector>
#include <string>
#include <memory>
#include <optional>

#include "values.h"
#include "types.h"
#include "tags.h"
#include "core_ast.h"
#include "error.h"

namespace fe
{
	class typecheck_environment;

	namespace extended_ast
	{
		struct node
		{
			node(types::unique_type t) : type(std::move(t)) {}
			node(types::type* t) : type(t) {}
			node(const node& other) : type(other.type->copy()) {}
			node(node&& other) : type(std::move(other.type)) {}

			virtual ~node() {};
			virtual node* copy() = 0;
			virtual void typecheck(typecheck_environment&) = 0;
			virtual core_ast::node* lower() = 0;

			types::type& get_type() const
			{
				return *type;
			}

			void set_type(types::unique_type t)
			{
				type = std::move(t);
			}

			void set_type(types::type* t)
			{
				type.reset(t);
			}

		private:
			types::unique_type type;
		};

		using unique_node = std::unique_ptr<node>;

		struct division;

		// Definitions

		/* Expressions */

		struct integer : public node
		{
			integer(values::integer val) : node(new types::unset_type()), value(val) {}

			node* copy() override
			{
				return new integer(*this);
			}

			void typecheck(typecheck_environment&) override;
			core_ast::node* lower() override;

			// Copy
			integer(const integer& other) : node(other), value(other.value), tags(other.tags) {}

			// Move
			integer(integer&& other) : node(std::move(other)), value(std::move(other.value)), tags(std::move(other.tags)) {}
			integer& operator=(integer&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				value = std::move(other.value);
				tags = std::move(other.tags);
				return *this;
			}

			values::integer value;
			tags::tags tags;
		};

		struct string : public node
		{
			string(values::string val) : node(new types::unset_type()), value(val) {}

			node* copy() override
			{
				return new string(*this);
			}

			void typecheck(typecheck_environment& t_env) override;
			core_ast::node* lower() override;

			// Copy
			string(const string& other) : node(other), value(other.value), tags(other.tags) {}

			// Move
			string(string&& other) : node(std::move(other)), value(std::move(other.value)), tags(std::move(other.tags)) {}
			string& operator=(string&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				value = std::move(other.value);
				tags = std::move(other.tags);
				return *this;
			}

			values::string value;
			tags::tags tags;
		};

		struct identifier : public node
		{
			identifier(std::vector<std::string>&& modules) : node(new types::unset_type()), segments(std::move(modules)) {}

			node* copy() override
			{
				return new identifier(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			identifier(const identifier& other) : node(other), segments(other.segments), offsets(other.offsets), tags(other.tags) {}
			identifier& operator=(const identifier& other)
			{
				this->segments = other.segments;
				this->offsets = other.offsets;
				set_type(other.get_type().copy());
				tags = other.tags;
				return *this;
			}

			// Move
			identifier(identifier&& other) : node(std::move(other)), segments(std::move(other.segments)), offsets(std::move(other.offsets)), tags(std::move(other.tags)) {}
			identifier& operator=(identifier&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
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
				new_id.set_type(types::unique_type(this->get_type().copy()));
				new_id.offsets = offsets;
				return new_id;
			}

			std::string to_string() const
			{
				std::string res;
				for (auto segment : segments)
					res.append(segment + ".");
				res.erase(res.size() - 1, 1);
				return res;
			}

			std::vector<std::string> segments;
			std::vector<int> offsets;
			tags::tags tags;
		};

		struct tuple : public node
		{
			tuple(std::vector<unique_node>&& children) : node(new types::unset_type()), children(std::move(children)) {}

			node* copy() override
			{
				return new tuple(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

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
				set_type(types::unique_type(other.get_type().copy()));
				children = std::move(other.children);
				tags = std::move(other.tags);
				return *this;
			}

			std::vector<unique_node> children;
			tags::tags tags;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, unique_node params) : node(new types::unset_type()), id(std::move(id)), params(std::move(params)) {}

			function_call(std::vector<unique_node>&& children);

			node* copy() override
			{
				return new function_call(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			function_call(const function_call& other);

			// Move
			function_call(function_call&& other) : node(std::move(other)), id(std::move(other.id)), params(std::move(other.params)), tags(std::move(other.tags)) { }
			function_call& operator=(function_call&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				id = std::move(other.id);
				params = std::move(other.params);
				tags = std::move(other.tags);
				return *this;
			}

			identifier id;
			unique_node params;
			tags::tags tags;
		};

		struct match_branch : public node
		{
			match_branch(unique_node test, unique_node code) : node(new types::unset_type()), test_path(std::move(test)), code_path(std::move(code)) {}

			match_branch(std::vector<unique_node>&& children) :
				node(new types::unset_type()),
				test_path(std::move(children.at(0))),
				code_path(std::move(children.at(1))) {}

			node* copy() override
			{
				return new match_branch(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			match_branch(const match_branch& other);

			// Move
			match_branch(match_branch&& other) : node(std::move(other)), code_path(std::move(other.code_path)), test_path(std::move(other.test_path)), tags(std::move(other.tags)) {}
			match_branch& operator=(match_branch&& other) {
				set_type(types::unique_type(other.get_type().copy()));
				this->code_path = std::move(other.code_path);
				this->test_path = std::move(other.test_path);
				tags = std::move(other.tags);
				return *this;
			}

			unique_node test_path;
			unique_node code_path;
			tags::tags tags;
		};

		struct match : public node
		{
			match(std::vector<match_branch>&& branches) : node(new types::unset_type()), branches(std::move(branches)) {}

			match(std::vector<unique_node>&& children) :
				node(new types::unset_type()), expression(std::move(children.at(0)))
			{
				// Match has children [ expression, match_branch* ]

				for (auto it = children.begin() + 1; it != children.end(); it++)
				{
					branches.push_back(std::move(*dynamic_cast<match_branch*>(it->get())));
				}
			}

			node* copy() override
			{
				return new match(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			match(const match& other) : node(other), branches(other.branches), tags(other.tags) {}

			// Move
			match(match&& other) : node(std::move(other)), branches(std::move(other.branches)), tags(std::move(other.tags)) {}
			match& operator=(match&& other) {
				set_type(types::unique_type(other.get_type().copy()));
				this->branches = std::move(other.branches);
				tags = std::move(other.tags);
				return *this;
			}

			unique_node expression;
			std::vector<match_branch> branches;
			tags::tags tags;
		};

		struct block : public node
		{
			block(std::vector<unique_node>&& children) : node(new types::unset_type()), children(std::move(children)) {}

			node* copy() override
			{
				return new block(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

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
				set_type(types::unique_type(other.get_type().copy()));
				children = std::move(other.children);
				tags = std::move(other.tags);
				return *this;
			}

			std::vector<unique_node> children;
			tags::tags tags;
		};

		/* Other */

		struct module_declaration : public node
		{
			module_declaration(identifier&& name) : node(new types::unset_type()), name(std::move(name)) {}
			module_declaration(std::vector<unique_node>&& children) :
				node(new types::unset_type()),
				name(std::move(*dynamic_cast<identifier*>(children.at(0).get()))) {}

			// Copy
			module_declaration(const module_declaration& other) : node(other), name(other.name), tags(other.tags) {}

			// Move
			module_declaration(module_declaration&& other) : node(std::move(other)), name(std::move(other.name)), tags(std::move(other.tags)) {}
			module_declaration& operator=(module_declaration&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->name = std::move(other.name);
				this->tags = std::move(other.tags);
				return *this;
			}

			node* copy() override
			{
				return new module_declaration(*this);
			}
			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			identifier name;
			tags::tags tags;
		};

		/* Declarations */
		struct atom_declaration : public node
		{
			atom_declaration(unique_node type_name, identifier&& name) : node(new types::unset_type()), type_expression(std::move(type_name)), name(std::move(name))
			{}
			atom_declaration(std::vector<unique_node>&& children);

			node* copy() override
			{
				return new atom_declaration(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			atom_declaration(const atom_declaration& other) : node(other), type_expression(other.type_expression->copy()), name(other.name), tags(other.tags)
			{}

			// Move
			atom_declaration(atom_declaration&& other) : type_expression(std::move(other.type_expression)), name(std::move(other.name)), tags(std::move(other.tags)), node(std::move(other))
			{}
			atom_declaration& operator=(atom_declaration&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->type_expression = std::move(other.type_expression);
				this->name = std::move(other.name);
				tags = std::move(other.tags);
				return *this;
			}

			unique_node type_expression;
			identifier name;
			tags::tags tags;
		};

		struct tuple_declaration : public node
		{
			tuple_declaration(std::vector<unique_node>&& children);

			node* copy() override
			{
				return new tuple_declaration(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			tuple_declaration(const tuple_declaration& other) : node(other), tags(other.tags)
			{
				for (decltype(auto) element : other.elements)
				{
					elements.push_back(unique_node(element->copy()));
				}
			}

			// Move
			tuple_declaration(tuple_declaration&& other) : elements(std::move(other.elements)), tags(std::move(other.tags)), node(std::move(other)) {}
			tuple_declaration& operator=(tuple_declaration&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->elements = std::move(other.elements);
				tags = std::move(other.tags);
				return *this;
			}

			std::vector<unique_node> elements;
			tags::tags tags;
		};

		struct function : public node
		{
			function(std::optional<identifier> name, unique_node from, unique_node to, unique_node body) : node(new types::unset_type()), name(std::move(name)), from(std::move(from)), to(std::move(to)), body(std::move(body)) {}
			function(std::vector<unique_node>&& children);

			node* copy() override
			{
				return new function(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			function(const function& other);

			// Move
			function(function&& other) : node(std::move(other)), name(std::move(other.name)), from(std::move(other.from)), to(std::move(other.to)), body(std::move(other.body)), tags(std::move(other.tags)) {}
			function& operator=(function&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				name = std::move(other.name);
				from = std::move(other.from);
				to = std::move(other.to);
				body = std::move(other.body);
				tags = std::move(other.tags);
				return *this;
			}

			// Name is set when the function is not anonymous, for recursion
			std::optional<identifier> name;
			unique_node from;
			unique_node to;
			unique_node body;

			tags::tags tags;
		};

		/* Statements */

		struct type_definition : public node
		{
			//type_definition(identifier&& name, tuple_declaration types) : node(new types::atom_type{ "void" }), id(std::move(name)), types(std::move(types)) {}
			//type_definition(identifier&& name, atom_declaration types) : node(new types::atom_type{ "void" }), id(std::move(name)), types(std::move(types)) {}
			type_definition(std::vector<unique_node>&& children);

			node* copy() override
			{
				return new type_definition(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			type_definition(const type_definition& other) : node(other), id(other.id), types(other.types->copy()), tags(other.tags) {}

			// Move
			type_definition(type_definition&& other) : node(std::move(other)), id(std::move(other.id)), types(std::move(other.types)), tags(std::move(other.tags)) {}
			type_definition& operator=(type_definition&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				id = std::move(other.id);
				types = std::move(other.types);
				tags = std::move(other.tags);
				return *this;
			}

			identifier id;
			unique_node types;
			tags::tags tags;
		};

		struct export_stmt : public node
		{
			export_stmt(std::vector<identifier> name) : node(new types::unset_type()), names(std::move(names)) {}
			export_stmt(std::vector<unique_node>&& children) :
				node(new types::unset_type())
			{
				for (decltype(auto) child : children)
					names.push_back(std::move(*dynamic_cast<identifier*>(child.get())));
			}

			node* copy() override
			{
				return new export_stmt(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			export_stmt(const export_stmt& other) : node(other), names(other.names), tags(other.tags) {}

			// Move
			export_stmt(export_stmt&& other) : node(std::move(other)), names(std::move(other.names)), tags(std::move(other.tags)) {}
			export_stmt& operator=(export_stmt&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->names = std::move(other.names);
				this->tags = std::move(other.tags);
				return *this;
			}

			std::vector<identifier> names;
			tags::tags tags;
		};

		struct assignment : public node
		{
			assignment(identifier&& id, unique_node val) : node(new types::unset_type()), id(std::move(id)), value(std::move(val)) {}

			assignment(std::vector<unique_node>&& children) :
				node(new types::unset_type()),
				id(std::move(*dynamic_cast<identifier*>(children.at(0).get()))),
				value(std::move(children.at(1)))
			{
				if (auto fun = dynamic_cast<function*>(value.get()))
				{
					fun->name = identifier(id);
				}
			}

			node* copy() override
			{
				return new assignment(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			assignment(const assignment& other);

			// Move
			assignment(assignment&& other) : node(std::move(other)), id(std::move(other.id)), value(std::move(other.value)), tags(std::move(other.tags)) {}
			assignment& operator=(assignment&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				id = std::move(other.id);
				value = std::move(other.value);
				tags = std::move(other.tags);
				return *this;
			}

			identifier id;
			unique_node value;
			tags::tags tags;
		};

		/* Type Expressions */

		struct type_tuple : public node
		{
			type_tuple(std::vector<unique_node>&& children);

			node* copy() override
			{
				return new type_tuple(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			type_tuple(const type_tuple& other) : node(other), tags(other.tags)
			{
				for (decltype(auto) element : other.elements)
				{
					elements.push_back(unique_node(element->copy()));
				}
			}

			// Move
			type_tuple(type_tuple&& other) : node(std::move(other)), elements(std::move(other.elements)), tags(std::move(other.tags)) {}
			type_tuple& operator=(type_tuple&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->elements = std::move(other.elements);
				this->tags = std::move(other.tags);
				return *this;
			}

			std::vector<unique_node> elements;
			tags::tags tags;
		};

		struct type_atom : public node
		{
			type_atom(std::vector<unique_node>&& children) :
				node(new types::unset_type()),
				type(std::move(children.at(0))) {}

			node* copy() override
			{
				return new type_atom(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			type_atom(const type_atom& other) : node(other), type(other.type->copy()), tags(other.tags) {}

			// Move
			type_atom(type_atom&& other) : type(std::move(other.type)), tags(std::move(other.tags)), node(std::move(other)) {}
			type_atom& operator=(type_atom&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->type = std::move(other.type);
				this->tags = std::move(other.tags);
				return *this;
			}

			unique_node type;
			tags::tags tags;
		};

		struct function_type : public node
		{
			function_type(std::vector<unique_node>&& children) :
				node(new types::unset_type()),
				from(std::move(children.at(0))),
				to(std::move(children.at(1)))
			{

			}

			node* copy() override
			{
				return new function_type(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			function_type(const function_type& other) : node(other), from(other.from->copy()), to(other.to->copy()), tags(other.tags) {}

			// Move
			function_type(function_type&& other) : node(std::move(other)), from(std::move(other.from)), to(std::move(other.to)), tags(std::move(other.tags)) {}
			function_type& operator=(function_type&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->from = std::move(other.from);
				this->to = std::move(other.to);
				tags = std::move(other.tags);
				return *this;
			}

			unique_node from;
			unique_node to;
			tags::tags tags;
		};

		struct reference_type : public node
		{
			reference_type(std::vector<unique_node>&& children) :
				node(new types::unset_type()),
				child(std::move(children.at(0))) {}

			node* copy() override
			{
				return new reference_type(*this);
			}

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			// Copy
			reference_type(const reference_type& other) : node(other), child(other.child->copy()) {}

			// Move
			reference_type(reference_type&& other) : node(std::move(other)), child(std::move(other.child)) {}
			reference_type& operator=(reference_type&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->child = std::move(other.child);
				return *this;
			}

			unique_node child;
		};

		struct array_type : public node
		{
			array_type(std::vector<unique_node>&& children);

			// Copy
			array_type(const array_type&);

			// Move
			array_type(array_type&&);
			array_type& operator=(array_type&&);

			node* copy() override;

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node child;
		};

		struct reference : public node
		{
			reference(std::vector<unique_node>&& children);

			// Copy
			reference(const reference&);

			// Move
			reference(reference&&);
			reference& operator=(reference&&);

			node* copy() override;

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node child;
		};

		struct array_value : public node
		{
			array_value(std::vector<unique_node>&& children);

			// Copy
			array_value(const array_value&);

			// Move
			array_value(array_value&&);
			array_value& operator=(array_value&&);

			node* copy() override;

			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			std::vector<unique_node> children;
		};

		struct equality : public node
		{
			equality(std::vector<unique_node>&& children);

			// Copy
			equality(const equality&);

			// Move
			equality(equality&&);
			equality& operator=(equality&&);

			node* copy() override;
			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node left, right;
		};

		struct addition : public node
		{
			addition(std::vector<unique_node>&& children);

			// Copy
			addition(const addition&);

			// Move
			addition(addition&&);
			addition& operator=(addition&&);

			node* copy() override;
			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node left, right;
		};

		struct subtraction : public node
		{
			subtraction(std::vector<unique_node>&& children);

			// Copy
			subtraction(const subtraction&);

			// Move
			subtraction(subtraction&&);
			subtraction& operator=(subtraction&&);

			node* copy() override;
			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node left, right;
		};

		struct multiplication : public node
		{
			multiplication(std::vector<unique_node>&& children);

			// Copy
			multiplication(const multiplication&);

			// Move
			multiplication(multiplication&&);
			multiplication& operator=(multiplication&&);

			node* copy() override;
			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node left, right;
		};

		struct division : public node
		{
			division(std::vector<unique_node>&& children);

			// Copy
			division(const division&);

			// Move
			division(division&&);
			division& operator=(division&&);

			node* copy() override;
			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node left, right;
		};

		struct array_index : public node
		{
			array_index(std::vector<unique_node>&& children);

			// Copy
			array_index(const array_index&);

			// Move
			array_index(array_index&&);
			array_index& operator=(array_index&&);

			node* copy() override;
			void typecheck(typecheck_environment& env) override;
			core_ast::node* lower() override;

			unique_node array_exp, index_exp;
		};

		struct while_loop : public node
		{
			while_loop(std::vector<unique_node>&& children)
				: node(new types::unset_type()), body(std::move(children.at(1))), test(std::move(children.at(0))) {}

			// Copy
			while_loop(const while_loop& other) : node(other), body(unique_node(other.body->copy())), test(unique_node(other.test->copy())) {}

			// Move
			while_loop(while_loop&& other) : node(std::move(other)), body(std::move(other.body)), test(std::move(other.test)) {}
			while_loop& operator=(while_loop&& other)
			{
				set_type(other.get_type().copy());
				this->body = std::move(other.body);
				this->test = std::move(other.test);
				return *this;
			}

			node* copy() override { return new while_loop(*this); }
			void typecheck(typecheck_environment& env) override
			{
				test->typecheck(env);
				body->typecheck(env);
				set_type(new types::unset_type());

				if (!(types::atom_type("boolean") == &test->get_type()))
				{
					throw typecheck_error{ "Test branch of while loop must have boolean type" };
				}
			}
			core_ast::node* lower() override
			{
				return new core_ast::while_loop(core_ast::unique_node(test->lower()), core_ast::unique_node(body->lower()));
			}

			unique_node test;
			unique_node body;
		};

		struct import_declaration : public node
		{
			import_declaration(std::vector<unique_node>&& children) : node(new types::unset_type())
			{
				for (auto&& child : children) modules.push_back(std::move(*dynamic_cast<identifier*>(child.get())));
			}

			// Copy
			import_declaration(const import_declaration& other) : node(other), modules(other.modules) {}
			import_declaration& operator=(const import_declaration& other)
			{
				set_type(other.get_type().copy());
				this->modules = other.modules;
				return *this;
			}

			// Move
			import_declaration(import_declaration&& other) : node(std::move(other)), modules(std::move(other.modules)) {}
			import_declaration& operator=(import_declaration&& other)
			{
				set_type(other.get_type().copy());
				this->modules = std::move(other.modules);
				return *this;
			}

			node* copy() override { return new import_declaration(*this); }
			void typecheck(typecheck_environment& env) override {}
			core_ast::node* lower() override
			{
				return new core_ast::no_op();
			}

			std::vector<identifier> modules;
		};
	}
}