#include "fe/data/extended_ast.h"
#include "fe/data/scope_environment.h"

namespace fe::extended_ast
{
	void integer::resolve(scope_environment& s_env) {}

	void string::resolve(scope_environment& s_env) {}

	void identifier::resolve(scope_environment& s_env)
	{
		this->scope_distance = s_env.resolve(*this);
	}

	void tuple::resolve(scope_environment& s_env)
	{
		for (auto& elem : this->children) elem->resolve(s_env);
	}

	void function_call::resolve(scope_environment& s_env)
	{
		this->params->resolve(s_env);
	}

	void match_branch::resolve(scope_environment& s_env)
	{
		s_env.push();
		this->test_path->resolve(s_env);
		this->code_path->resolve(s_env);
		s_env.pop();
	}

	void match::resolve(scope_environment& s_env)
	{
		this->expression->resolve(s_env);
		for (auto& branch : this->branches) branch.resolve(s_env);
	}

	void block::resolve(scope_environment& s_env)
	{
		s_env.push();
		for (auto& child : this->children) child->resolve(s_env);
		s_env.pop();
	}

	void module_declaration::resolve(scope_environment& s_env) {}

	void atom_declaration::resolve(scope_environment& s_env)
	{
		s_env.declare(this->name);
	}

	void tuple_declaration::resolve(scope_environment& s_env)
	{
		for (auto& elem : this->elements) elem->resolve(s_env);
	}

	void function::resolve(scope_environment& s_env)
	{
		s_env.push();
		this->from->resolve(s_env);
		this->to->resolve(s_env);
		this->body->resolve(s_env);
		s_env.pop();
	}

	void type_definition::resolve(scope_environment& s_env) {}

	void export_stmt::resolve(scope_environment& s_env)
	{
		for (auto& child : this->names) child.resolve(s_env);
	}

	void identifier_tuple::resolve(scope_environment& s_env)
	{
		for (auto& child : this->content)
		{
			if (std::holds_alternative<identifier>(child)) s_env.define(std::get<identifier>(child));
			else std::get<identifier_tuple>(child).resolve(s_env);
		}
	}

	void assignment::resolve(scope_environment& s_env)
	{
		if (std::holds_alternative<identifier>(this->lhs)) s_env.define(std::get<identifier>(this->lhs));
		else std::get<identifier_tuple>(this->lhs).resolve(s_env);

		this->value->resolve(s_env);
	}

	void type_tuple::resolve(scope_environment& s_env) {}
	void type_atom::resolve(scope_environment& s_env) {}
	void function_type::resolve(scope_environment& s_env) {}
	void reference_type::resolve(scope_environment& s_env) {}
	void array_type::resolve(scope_environment& s_env) {}

	void reference::resolve(scope_environment& s_env)
	{
		this->child->resolve(s_env);
	}

	void array_value::resolve(scope_environment& s_env)
	{
		for (auto& child : this->children)
			child->resolve(s_env);
	}

	void equality::resolve(scope_environment& s_env)
	{
		this->left->resolve(s_env);
		this->right->resolve(s_env);
	}

	void addition::resolve(scope_environment& s_env)
	{
		this->left->resolve(s_env);
		this->right->resolve(s_env);
	}

	void subtraction::resolve(scope_environment& s_env)
	{
		this->left->resolve(s_env);
		this->right->resolve(s_env);
	}

	void multiplication::resolve(scope_environment& s_env)
	{
		this->left->resolve(s_env);
		this->right->resolve(s_env);
	}

	void division::resolve(scope_environment& s_env)
	{
		this->left->resolve(s_env);
		this->right->resolve(s_env);
	}

	void array_index::resolve(scope_environment& s_env)
	{
		this->array_exp->resolve(s_env);
		this->index_exp->resolve(s_env);
	}

	void while_loop::resolve(scope_environment& s_env)
	{
		this->test->resolve(s_env);
		this->body->resolve(s_env);
	}

	void import_declaration::resolve(scope_environment& s_env) {}
}