#pragma once
#include "fe/data/values.h"
#include "fe/data/types.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/error.h"

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <assert.h>

namespace fe
{
	class type_environment;
	class scope_environment;

	namespace extended_ast
	{
		struct node
		{
			explicit node(types::unique_type t) : type(std::move(t)) {}
			explicit node(types::type* t) : type(t) {}

			node(const node& other) : type(other.type->copy()) {}
			node& operator=(const node& o)
			{
				this->type = types::unique_type(o.type->copy());
				return *this;
			}
 
			node(node&& other) noexcept : type(std::move(other.type)) {}
			node& operator=(node&& o)
			{
				this->type = std::move(o.type);
				return *this;
			}

			virtual ~node() {};
			virtual void typecheck(type_environment&) = 0;
			virtual void resolve(scope_environment& s_env) = 0;
			virtual core_ast::node* lower() = 0;
			virtual node* copy() = 0;

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

		struct literal : public node
		{
			literal(values::unique_value val) : node(new types::unset()), val(std::move(val)) {}

			literal(const literal& other) : node(other), val(other.val->copy()) {}
			literal& operator=(const literal& o)
			{
				node::operator=(o);
				this->val = values::unique_value(o.val->copy());
				return *this;
			}

			literal(literal&& other) noexcept : node(std::move(other)), val(std::move(other.val)) {}
			literal& operator=(literal&& other)
			{
				node::operator=(std::move(other));
				val = std::move(other.val);
				return *this;
			}

			void typecheck(type_environment&) override { this->set_type(val->get_type()); }
			void resolve(scope_environment& s_env) override {}
			core_ast::node* lower() override { return new core_ast::literal(values::unique_value(val->copy())); }
			node* copy() override { return new literal(*this); }

			values::unique_value val;
		};

		struct identifier : public node
		{
			explicit identifier(std::string segment) : node(new types::unset()), segments({ segment }) {}
			explicit identifier(std::vector<std::string>&& segments) : node(new types::unset()),
				segments(std::move(segments)) {}

			identifier(const identifier& other) : node(other), segments(other.segments), offsets(other.offsets) {}
			identifier& operator=(const identifier& other)
			{
				node::operator=(other);
				this->segments = other.segments;
				this->offsets = other.offsets;
				return *this;
			}

			identifier(identifier&& other) noexcept : node(std::move(other)), segments(std::move(other.segments)),
				offsets(std::move(other.offsets)), scope_distance(std::move(other.scope_distance)) {}
			identifier& operator=(identifier&& other)
			{
				node::operator=(std::move(other));
				segments = std::move(other.segments);
				offsets = std::move(other.offsets);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new identifier(*this);
			}

			bool identifier::operator==(const identifier& other) const
			{
				if (segments.size() != other.segments.size()) return false;
				if (offsets.size() != other.offsets.size()) return false;

				for (auto i = 0; i < segments.size(); i++)
					if (segments.at(i) != other.segments.at(i)) return false;

				for (auto i = 0; i < offsets.size(); i++)
					if (offsets.at(i) != other.offsets.at(i)) return false;

				if (scope_distance != other.scope_distance) return false;

				return true;
			}

			identifier without_first_segment() const
			{
				identifier new_id(*this);
				new_id.segments.erase(new_id.segments.begin());
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
			std::optional<std::size_t> scope_distance;
		};

		struct tuple : public node
		{
			explicit tuple(std::vector<unique_node>&& children) : node(new types::unset()),
				children(std::move(children)) {}

			// Copy
			tuple(const tuple& other) : node(other)
			{
				for (decltype(auto) child : other.children)
				{
					this->children.push_back(unique_node(child->copy()));
				}
			}

			// Move
			tuple(tuple&& other) noexcept : node(std::move(other)), children(std::move(other.children)) {}
			tuple& operator=(tuple&& other)
			{
				node::operator=(std::move(other));
				children = std::move(other.children);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new tuple(*this);
			}

			std::vector<unique_node> children;
		};

