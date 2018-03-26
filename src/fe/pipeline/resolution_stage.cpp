#include "fe/data/extended_ast.h"
#include "fe/data/scope_environment.h"

namespace fe::extended_ast
{
	resolution::scope_tree_node* literal::build_scope_tree() { return new resolution::unscoped_node(); }

	void literal::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* identifier::build_scope_tree() { return new resolution::unscoped_node(); }

	void identifier::resolve(resolution::scope_tree_node& s_env)
	{
		auto var_lookup = s_env.resolve_var_id(*this);

		if (!var_lookup.has_value())
			throw resolution_error{ std::string("Cannot resolve reference: ").append(this->to_string()) };
		this->scope_distance = var_lookup.value().scope_distance;

		auto type_lookup = s_env.resolve_type(var_lookup.value().type_name);
		assert(type_lookup.has_value());

		this->offsets = type_lookup.value().type_structure.resolve(this->without_first_segment()).value();
	}

	resolution::scope_tree_node* tuple::build_scope_tree() 
	{ 
		auto n = new resolution::unscoped_node();

		for (auto& child : this->children)
		{
			auto child_n = child->build_scope_tree();
			child_n->set_parent(n);
			n->add_child(child_n);
		}

		return n;
	}

	void tuple::resolve(resolution::scope_tree_node& s_env)
	{
		size_t i = 0;
		for (decltype(auto) elem : this->children)
		{
			elem->resolve(s_env[i]);
			i++;
		}
	}

	resolution::scope_tree_node* function_call::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto param_tree = this->params->build_scope_tree();
		param_tree->set_parent(n);
		n->add_child(param_tree);

