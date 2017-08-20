#pragma once
#include <memory>

#include "core_ast.h"
#include "extended_ast.h"

namespace fe
{
	struct typecheck_error
	{

	};

	class typechecker_stage : public language::typechecking_stage<extended_ast::node, extended_ast::node, fe::environment, typecheck_error>
	{
	private:
		type_environment base_environment;

	public:
		typechecker_stage() {}
		typechecker_stage(type_environment environment) : base_environment(environment) {}

		std::variant<std::tuple<extended_ast::node, fe::environment>, typecheck_error> typecheck(extended_ast::node n, environment env) override
		{
			using namespace types;
		
			if (std::holds_alternative<extended_ast::value_tuple>(n))
			{
				auto value_tuple = std::move(std::get<extended_ast::value_tuple>(n));
				auto new_type = product_type();

				for (decltype(auto) element : value_tuple.children)
				{
					auto checked_element = typecheck(std::move(element), std::move(env));
					element = std::move(std::get<extended_ast::node>(checked_element));
					env = std::move(std::get<environment>(checked_element));

					new_type.product.push_back(std::visit(extended_ast::get_type, element));
				}
				
				value_tuple.type = std::move(new_type);

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
				assignment.value = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_value)));
				env = std::move(std::get<environment>(checked_value));

				// Put id type in env
				auto type = std::visit(extended_ast::get_type, *assignment.value);
				env.set_type(assignment.id.name, type);
				assignment.id.type = std::move(type);

				assignment.type = void_type();

				return std::make_tuple(
					std::move(assignment),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::function_call>(n))
			{
				auto& fc = std::get<extended_ast::function_call>(n);

				auto res = typecheck(
					std::move(fc.params),
					std::move(env)
				);

				if (std::holds_alternative<typecheck_error>(res))
					return std::get<typecheck_error>(res);
				auto checked_params = std::move(std::get<std::tuple<extended_ast::node, fe::environment>>(res));

				env = std::move(std::get<environment>(checked_params));
				fc.params = std::move(std::get<extended_ast::value_tuple>(std::get<extended_ast::node>(checked_params)));

				auto env_fn_type = std::get<types::function_type>(env.typeof(fc.id.name));

				// Check function signature against call signature
				if (!(fc.params.type == *env_fn_type.from))
				{
					throw std::runtime_error("Type error");
				}

				fc.type = std::move(*env_fn_type.to);

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
				type_declaration.types.type = std::move(type);

				auto ft = function_type(
					types::make_unique(type_declaration.types.type),
					types::make_unique(type_declaration.types.type)
				);

				env.set_type(type_declaration.id.name, std::move(ft));

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
				operation.left = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_left)));

				auto checked_right = typecheck(std::move(*operation.right), std::move(env));
				env = std::move(std::get<environment>(checked_right));
				operation.right = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_right)));

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
			return std::get<values::type>(value).kind;
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