		struct function_call : public node
		{
			function_call(identifier&& id, unique_node params) : node(new types::unset()), id(std::move(id)),
				params(std::move(params)) {}
			function_call(std::vector<unique_node>&& children);

			// Copy
			function_call(const function_call& other);

			// Move
			function_call(function_call&& other) : node(std::move(other)), id(std::move(other.id)),
				params(std::move(other.params)) { }
			function_call& operator=(function_call&& other)
			{
				node::operator=(std::move(other));
				id = std::move(other.id);
				params = std::move(other.params);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new function_call(*this);
			}

			identifier id;
			unique_node params;
		};

		struct match_branch : public node
		{
			match_branch(unique_node test, unique_node code) : node(new types::unset()), test_path(std::move(test)),
				code_path(std::move(code)) {}

			match_branch(std::vector<unique_node>&& children) : node(new types::unset()),
				test_path(std::move(children.at(0))), code_path(std::move(children.at(1))) {}

			// Copy
			match_branch(const match_branch& other);

			// Move
			match_branch(match_branch&& other) : node(std::move(other)), code_path(std::move(other.code_path)),
				test_path(std::move(other.test_path)) {}
			match_branch& operator=(match_branch&& other) {
				node::operator=(std::move(other));
				this->code_path = std::move(other.code_path);
				this->test_path = std::move(other.test_path);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new match_branch(*this);
			}

			unique_node test_path;
			unique_node code_path;
		};

		struct match : public node
		{
			match(std::vector<match_branch>&& branches) : node(new types::unset()), branches(std::move(branches)) {}

			match(std::vector<unique_node>&& children) : node(new types::unset()),
				expression(std::move(children.at(0)))
			{
				for (auto it = children.begin() + 1; it != children.end(); it++)
				{
					branches.push_back(std::move(*dynamic_cast<match_branch*>(it->get())));
				}
			}

			// Copy
			match(const match& other) : node(other), expression(other.expression->copy()), branches(other.branches) {}

			// Move
			match(match&& other) : node(std::move(other)), expression(std::move(other.expression)),
				branches(std::move(other.branches)) {}
			match& operator=(match&& other) {
				node::operator=(std::move(other));
				this->expression = std::move(other.expression);
				this->branches = std::move(other.branches);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new match(*this);
			}

			unique_node expression;
			std::vector<match_branch> branches;
		};

		struct block : public node
		{
			block(std::vector<unique_node>&& children) : node(new types::unset()), children(std::move(children)) {}

			// Copy
			block(const block& other) : node(other)
			{
				for (decltype(auto) child : other.children)
				{
					this->children.push_back(unique_node(child->copy()));
				}
			}

			// Move
			block(block&& other) : node(std::move(other)), children(std::move(other.children)) {}
			block& operator=(block&& other)
			{
				node::operator=(std::move(other));
				children = std::move(other.children);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new block(*this);
			}

			std::vector<unique_node> children;
		};

		struct module_declaration : public node
		{
			module_declaration(identifier&& name) : node(new types::unset()), name(std::move(name)) {}
			module_declaration(std::vector<unique_node>&& children) :
				node(new types::unset()), name(std::move(*dynamic_cast<identifier*>(children.at(0).get()))) {}

			// Copy
			module_declaration(const module_declaration& other) : node(other), name(other.name) {}

			// Move
			module_declaration(module_declaration&& other) : node(std::move(other)), name(std::move(other.name)) {}
			module_declaration& operator=(module_declaration&& other)
			{
				node::operator=(std::move(other));
				this->name = std::move(other.name);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new module_declaration(*this);
			}

			identifier name;
		};

		struct atom_declaration : public node
		{
			atom_declaration(unique_node type_name, identifier&& name) : node(new types::unset()),
				type_expression(std::move(type_name)), name(std::move(name)) {}
			atom_declaration(std::vector<unique_node>&& children);

			// Copy
			atom_declaration(const atom_declaration& other) : node(other),
				type_expression(other.type_expression->copy()), name(other.name) {}

