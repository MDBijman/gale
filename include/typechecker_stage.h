#pragma once
#include <memory>

#include "core_ast.h"
#include "extended_ast.h"

namespace fe
{
	class typechecker_stage : public language::typechecking_stage<extended_ast::node_p, extended_ast::node_p, fe::environment>
	{
	private:
		type_environment base_environment;

	public:
		typechecker_stage() {}
		typechecker_stage(type_environment environment) : base_environment(environment) {}

		std::tuple<extended_ast::node_p, fe::environment> typecheck(extended_ast::node_p n, fe::environment env) override
		{
			using namespace types;
		
			if (auto tuple = dynamic_cast<extended_ast::tuple*>(n.get()))
			{
				auto new_type = product_type();

				for (decltype(auto) element : tuple->get_children())
				{
					std::tie(element, env) = typecheck(std::move(element), std::move(env));
					new_type.product.push_back(element->type);
				}

				tuple->type = new_type;

				return std::make_tuple(
					std::move(n),
					std::move(env)
				);
			}
			else if (auto id = dynamic_cast<extended_ast::identifier*>(n.get()))
			{
				id->type = env.typeof(id->name);

				return std::make_tuple(
					std::move(n),
					std::move(env)
				);
			}
			else if (auto assignment = dynamic_cast<extended_ast::assignment*>(n.get()))
			{
				// Typecheck value
				std::tie(assignment->value, env) = typecheck(std::move(assignment->value), std::move(env));

				// Put id type in env
				env.set_type(assignment->id.name, assignment->value->type);
				assignment->id.type = assignment->value->type;
				
				assignment->type = types::void_type();

				return std::make_tuple(
					std::move(n),
					std::move(env)
				);
			}
			else if (auto fc = dynamic_cast<extended_ast::function_call*>(n.get()))
			{
				auto typechecked_params = extended_ast::node_p();
				std::tie(typechecked_params, env) = typecheck(
					std::move(std::make_unique<extended_ast::tuple>(std::move(fc->params))),
					std::move(env)
				);

				fc->params = std::move(*dynamic_cast<extended_ast::tuple*>(typechecked_params.get()));

				auto env_fn_type = std::get<5>(env.typeof(fc->id.name));

				// Check function signature against call signature
				if (!(std::get<1>(fc->params.type) == env_fn_type.from))
				{
					throw std::runtime_error("Type error");
				}

				fc->type = env_fn_type;

				return std::make_tuple(
					std::move(n),
					std::move(env)
				);
			}
			else if (auto export_stmt = dynamic_cast<extended_ast::export_stmt*>(n.get()))
			{
				export_stmt->type = types::void_type();
				//env.t_env.set(export_stmt->name, env.);
				return std::make_tuple(std::move(n), std::move(env));
			}
			else if (auto type_declaration = dynamic_cast<extended_ast::type_declaration*>(n.get()))
			{
				type_declaration->type = types::void_type();

				extended_ast::node_p typechecked_tuple;
				std::tie(typechecked_tuple, env) = typecheck(std::make_unique<extended_ast::tuple>(std::move(type_declaration->types)), std::move(env));
				type_declaration->types = std::move(*dynamic_cast<extended_ast::tuple*>(typechecked_tuple.get()));
			
				env.set_type(type_declaration->id.name, types::function_type(
					std::get<1>(type_declaration->types.type), std::get<1>(type_declaration->types.type)
				));

				return std::make_tuple(std::move(n), std::move(env));
			}
			else if (auto integer = dynamic_cast<extended_ast::integer*>(n.get()))
			{
				integer->type = types::integer_type();
				return std::make_tuple(std::move(n), std::move(env));
			}
			else if (auto string = dynamic_cast<extended_ast::string*>(n.get()))
			{
				string->type = types::string_type();
				return std::make_tuple(std::move(n), std::move(env));
			}

			throw std::runtime_error("Unknown node type");
		}
	};
}