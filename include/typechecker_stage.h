#pragma once
#include <memory>

#include "core_ast.h"
#include "extended_ast.h"

namespace fe
{
	class typechecker_stage : public language::typechecking_stage<extended_ast::node_v, extended_ast::node_v, fe::environment>
	{
	private:
		type_environment base_environment;

	public:
		typechecker_stage() {}
		typechecker_stage(type_environment environment) : base_environment(environment) {}

		std::tuple<extended_ast::node_v, fe::environment> typecheck(extended_ast::node_v n, environment env) override
		{
			using namespace types;
		
			if (std::holds_alternative<extended_ast::value_tuple>(n))
			{
				auto value_tuple = std::move(std::get<extended_ast::value_tuple>(n));
				auto new_type = product_type();

				for (decltype(auto) element : value_tuple.children)
				{
					std::tie(element, env) = typecheck(std::move(element), std::move(env));
					new_type.product.push_back(std::visit(extended_ast::get_type, element));
				}
				
				value_tuple.type = new_type;

				return std::make_tuple(
					std::move(value_tuple),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::identifier>(n))
			{
				auto id = std::move(std::get<extended_ast::identifier>(n));
				id.type = env.typeof(id.name);

				return std::make_tuple(
					std::move(id),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::assignment>(n))
			{
				auto assignment = std::move(std::get<extended_ast::assignment>(n));

				// Typecheck value
				auto checked_value = typecheck(std::move(*assignment.value), std::move(env));
				assignment.value = std::make_unique<extended_ast::node_v>(std::move(std::get<0>(checked_value)));
				env = std::move(std::get<1>(checked_value));

				// Put id type in env
				auto type = std::visit(extended_ast::get_type, *assignment.value);
				env.set_type(assignment.id.name, type);
				assignment.id.type = type;

				assignment.type = void_type();

				return std::make_tuple(
					std::move(assignment),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::function_call>(n))
			{
				auto& fc = std::get<extended_ast::function_call>(n);

				auto checked_params = typecheck(
					std::move(fc.params),
					std::move(env)
				);
				env = std::get<1>(checked_params);
				fc.params = std::move(std::get<extended_ast::value_tuple>(std::get<0>(checked_params)));

				auto env_fn_type = std::get<5>(env.typeof(fc.id.name));

				// Check function signature against call signature
				if (!(std::get<1>(fc.params.type) == env_fn_type.from))
				{
					throw std::runtime_error("Type error");
				}

				fc.type = env_fn_type.to;

				return std::make_tuple(
					std::move(fc),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::export_stmt>(n))
			{
				auto export_stmt = std::move(std::get<extended_ast::export_stmt>(n));
				export_stmt.type = void_type();
				return std::make_tuple(std::move(export_stmt), std::move(env));
			}
			else if (std::holds_alternative<extended_ast::type_declaration>(n))
			{
				auto type_declaration = std::move(std::get<extended_ast::type_declaration>(n));
				type_declaration.type = void_type();

				auto type = interpret(type_declaration.types, env);
				type_declaration.types.type = type;

				env.set_type(type_declaration.id.name, function_type(
					std::get<product_type>(type_declaration.types.type), 
					std::get<product_type>(type_declaration.types.type)
				));

				return std::make_tuple(std::move(type_declaration), std::move(env));
			}
			else if (std::holds_alternative<extended_ast::integer>(n))
			{
				std::get<extended_ast::integer>(n).type = integer_type();
				return std::make_tuple(std::move(n), std::move(env));
			}
			else if (std::holds_alternative<extended_ast::string>(n))
			{
				std::get<extended_ast::string>(n).type = string_type();
				return std::make_tuple(std::move(n), std::move(env));
			}
			else if (std::holds_alternative<extended_ast::bin_op>(n))
			{
				auto& operation = std::get<extended_ast::bin_op>(n);

				auto checked_left = typecheck(std::move(*operation.left), std::move(env));
				env = std::move(std::get<environment>(checked_left));
				operation.left = std::make_unique<extended_ast::node_v>(std::move(std::get<extended_ast::node_v>(checked_left)));

				auto checked_right = typecheck(std::move(*operation.right), std::move(env));
				env = std::move(std::get<environment>(checked_right));
				operation.right = std::make_unique<extended_ast::node_v>(std::move(std::get<extended_ast::node_v>(checked_right)));

				auto left_type = std::visit(extended_ast::get_type, *(operation.left));
				auto right_type = std::visit(extended_ast::get_type, *(operation.right));
				if (!(left_type == types::type(types::integer_type())) || !(right_type == types::type(types::integer_type())))
				{
					throw std::runtime_error("Cannot perform binary operation, types must both be int");
				}

				operation.type = types::type(types::integer_type());
				return std::make_tuple(std::move(operation), std::move(env));
			}

			throw std::runtime_error("Unknown node type");
		}

		types::type interpret(const extended_ast::identifier& identifier, const fe::environment& env)
		{
			auto& type = env.typeof(identifier.name);
			if (!std::holds_alternative<types::meta_type>(type))
			{
				throw std::runtime_error("Identifier does not reference a type");
			}

			auto& value = env.valueof(identifier.name);
			return dynamic_cast<values::type*>(value.get())->kind;
		}

		types::type interpret(const extended_ast::type_tuple& tuple, const fe::environment& env)
		{
			types::product_type result;
			for (const auto& child : tuple.children)
			{
				if (std::holds_alternative<extended_ast::identifier>(child))
				{
					result.product.push_back(interpret(std::get<extended_ast::identifier>(child), env));
				}
				else if (std::holds_alternative<extended_ast::type_tuple>(child))
				{
					result.product.push_back(interpret(std::get<extended_ast::type_tuple>(child), env));
				}
			}
			return result;
		}

	};
}