			// Move
			atom_declaration(atom_declaration&& other) : type_expression(std::move(other.type_expression)),
				name(std::move(other.name)), node(std::move(other)) {}
			atom_declaration& operator=(atom_declaration&& other)
			{
				node::operator=(std::move(other));
				this->type_expression = std::move(other.type_expression);
				this->name = std::move(other.name);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new atom_declaration(*this);
			}

			unique_node type_expression;
			identifier name;
		};

		struct tuple_declaration : public node
		{
			tuple_declaration(std::vector<unique_node>&& children);

			// Copy
			tuple_declaration(const tuple_declaration& other) : node(other)
			{
				for (decltype(auto) element : other.elements)
				{
					elements.push_back(unique_node(element->copy()));
				}
			}

			// Move
			tuple_declaration(tuple_declaration&& other) : elements(std::move(other.elements)),
				node(std::move(other)) {}
			tuple_declaration& operator=(tuple_declaration&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->elements = std::move(other.elements);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new tuple_declaration(*this);
			}

			std::vector<unique_node> elements;
		};

		struct function : public node
		{
			function(identifier name, unique_node from, unique_node to, unique_node body) : node(new types::unset()),
				name(std::move(name)), from(std::move(from)), to(std::move(to)), body(std::move(body)) {}
			function(std::vector<unique_node>&& children);

			// Copy
			function(const function& other);

			// Move
			function(function&& other) : node(std::move(other)), name(std::move(other.name)),
				from(std::move(other.from)), to(std::move(other.to)), body(std::move(other.body)) {}
			function& operator=(function&& other)
			{
				node::operator=(std::move(other));
				name = std::move(other.name);
				from = std::move(other.from);
				to = std::move(other.to);
				body = std::move(other.body);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new function(*this);
			}

			identifier name;
			unique_node from;
			unique_node to;
			unique_node body;
		};

		struct type_definition : public node
		{
			type_definition(std::vector<unique_node>&& children);

			// Copy
			type_definition(const type_definition& other) : node(other), id(other.id), types(other.types->copy()) {}

			// Move
			type_definition(type_definition&& other) : node(std::move(other)), id(std::move(other.id)), types(std::move(other.types)) {}
			type_definition& operator=(type_definition&& other)
			{
				node::operator=(std::move(other));
				id = std::move(other.id);
				types = std::move(other.types);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new type_definition(*this);
			}

			identifier id;
			unique_node types;
		};

		struct export_stmt : public node
		{
			export_stmt(std::vector<identifier> name) : node(new types::unset()), names(std::move(names)) {}
			export_stmt(std::vector<unique_node>&& children) : node(new types::unset())
			{
				for (decltype(auto) child : children)
					names.push_back(std::move(*dynamic_cast<identifier*>(child.get())));
			}

			// Copy
			export_stmt(const export_stmt& other) : node(other), names(other.names) {}

			// Move
			export_stmt(export_stmt&& other) : node(std::move(other)), names(std::move(other.names)) {}
			export_stmt& operator=(export_stmt&& other)
			{
				node::operator=(std::move(other));
				this->names = std::move(other.names);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new export_stmt(*this);
			}

			std::vector<identifier> names;
		};

		struct identifier_tuple : public node
		{
			identifier_tuple(std::vector<unique_node>&& children) : node(new types::unset())
			{
				for (auto&& child : children)
				{
					if (auto id = dynamic_cast<identifier*>(child.get()))
					{
						content.push_back(std::move(*id));
					}
					else if (auto id_tuple = dynamic_cast<identifier_tuple*>(child.get()))
					{
						content.push_back(std::move(*id_tuple));
					}
				}
			}

			// Copy
			identifier_tuple(const identifier_tuple& other) : node(other), content(other.content) {}
			identifier_tuple& operator=(const identifier_tuple& other)
			{
				set_type(other.get_type().copy());
				content = other.content;
				return *this;
			}

			// Move
			identifier_tuple(identifier_tuple&& other) : node(std::move(other)), content(std::move(other.content)) {}
			identifier_tuple& operator=(identifier_tuple&& other)
			{
				set_type(other.get_type().copy());
				this->content = std::move(other.content);
				return *this;
			}

