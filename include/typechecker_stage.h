#pragma once
#include "core_ast.h"
#include "extended_ast.h"
#include <memory>

namespace fe
{
	using namespace types;

	struct type_environment
	{
		void set(const std::string& name, type type)
		{
			types.insert_or_assign(name, type);
		}

		type get(const std::string& name)
		{
			return types.at(name);
		}

	private:
		std::unordered_map<std::string, type> types;
	};

	class typechecker_stage : public language::typechecking_stage<extended_ast::node_p, extended_ast::node_p>
	{
	public:
		extended_ast::node_p typecheck(extended_ast::node_p extended_ast) override
		{
			auto t_env = type_environment{};
			t_env.set("Type", product_type({ integer_type(), integer_type(), product_type({string_type(), integer_type(), integer_type()}), product_type({}), integer_type(), string_type() }));

			return std::get<0>(typecheck(std::move(extended_ast), std::move(t_env)));
		}

		std::tuple<extended_ast::node_p, type_environment> typecheck(extended_ast::node_p n, type_environment&& t_env)
		{
			if (auto tuple = dynamic_cast<extended_ast::tuple*>(n.get()))
			{
				auto new_node = std::make_unique<extended_ast::tuple>();
				auto new_type = product_type({});

				for (decltype(auto) element : tuple->get_children())
				{
					extended_ast::node_p new_element;
					std::tie(new_element, t_env) = typecheck(std::move(element), std::move(t_env));
					new_type.product.push_back(new_element->type);
					new_node->add(std::move(new_element));
				}

				new_node->type = new_type;

				return std::make_tuple(
					std::move(new_node),
					std::move(t_env
					));
			}
			else if (auto id = dynamic_cast<extended_ast::identifier*>(n.get()))
			{
				id->type = t_env.get(id->name);
				return std::make_tuple(
					std::move(n),
					std::move(t_env)
				);
			}
			else if (auto assignment = dynamic_cast<extended_ast::assignment*>(n.get()))
			{
				// Typecheck value
				extended_ast::node_p new_value;
				std::tie(new_value, t_env) = typecheck(std::move(assignment->value), std::move(t_env));

				// Put id type in env
				t_env.set(assignment->id.name, new_value->type);
				assignment->id.type = new_value->type;

				return std::make_tuple(
					std::make_unique<extended_ast::assignment>(
						std::move(assignment->id),
						std::move(new_value)
						),
					std::move(t_env)
				);
			}
			else if (auto fc = dynamic_cast<extended_ast::function_call*>(n.get()))
			{
				extended_ast::node_p typechecked_params;
				std::tie(typechecked_params, t_env) = typecheck(
					std::move(std::make_unique<extended_ast::tuple>(std::move(fc->params))),
					std::move(t_env)
				);

				auto env_type = t_env.get(fc->id.name);

				if (!(typechecked_params->type == env_type))
				{
					throw std::runtime_error("Type error");
				}

				fc->type = env_type;

				return std::make_tuple(
					std::make_unique<extended_ast::function_call>(
						std::move(fc->id),
						std::move(*dynamic_cast<extended_ast::tuple*>(typechecked_params.get())),
						std::get<1>(env_type)
						),
					std::move(t_env)
				);
			}
			// TODO can we remove constructor use function call instead?
			else if (auto constructor = dynamic_cast<extended_ast::constructor*>(n.get()))
			{
				extended_ast::node_p typechecked_value;
				std::tie(typechecked_value, t_env) = typecheck(
					std::make_unique<extended_ast::tuple>(std::move(constructor->value)),
					std::move(t_env)
				);

				auto env_type = t_env.get(constructor->id.name);

				if (!(typechecked_value->type == env_type))
				{
					throw std::runtime_error("Type error");
				}

				return std::make_tuple(
					std::make_unique<extended_ast::constructor>(
						std::move(constructor->id),
						std::move(*dynamic_cast<extended_ast::tuple*>(typechecked_value.get()))
						),
					std::move(t_env)
				);
			}
			else if (auto integer = dynamic_cast<extended_ast::integer*>(n.get()))
			{
				integer->type = types::integer_type();
				return std::make_tuple(std::move(n), std::move(t_env));
			}
			else if (auto string = dynamic_cast<extended_ast::string*>(n.get()))
			{
				string->type = types::string_type();
				return std::make_tuple(std::move(n), std::move(t_env));
			}

			throw std::runtime_error("Unknown node type");
		}
	};
}