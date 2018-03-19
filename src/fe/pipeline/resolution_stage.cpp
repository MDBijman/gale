#include "fe/data/extended_ast.h"
#include "fe/data/scope_environment.h"
#include <locale>

namespace fe::extended_ast
{
	void integer::resolve(scope_environment& s_env) {}

	void string::resolve(scope_environment& s_env) {}

	void identifier::resolve(scope_environment& s_env)
	{
		auto access_pattern = s_env.resolve_reference(*this);

		if(!access_pattern.has_value())
			throw resolution_error{ std::string("Cannot resolve reference: ").append(this->to_string()) };

		this->scope_distance = access_pattern.value().first;
		this->offsets = access_pattern.value().second;
	}

	void tuple::resolve(scope_environment& s_env)
	{
		for (decltype(auto) elem : this->children) elem->resolve(s_env);
	}

	void function_call::resolve(scope_environment& s_env)
	{
		if(auto access_pattern = s_env.resolve_reference(this->id); access_pattern.has_value())
		{
			this->id.scope_distance = access_pattern.value().first;
			this->id.offsets = access_pattern.value().second;
		}
		else if (auto access_pattern = s_env.resolve_type(this->id); access_pattern.has_value())
		{
			this->id.scope_distance = access_pattern.value().first;
		}
		else
		{
			throw resolution_error{ "Cannot resolve function call name" };
		}

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
		for (decltype(auto) branch : this->branches) branch.resolve(s_env);
	}

	void block::resolve(scope_environment& s_env)
	{
		s_env.push();
		for (decltype(auto) child : this->children) child->resolve(s_env);
		s_env.pop();
	}

	void module_declaration::resolve(scope_environment& s_env) {}

	void atom_declaration::resolve(scope_environment& s_env){}

	void tuple_declaration::resolve(scope_environment& s_env)
	{
		for (decltype(auto) elem : this->elements) elem->resolve(s_env);
	}

	void function::resolve(scope_environment& s_env)
	{
		s_env.declare(this->name, extended_ast::identifier({ "_function" }));
		s_env.define(this->name);

		s_env.push();
		this->from->resolve(s_env);

		// Define parameters
		std::function<void(node*, scope_environment& s_env)> define = [&](node* n, scope_environment& s_env) -> void {
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
					s_env.declare(atom_dec->name, *dynamic_cast<identifier*>(type_expression_name->type.get()));
					s_env.define(atom_dec->name);
				}
				else
				{
					throw resolution_error{ "Type expression name resolution not supported yet" };
				}
			}
		};
		define(this->from.get(), s_env);

		this->to->resolve(s_env);
		this->body->resolve(s_env);
		s_env.pop();
	}

	void type_definition::resolve(scope_environment& s_env)
	{
		s_env.define_type(this->id, this->types);
	}

	void export_stmt::resolve(scope_environment& s_env)
	{
		for (auto& child : this->names)
		{
			s_env.resolve_type(child);
		}
	}

	void identifier_tuple::resolve(scope_environment& s_env) {}

	void declaration::resolve(scope_environment& s_env)
	{
		if (std::holds_alternative<extended_ast::identifier>(this->lhs))
		{
			auto& lhs_id = std::get<extended_ast::identifier>(this->lhs);
			s_env.declare(lhs_id, this->type_name);
			this->value->resolve(s_env);
			s_env.define(lhs_id);
		}
		else
		{
			std::function<void(decltype(this->lhs))> resolve 
				= [this, &s_env, &resolve](const std::variant<identifier, identifier_tuple>& lhs) 
			{
				if (std::holds_alternative<extended_ast::identifier>(lhs))
				{
					auto& lhs_id = std::get<extended_ast::identifier>(lhs);
					s_env.declare(lhs_id, extended_ast::identifier({ "_defer" }));
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

			this->value->resolve(s_env);

			std::function<void(decltype(this->lhs))> define
				= [this, &s_env, &define](const std::variant<identifier, identifier_tuple>& lhs)
			{
				if (std::holds_alternative<extended_ast::identifier>(lhs))
				{
					s_env.define(std::get<extended_ast::identifier>(lhs));
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

	void assignment::resolve(scope_environment& s_env)
	{
		this->lhs.resolve(s_env);
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

	void while_loop::resolve(scope_environment& s_env)
	{
		this->test->resolve(s_env);
		this->body->resolve(s_env);
	}

	void if_statement::resolve(scope_environment& s_env)
	{
		s_env.push();
		this->test->resolve(s_env);
		this->body->resolve(s_env);
		s_env.pop();
	}

	void import_declaration::resolve(scope_environment& s_env) {}
}