			void typecheck(type_environment& env) override {}
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override
			{
				throw typecheck_error{
					"Cannot lower an identifier tuple"
				};
			}
			node* copy() override { return new identifier_tuple(*this); }

			std::vector<std::variant<identifier, identifier_tuple>> content;
		};

		struct declaration : public node
		{
			declaration(std::vector<unique_node>&& children) :
				node(new types::unset()),
				value(std::move(children.at(2))),
				type_name(std::move(*dynamic_cast<identifier*>(children.at(1).get()))),
				lhs(dynamic_cast<identifier*>(children.at(0).get()) != nullptr
					? std::variant<identifier, identifier_tuple>(std::move(*dynamic_cast<identifier*>(children.at(0).get())))
					: std::variant<identifier, identifier_tuple>(std::move(*dynamic_cast<identifier_tuple*>(children.at(0).get()))))
			{
				if (auto fun = dynamic_cast<function*>(value.get()))
				{
					assert(std::holds_alternative<identifier>(lhs));
					fun->name = identifier(std::get<identifier>(lhs));
				}
			}

			// Copy
			declaration(const declaration& other);

			// Move
			declaration(declaration&& other) : node(std::move(other)), type_name(std::move(other.type_name)), lhs(std::move(other.lhs)), value(std::move(other.value)) {}
			declaration& operator=(declaration&& other)
			{
				node::operator=(std::move(other));
				type_name = std::move(other.type_name);
				lhs = std::move(other.lhs);
				value = std::move(other.value);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new declaration(*this);
			}

			std::variant<identifier, identifier_tuple> lhs;
			identifier type_name;
			unique_node value;
		};

		struct assignment : public node
		{
			assignment(std::vector<unique_node>&& children) :
				node(new types::unset()), lhs(*dynamic_cast<identifier*>(children.at(0).get())),
				value(std::move(children.at(1))) {}

			// Copy
			assignment(const assignment& other);

			// Move
			assignment(assignment&& other) : node(std::move(other)), lhs(std::move(other.lhs)),
				value(std::move(other.value)) {}
			assignment& operator=(assignment&& other)
			{
				node::operator=(std::move(other));
				lhs = std::move(other.lhs);
				value = std::move(other.value);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new assignment(*this);
			}

			identifier lhs;
			unique_node value;
		};

		struct type_tuple : public node
		{
			type_tuple(std::vector<unique_node>&& children);

			// Copy
			type_tuple(const type_tuple& other) : node(other)
			{
				for (decltype(auto) element : other.elements)
				{
					elements.push_back(unique_node(element->copy()));
				}
			}

			// Move
			type_tuple(type_tuple&& other) : node(std::move(other)), elements(std::move(other.elements)) {}
			type_tuple& operator=(type_tuple&& other)
			{
				node::operator=(std::move(other));
				this->elements = std::move(other.elements);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new type_tuple(*this);
			}

			std::vector<unique_node> elements;
		};

		struct type_atom : public node
		{
			type_atom(std::vector<unique_node>&& children) : node(new types::unset()),
				type(std::move(children.at(0))) {}

			// Copy
			type_atom(const type_atom& other) : node(other), type(other.type->copy()) {}

			// Move
			type_atom(type_atom&& other) : type(std::move(other.type)), node(std::move(other)) {}
			type_atom& operator=(type_atom&& other)
			{
				node::operator=(std::move(other));
				this->type = std::move(other.type);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new type_atom(*this);
			}

			// TODO turn into identifier since thats the only thing a type atom can contain
			unique_node type;
		};

		struct function_type : public node
		{
			function_type(std::vector<unique_node>&& children) : node(new types::unset()),
				from(std::move(children.at(0))), to(std::move(children.at(1))) {}

			// Copy
			function_type(const function_type& other) : node(other), from(other.from->copy()), to(other.to->copy()) {}

			// Move
			function_type(function_type&& other) : node(std::move(other)), from(std::move(other.from)), to(std::move(other.to)) {}
			function_type& operator=(function_type&& other)
			{
				node::operator=(std::move(other));
				this->from = std::move(other.from);
				this->to = std::move(other.to);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override
			{
				return new function_type(*this);
			}

			unique_node from;
			unique_node to;
		};