		return n;
	}

	void function_call::resolve(resolution::scope_tree_node& s_env)
	{
		if (auto access_pattern = s_env.resolve_var_id(this->id); access_pattern.has_value())
		{
			this->id.scope_distance = access_pattern.value().scope_distance;

			auto type_lookup = s_env.resolve_type(access_pattern.value().type_name);
			assert(type_lookup.has_value());

			this->id.offsets = type_lookup.value().type_structure.resolve(this->id.without_first_segment()).value();
		}
		else if (auto access_pattern = s_env.resolve_type(this->id); access_pattern.has_value())
		{
			this->id.scope_distance = access_pattern.value().scope_distance;
		}
		else
		{
			throw resolution_error{ "Cannot resolve function call name" };
		}

		this->params->resolve(s_env[0]);
	}

	resolution::scope_tree_node* match_branch::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto test_tree = this->test_path->build_scope_tree();
		test_tree->set_parent(n);
		n->add_child(test_tree);

		auto code_tree = this->code_path->build_scope_tree();
		code_tree->set_parent(n);
		n->add_child(code_tree);

		return n;
	}

	void match_branch::resolve(resolution::scope_tree_node& s_env)
	{
		this->test_path->resolve(s_env[0]);
		this->code_path->resolve(s_env[1]);
	}

	resolution::scope_tree_node* match::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto exp_tree = this->expression->build_scope_tree();
		exp_tree->set_parent(n);
		n->add_child(exp_tree);

		for (auto& branch : this->branches)
		{
			auto branch_tree = branch.build_scope_tree();
			branch_tree->set_parent(n);
			n->add_child(branch_tree);
		}

		return n;
	}

	void match::resolve(resolution::scope_tree_node& s_env)
	{
		this->expression->resolve(s_env[0]);

		size_t i = 1;
		for (decltype(auto) branch : this->branches) 
		{
			branch.resolve(s_env[i]);
			i++;
		}
	
	}

	resolution::scope_tree_node* block::build_scope_tree()
	{
		auto n = new resolution::scoped_node();

		for (decltype(auto) child : this->children)
		{
			auto child_tree = child->build_scope_tree();
			child_tree->set_parent(n);
			n->add_child(child_tree);
		}

		return n;
	}

	void block::resolve(resolution::scope_tree_node& s_env)
	{
		size_t i = 0;
		for (decltype(auto) child : this->children)
		{
			child->resolve(s_env[i]);
			i++;
		}
	}

	resolution::scope_tree_node* module_declaration::build_scope_tree() { return new resolution::unscoped_node(); }

	void module_declaration::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* atom_declaration::build_scope_tree() 
	{ 
		auto n = new resolution::unscoped_node(); 

		auto exp_tree = this->type_expression->build_scope_tree();
		exp_tree->set_parent(n);
		n->add_child(exp_tree);

		return n;
	}

	void atom_declaration::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* tuple_declaration::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		for (decltype(auto) child : this->elements)
		{
			auto child_tree = child->build_scope_tree();
			child_tree->set_parent(n);
			n->add_child(child_tree);
		}

		return n;
	}

	void tuple_declaration::resolve(resolution::scope_tree_node& s_env)
	{
		size_t i = 0;
		for (decltype(auto) elem : this->elements)
		{
			elem->resolve(s_env[i]);
			i++;
		}
	}

	resolution::scope_tree_node* function::build_scope_tree()
	{
		auto n = new resolution::scoped_node();

		auto from_tree = this->from->build_scope_tree();
		from_tree->set_parent(n);
		n->add_child(from_tree);

		auto to_tree = this->to->build_scope_tree();
		to_tree->set_parent(n);
		n->add_child(to_tree);

		auto body_tree = this->body->build_scope_tree();
		body_tree->set_parent(n);
		n->add_child(body_tree);

		return n;
	}

	void function::resolve(resolution::scope_tree_node& s_env)
	{
		s_env.declare_var_id(this->name.segments[0], extended_ast::identifier("_function"));
		s_env.define_var_id(this->name.segments[0]);

		this->from->resolve(s_env[0]);

		// Define parameters
		std::function<void(node*, resolution::scope_tree_node& s_env)> define = 
			[&](node* n, resolution::scope_tree_node& s_env) -> void 
		{
			if (auto tuple_dec = dynamic_cast<tuple_declaration*>(n))
			{
				for (decltype(auto) child : tuple_dec->elements)
				{
					define(child.get(), s_env);
				}
			}
			else if (auto atom_dec = dynamic_cast<atom_declaration*>(n))
			{
				if (auto type_expression_name = dynamic_cast<type_atom*>(atom_dec->type_expression.get()))
				{
					s_env.declare_var_id(atom_dec->name.segments[0], 
						*dynamic_cast<identifier*>(type_expression_name->type.get()));
					s_env.define_var_id(atom_dec->name.segments[0]);
				}
				else
				{
					throw resolution_error{ "Type expression name resolution not supported yet" };
				}
			}
		};
		define(this->from.get(), s_env);

		this->to->resolve(s_env[1]);
		this->body->resolve(s_env[2]);
	}

	resolution::scope_tree_node* type_definition::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto types_tree = this->types->build_scope_tree();
		types_tree->set_parent(n);
		n->add_child(types_tree);

		return n;
	}

	void type_definition::resolve(resolution::scope_tree_node& s_env)
	{
		s_env.define_type(this->id.segments[0], this->types);
	}

	resolution::scope_tree_node* export_stmt::build_scope_tree()
	{
		return new resolution::unscoped_node();
	}

	void export_stmt::resolve(resolution::scope_tree_node& s_env)
	{
		for (auto& child : this->names)
		{
			s_env.resolve_type(child);
		}
	}

	resolution::scope_tree_node* identifier_tuple::build_scope_tree()
	{
		return new resolution::unscoped_node();
	}

	void identifier_tuple::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* declaration::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto val_tree = this->value->build_scope_tree();
		val_tree->set_parent(n);
		n->add_child(val_tree);

		return n;
	}

	void declaration::resolve(resolution::scope_tree_node& s_env)
	{
		if (std::holds_alternative<extended_ast::identifier>(this->lhs))
		{
			auto& lhs_id = std::get<extended_ast::identifier>(this->lhs);
			s_env.declare_var_id(lhs_id.segments[0], this->type_name);
			this->value->resolve(s_env[0]);
			s_env.define_var_id(lhs_id.segments[0]);
		}
		else
		{
			std::function<void(decltype(this->lhs))> resolve
				= [this, &s_env, &resolve](const std::variant<identifier, identifier_tuple>& lhs)
			{
				if (std::holds_alternative<extended_ast::identifier>(lhs))
				{
					auto& lhs_id = std::get<extended_ast::identifier>(lhs);
					s_env.declare_var_id(lhs_id.segments[0], extended_ast::identifier("_defer"));
				}
				else if (std::holds_alternative<extended_ast::identifier_tuple>(lhs))
				{
					auto& lhs_ids = std::get<extended_ast::identifier_tuple>(lhs);

					for (auto& id : lhs_ids.content)
					{
						resolve(id);
					}
				}
			};
			resolve(this->lhs);

			this->value->resolve(s_env[0]);

			std::function<void(decltype(this->lhs))> define
				= [this, &s_env, &define](const std::variant<identifier, identifier_tuple>& lhs)
			{
				if (std::holds_alternative<extended_ast::identifier>(lhs))
				{
					s_env.define_var_id(std::get<extended_ast::identifier>(lhs).segments[0]);
				}
				else if (std::holds_alternative<extended_ast::identifier_tuple>(lhs))
				{
					auto& lhs_ids = std::get<extended_ast::identifier_tuple>(lhs);

					for (auto& id : lhs_ids.content)
						define(id);
				}
			};
			define(this->lhs);
		}
	}

	resolution::scope_tree_node* assignment::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto lhs_tree = this->lhs.build_scope_tree();
		lhs_tree->set_parent(n);
		n->add_child(lhs_tree);

		auto val_tree = this->value->build_scope_tree();
		val_tree->set_parent(n);
		n->add_child(val_tree);

		return n;
	}

	void assignment::resolve(resolution::scope_tree_node& s_env)
	{
		this->lhs.resolve(s_env[0]);
		this->value->resolve(s_env[1]);
	}

	resolution::scope_tree_node* type_tuple::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		for (decltype(auto) child : this->elements)
		{
			auto child_tree = child->build_scope_tree();
			child_tree->set_parent(n);
			n->add_child(child_tree);
		}

		return n;
	}

	void type_tuple::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* type_atom::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto type_tree = this->type->build_scope_tree();
		type_tree->set_parent(n);
		n->add_child(type_tree);

		return n;
	}

	void type_atom::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* function_type::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto from_tree = this->from->build_scope_tree();
		from_tree->set_parent(n);
		n->add_child(from_tree);

		auto to_tree = this->to->build_scope_tree();
		to_tree->set_parent(n);
		n->add_child(to_tree);

		return n;
	}

	void function_type::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* reference_type::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto child_type = this->child->build_scope_tree();
		child_type->set_parent(n);
		n->add_child(child_type);

		return n;
	}

	void reference_type::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* array_type::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto child_type = this->child->build_scope_tree();
		child_type->set_parent(n);
		n->add_child(child_type);

		return n;
	}

	void array_type::resolve(resolution::scope_tree_node& s_env) {}

	resolution::scope_tree_node* reference::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto child_type = this->child->build_scope_tree();
		child_type->set_parent(n);
		n->add_child(child_type);

		return n;
	}

	void reference::resolve(resolution::scope_tree_node& s_env)
	{
		this->child->resolve(s_env[0]);
	}

	resolution::scope_tree_node* array_value::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		for (decltype(auto) child : children)
		{
			auto child_type = child->build_scope_tree();
			child_type->set_parent(n);
			n->add_child(child_type);
		}

		return n;
	}

	void array_value::resolve(resolution::scope_tree_node& s_env)
	{
		size_t i = 0;
		for (auto& child : this->children)
		{
			child->resolve(s_env[i]);
			i++;
		}
	}

	resolution::scope_tree_node* while_loop::build_scope_tree()
	{
		auto n = new resolution::unscoped_node();

		auto test_tree = this->test->build_scope_tree();
		test_tree->set_parent(n);
		n->add_child(test_tree);

		auto body_tree = this->body->build_scope_tree();
		body_tree->set_parent(n);
		n->add_child(body_tree);

		return n;
	}

	void while_loop::resolve(resolution::scope_tree_node& s_env)
	{
		this->test->resolve(s_env[0]);
		this->body->resolve(s_env[1]);
	}

	resolution::scope_tree_node* if_statement::build_scope_tree()
	{
		auto n = new resolution::scoped_node();

		auto test_tree = this->test->build_scope_tree();
		test_tree->set_parent(n);
		n->add_child(test_tree);

		auto body_tree = this->body->build_scope_tree();
		body_tree->set_parent(n);
		n->add_child(body_tree);

		return n;
	}

	void if_statement::resolve(resolution::scope_tree_node& s_env)
	{
		// #todo remove extra scope here?
		this->test->resolve(s_env[0]);
		this->body->resolve(s_env[1]);
	}

	resolution::scope_tree_node* import_declaration::build_scope_tree()
	{
		return new resolution::unscoped_node();
	}

	void import_declaration::resolve(resolution::scope_tree_node& s_env) {}
}