		struct reference_type : public node
		{
			reference_type(std::vector<unique_node>&& children) :
				node(new types::unset()),
				child(std::move(children.at(0))) {}

			reference_type(const reference_type& other) : node(other), child(other.child->copy()) {}
			reference_type& operator=(const reference_type& other);

			reference_type(reference_type&& other) : node(std::move(other)), child(std::move(other.child)) {}
			reference_type& operator=(reference_type&& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->child = std::move(other.child);
				return *this;
			}

			void typecheck(type_environment& env) override;
			core_ast::node* lower() override;
			void resolve(scope_environment& s_env) override;
			node* copy() override
			{
				return new reference_type(*this);
			}

			unique_node child;
		};

		struct array_type : public node
		{
			array_type(std::vector<unique_node>&& children);

			array_type(const array_type&);
			array_type& operator=(const array_type&);

			array_type(array_type&&);
			array_type& operator=(array_type&&);

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override;

			unique_node child;
		};

		struct reference : public node
		{
			reference(std::vector<unique_node>&& children);

			reference(const reference&);
			reference& operator=(const reference&);

			reference(reference&&);
			reference& operator=(reference&&);

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override;

			unique_node child;
		};

		struct array_value : public node
		{
			explicit array_value(std::vector<unique_node>&& children);

			// Copy
			array_value(const array_value&);

			// Move
			array_value(array_value&&);
			array_value& operator=(array_value&&);

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override;
			node* copy() override;

			std::vector<unique_node> children;
		};

		enum class bin_op_type
		{
			EQ, LT, LTE, GT, GTE,
			SUB, ADD, MUL, DIV, MOD
		};

		inline constexpr const char* op_func(bin_op_type op)
		{
			switch (op) {
			case bin_op_type::EQ:  return "eq";  break;
			case bin_op_type::LT:  return "lt";  break;
			case bin_op_type::LTE: return "lte"; break;
			case bin_op_type::GT:  return "gt";  break;
			case bin_op_type::GTE: return "gte"; break;
			case bin_op_type::SUB: return "sub"; break;
			case bin_op_type::ADD: return "add"; break;
			case bin_op_type::MUL: return "mul"; break;
			case bin_op_type::DIV: return "div"; break;
			case bin_op_type::MOD: return "mod"; break;
			}
		};

		static constexpr types::atom_type op_res_type(bin_op_type op)
		{
			switch (op) {
			case bin_op_type::EQ:  return types::atom_type::BOOL; break;
			case bin_op_type::LT:  return types::atom_type::BOOL; break;
			case bin_op_type::LTE: return types::atom_type::BOOL; break;
			case bin_op_type::GT:  return types::atom_type::BOOL; break;
			case bin_op_type::GTE: return types::atom_type::BOOL; break;
			case bin_op_type::SUB: return types::atom_type::I32;  break;
			case bin_op_type::ADD: return types::atom_type::I32;  break;
			case bin_op_type::MUL: return types::atom_type::I32;  break;
			case bin_op_type::DIV: return types::atom_type::I32;  break;
			case bin_op_type::MOD: return types::atom_type::I32;  break;
			}
		};

		static constexpr types::atom_type op_lhs_type(bin_op_type op)
		{
			switch (op) {
			case bin_op_type::EQ:  return types::atom_type::I32; break;
			case bin_op_type::LT:  return types::atom_type::I32; break;
			case bin_op_type::LTE: return types::atom_type::I32; break;
			case bin_op_type::GT:  return types::atom_type::I32; break;
			case bin_op_type::GTE: return types::atom_type::I32; break;
			case bin_op_type::SUB: return types::atom_type::I32; break;
			case bin_op_type::ADD: return types::atom_type::I32; break;
			case bin_op_type::MUL: return types::atom_type::I32; break;
			case bin_op_type::DIV: return types::atom_type::I32; break;
			case bin_op_type::MOD: return types::atom_type::I32; break;
			}
		};

		static constexpr types::atom_type op_rhs_type(bin_op_type op)
		{
			switch (op) {
			case bin_op_type::EQ:  return types::atom_type::I32; break;
			case bin_op_type::LT:  return types::atom_type::I32; break;
			case bin_op_type::LTE: return types::atom_type::I32; break;
			case bin_op_type::GT:  return types::atom_type::I32; break;
			case bin_op_type::GTE: return types::atom_type::I32; break;
			case bin_op_type::SUB: return types::atom_type::I32; break;
			case bin_op_type::ADD: return types::atom_type::I32; break;
			case bin_op_type::MUL: return types::atom_type::I32; break;
			case bin_op_type::DIV: return types::atom_type::I32; break;
			case bin_op_type::MOD: return types::atom_type::I32; break;
			}
		};

		template<bin_op_type Op>
		struct bin_op : public node
		{
			static constexpr types::atom_type lhs_t = op_lhs_type(Op), rhs_t = op_rhs_type(Op), res_t = op_res_type(Op);

			bin_op(std::vector<unique_node>&& children) :
				node(new types::unset()),
				lhs(std::move(children.at(0))),
				rhs(std::move(children.at(1))),
				scope_depth(0)
			{}

			bin_op(const bin_op& other) :
				node(other),
				lhs(other.lhs->copy()),
				rhs(other.rhs->copy()),
				scope_depth(other.scope_depth)
			{}
			bin_op& operator=(const bin_op& other)
			{
				set_type(types::unique_type(other.get_type().copy()));
				this->lhs = unique_node(other.lhs->copy());
				this->rhs = unique_node(other.rhs->copy());
				this->scope_depth = other.scope_depth;
				return *this;
			}

			bin_op(bin_op&& other) :
				node(std::move(other)),
				lhs(std::move(other.lhs)),
				rhs(std::move(other.rhs)),
				scope_depth(other.scope_depth)
			{}
			bin_op& operator=(bin_op&& other)
			{
				set_type(std::move(other.type));
				this->lhs = std::move(other.lhs);
				this->rhs = std::move(other.rhs);
				this->scope_depth = other.scope_depth;
				return *this;
			}

			void typecheck(type_environment& env) override
			{
				lhs->typecheck(env);
				rhs->typecheck(env);

				if (!(lhs->get_type() == &types::atom<lhs_t>()))
				{
					throw typecheck_error{ std::string("Lhs of ").append(op_func(Op))
						.append(" operator must be of type ").append(types::atom_type_str(lhs_t)) };
				}

				if (!(rhs->get_type() == &types::atom<rhs_t>()))
				{
					throw typecheck_error{ std::string("Rhs of ").append(op_func(Op))
						.append(" operator must be of type ").append(types::atom_type_str(rhs_t)) };
				}

				set_type(types::make_unique(types::atom<res_t>()));
			}

			void resolve(scope_environment& s_env) override
			{
				this->lhs->resolve(s_env);
				this->rhs->resolve(s_env);
				this->scope_depth = s_env.depth() - 1;
			}

			core_ast::node* lower() override
			{
				std::vector<core_ast::unique_node> lowered_children;
				lowered_children.push_back(core_ast::unique_node(lhs->lower()));
				lowered_children.push_back(core_ast::unique_node(rhs->lower()));
				types::product_type p;
				p.product.push_back(types::make_unique(types::atom<lhs_t>()));
				p.product.push_back(types::make_unique(types::atom<rhs_t>()));

				std::string name = std::string(op_func(Op));

				auto id = core_ast::identifier{ {}, name, {}, scope_depth, types::make_unique(types::unset()) };
				auto tuple = core_ast::unique_node(new core_ast::tuple(
					std::move(lowered_children),
					types::make_unique(std::move(p))
				));
				auto type = types::unique_type(new types::atom<res_t>());

				return new core_ast::function_call(std::move(id), std::move(tuple), std::move(type));
			}

			node* copy() override
			{
				return new bin_op<Op>(*this);
			}

			unique_node lhs, rhs;
			std::size_t scope_depth;
		};

		using equality = bin_op<bin_op_type::EQ>;
		using less_than = bin_op<bin_op_type::LT>;
		using less_than_or_equal = bin_op<bin_op_type::LTE>;
		using greater_than = bin_op<bin_op_type::GT>;
		using greater_than_or_equal = bin_op<bin_op_type::GTE>;
		using subtraction = bin_op<bin_op_type::SUB>;
		using addition = bin_op<bin_op_type::ADD>;
		using multiplication = bin_op<bin_op_type::MUL>;
		using division = bin_op<bin_op_type::DIV>;
		using modulo = bin_op<bin_op_type::MOD>;

		struct while_loop : public node
		{
			while_loop(std::vector<unique_node>&& children)
				: node(new types::unset()), body(std::move(children.at(1))), test(std::move(children.at(0))) {}

			while_loop(const while_loop& other) : node(other), body(other.body->copy()), test(other.test->copy()) {}

			while_loop(while_loop&& other) : node(std::move(other)), body(std::move(other.body)),
				test(std::move(other.test)) {}
			while_loop& operator=(while_loop&& other)
			{
				set_type(other.get_type().copy());
				this->body = std::move(other.body);
				this->test = std::move(other.test);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override
			{
				return new core_ast::while_loop(
					core_ast::unique_node(test->lower()),
					core_ast::unique_node(body->lower()),
					this->get_type());
			}
			node* copy() override { return new while_loop(*this); }

			unique_node test;
			unique_node body;
		};

		struct if_statement : public node
		{
			if_statement(std::vector<unique_node>&& children)
				: node(new types::unset()), body(std::move(children.at(1))), test(std::move(children.at(0)))
			{}

			// Copy
			if_statement(const if_statement& other) : node(other), body(other.body->copy()),
				test(other.test->copy()) {}

			// Move
			if_statement(if_statement&& other) : node(std::move(other)), body(std::move(other.body)),
				test(std::move(other.test)) {}
			if_statement& operator=(if_statement&& other)
			{
				set_type(other.get_type().copy());
				this->body = std::move(other.body);
				this->test = std::move(other.test);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override
			{
				std::vector<std::pair<core_ast::unique_node, core_ast::unique_node>> new_branches;

				new_branches.emplace_back(
					core_ast::unique_node(test->lower()),
					core_ast::unique_node(body->lower())
				);

				return new core_ast::branch(std::move(new_branches));
			}
			node* copy() override { return new if_statement(*this); }

			unique_node test;
			unique_node body;
		};

		struct import_declaration : public node
		{
			import_declaration(std::vector<unique_node>&& children) : node(new types::unset())
			{
				for (auto&& child : children) modules.push_back(std::move(*dynamic_cast<identifier*>(child.get())));
			}

			// Copy
			import_declaration(const import_declaration& other) : node(other), modules(other.modules) {}
			import_declaration& operator=(const import_declaration& other)
			{
				node::operator=(other);
				this->modules = other.modules;
				return *this;
			}

			// Move
			import_declaration(import_declaration&& other) : node(std::move(other)), modules(std::move(other.modules)) {}
			import_declaration& operator=(import_declaration&& other)
			{
				node::operator=(std::move(other));
				this->modules = std::move(other.modules);
				return *this;
			}

			void typecheck(type_environment& env) override;
			void resolve(scope_environment& s_env) override;
			core_ast::node* lower() override { return new core_ast::no_op(); }
			node* copy() override { return new import_declaration(*this); }

			std::vector<identifier> modules;
		};
	}

	namespace detail
	{
		template <class T>
		static void hash_combine(std::size_t & s, const T & v)
		{
			std::hash<T> h;
			s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
		}

		template<class T> struct node_hasher;

		template<>
		struct node_hasher<extended_ast::identifier>
		{
			std::size_t operator()(const extended_ast::identifier& id) const
			{
				std::size_t res = 0;
				for (auto& r : id.segments) hash_combine<std::string>(res, r);
				for (auto& r : id.offsets) hash_combine<int>(res, r);
				hash_combine<std::size_t>(res, id.scope_distance.value_or(0));
				return res;
			}
		};
	}
}
