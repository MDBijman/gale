#pragma once
#include <memory>

#include "core_ast.h"
#include "extended_ast.h"
#include "error.h"
#include "typecheck_environment.h"

namespace fe
{
	class typechecker_stage : public language::typechecking_stage<extended_ast::node, extended_ast::node, typecheck_environment, typecheck_error>
	{
	private:
		typecheck_environment base_environment;

	public:
		typechecker_stage() {}
		typechecker_stage(typecheck_environment environment) : base_environment(environment) {}

		std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::node n, typecheck_environment env) override
		{
			using namespace types;

			if (std::holds_alternative<extended_ast::tuple>(n))
			{
				auto& tuple = std::get<extended_ast::tuple>(n);
				auto new_type = product_type();

				for (decltype(auto) element : tuple.children)
				{
					auto res = typecheck(std::move(element), std::move(env));

					if (std::holds_alternative<typecheck_error>(res))
						return std::get<typecheck_error>(res);

					auto& checked_element = std::get<std::tuple<extended_ast::node, typecheck_environment>>(res);

					element = std::move(std::get<extended_ast::node>(checked_element));
					env = std::move(std::get<typecheck_environment>(checked_element));

					new_type.product.push_back({ "", std::visit(extended_ast::get_type, element) });
				}
				
				tuple.type = std::move(new_type);

				return std::make_tuple(
					std::move(tuple),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::block>(n))
			{
				auto& block = std::get<extended_ast::block>(n);

				types::type final_type;
				for (decltype(auto) element : block.children)
				{
					auto checked_element_or_error = typecheck(std::move(element), std::move(env));
					if (std::holds_alternative<typecheck_error>(checked_element_or_error))
						return std::get<typecheck_error>(checked_element_or_error);
					
					auto& checked_element = std::get<std::tuple<extended_ast::node, typecheck_environment>>(checked_element_or_error);

					element = std::move(std::get<extended_ast::node>(checked_element));
					env = std::move(std::get<typecheck_environment>(checked_element));

					final_type = std::visit(extended_ast::get_type, element);
				}

				block.type = std::move(final_type);
				
				return std::make_tuple(
					std::move(block),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::module_declaration>(n))
			{
				auto& declaration = std::get<extended_ast::module_declaration>(n);
				std::string module_name = declaration.name.name.at(0);
				env.name = std::move(module_name);
				return std::make_pair(std::move(n), std::move(env));
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
				auto res = typecheck(std::move(*assignment.value), std::move(env));

				if (std::holds_alternative<typecheck_error>(res))
					return std::get<typecheck_error>(res);
				auto& checked_value = std::get<std::tuple<extended_ast::node, typecheck_environment>>(res);

				assignment.value = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_value)));
				env = std::move(std::get<typecheck_environment>(checked_value));

				// Put id type in env
				auto type = std::visit(extended_ast::get_type, *assignment.value);
				env.set_type(assignment.id.name.at(0), type);
				assignment.id.type = std::move(type);

				assignment.type = void_type();

				return std::make_tuple(
					std::move(assignment),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::function_call>(n))
			{
				auto& call = std::get<extended_ast::function_call>(n);

				auto res = typecheck(
					std::move(*call.params),
					std::move(env)
				);

				if (std::holds_alternative<typecheck_error>(res))
					return std::get<typecheck_error>(res);
				auto& checked_params = std::get<std::tuple<extended_ast::node, typecheck_environment>>(res);

				env = std::move(std::get<typecheck_environment>(checked_params));
				call.params = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_params)));

				auto& argument_type = std::visit(extended_ast::get_type, *call.params);

				// TODO rework field access
				// TODO implement actual type checking for get set
				if (call.id.name.size() == 1 && call.id.name.at(0) == "get")
				{
					auto& param_tuple = std::get<extended_ast::tuple>(*call.params);

					auto& typeof_tuple = env.typeof(std::get<extended_ast::identifier>(param_tuple.children.at(0)).name);
					auto& product_type = std::get<types::product_type>(typeof_tuple);

					auto& field_name = std::get<extended_ast::identifier>(param_tuple.children.at(1));

					auto field = std::find_if(product_type.product.begin(), product_type.product.end(), [&](auto& x) { return x.first == field_name.name.at(0); });

					call.type = field->second;
				}
				else if (call.id.name.size() == 1 && call.id.name.at(0) == "set")
				{
					call.type = types::void_type();
				}
				else
				{
					auto function_or_type = env.typeof(call.id.name);
					if (std::holds_alternative<types::function_type>(function_or_type))
					{
						auto function_type = std::get<types::function_type>(function_or_type);

						// Check function signature against call signature
						if (!(argument_type == *function_type.from))
						{
							return typecheck_error{
								"Function call signature does not match function signature:\n"
								+ std::visit(types::to_string, argument_type) + "\n"
								+ std::visit(types::to_string, *function_type.from)
							};
						}

						call.type = std::move(*function_type.to);
					}
					else if (std::holds_alternative<types::product_type>(function_or_type))
					{
						auto product_type = std::get<types::product_type>(function_or_type);

						// Check type signature against call signature
						if (!(argument_type == types::type(product_type)))
						{
							return typecheck_error{
								"Function call signature does not match function signature:\n"
								+ std::visit(types::to_string, argument_type) + "\n"
								+ std::visit(types::to_string, types::type(product_type))
							};
						}

						call.type = std::move(product_type);
					}
					else
					{
						return typecheck_error{
							"Function call can only call constructor or function"
						};
					}
				}

				return std::make_tuple(
					std::move(call),
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

				env.set_type(type_declaration.id.name.at(0), type_declaration.types.type);

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
			else if (std::holds_alternative<extended_ast::function>(n))
			{
				auto& func = std::get<extended_ast::function>(n);

				// Check 'from' type expression
				types::type from_type = interpret(func.from, env);

				// Check 'to' type expression
				types::type to_type = interpret(*func.to, env);

				func.type = types::function_type(types::make_unique(from_type), types::make_unique(to_type));

				// Allow recursion
				if (func.name.has_value())
				{
					env.set_type(func.name.value().name.at(0), func.type);
				}

				auto res = typecheck(std::move(*func.body), std::move(env));

				if (std::holds_alternative<typecheck_error>(res))
					return std::get<typecheck_error>(res);

				auto& checked_body = std::get<std::tuple<extended_ast::node, typecheck_environment>>(res);
				func.body = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_body)));
				env = std::move(std::get<typecheck_environment>(checked_body));

				if (!(std::visit(extended_ast::get_type, *func.body) == to_type))
				{
					return typecheck_error{ "Given return type is not the same as the type of the body" };
				}

				return std::make_tuple(
					std::move(func),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::conditional_branch>(n))
			{
				auto& branch = std::get<extended_ast::conditional_branch>(n);
				types::type common_type = types::unset_type();

				for (uint32_t branch_count = 0; branch_count < branch.branches.size(); branch_count++)
				{
					auto typechecked_path_or_error = typecheck(std::move(branch.branches.at(branch_count)), std::move(env));
					if (std::holds_alternative<typecheck_error>(typechecked_path_or_error))
						return std::get<typecheck_error>(typechecked_path_or_error);
					auto typechecked_path = std::move(std::get<std::tuple<extended_ast::node, typecheck_environment>>(typechecked_path_or_error));
					env = std::move(std::get<typecheck_environment>(typechecked_path));
					branch.branches.at(branch_count) = std::move(std::get<extended_ast::conditional_branch_path>(std::get<extended_ast::node>(typechecked_path)));

					if (common_type == types::type(types::unset_type()))
						common_type = branch.branches.at(branch_count).type;

					if (!(common_type == branch.branches.at(branch_count).type))
						return typecheck_error{ std::string("Branch is of a different type than those before it") };
				}

				branch.type = common_type;

				return std::make_tuple(
					std::move(n),
					std::move(env)
				);
			}
			else if (std::holds_alternative<extended_ast::conditional_branch_path>(n))
			{
				auto& branch_path = std::get<extended_ast::conditional_branch_path>(n);

				// Typecheck the test path
				auto checked_test_path_or_error = typecheck(std::move(*branch_path.test_path), std::move(env));
				if (std::holds_alternative<typecheck_error>(checked_test_path_or_error))
					return std::get<typecheck_error>(checked_test_path_or_error);
				auto checked_test_path = std::move(std::get<std::tuple<extended_ast::node, typecheck_environment>>(checked_test_path_or_error));
				branch_path.test_path = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_test_path)));
				env = std::move(std::get<typecheck_environment>(checked_test_path));

				// Typecheck the code path
				auto checked_code_path_or_error = typecheck(std::move(*branch_path.code_path), std::move(env));
				if (std::holds_alternative<typecheck_error>(checked_code_path_or_error))
					return std::get<typecheck_error>(checked_code_path_or_error);
				auto checked_code_path = std::move(std::get<std::tuple<extended_ast::node, typecheck_environment>>(checked_code_path_or_error));
				branch_path.code_path = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_code_path)));
				env = std::move(std::get<typecheck_environment>(checked_code_path));

				// Check the validity of the type of the test path
				auto test_type = std::visit(extended_ast::get_type, *branch_path.test_path);
				if (!(test_type == types::type(types::boolean_type())))
					return typecheck_error{ std::string("Branch number does not have a boolean test") };

				branch_path.type = std::visit(extended_ast::get_type, *branch_path.code_path);

				return std::make_tuple(
					std::move(n),
					std::move(env)
				);
			}

			return typecheck_error{ std::string("Unknown node type: ").append(std::to_string(n.index())) };
		}

		types::type interpret(const extended_ast::node& node, typecheck_environment& env)
		{
			if (std::holds_alternative<extended_ast::atom_type>(node))
			{
				return interpret(std::get<extended_ast::atom_type>(node), env);
			}
			else if (std::holds_alternative<extended_ast::atom_declaration>(node))
			{
				return interpret(std::get<extended_ast::atom_declaration>(node), env);
			}
			else if (std::holds_alternative<extended_ast::tuple_type>(node))
			{
				return interpret(std::get<extended_ast::tuple_type>(node), env);
			}
			else if (std::holds_alternative<extended_ast::tuple_declaration>(node))
			{
				return interpret(std::get<extended_ast::tuple_declaration>(node), env);
			}
			else if (std::holds_alternative<extended_ast::function_type>(node))
			{
				return interpret(std::get<extended_ast::function_type>(node), env);
			}
			else if (std::holds_alternative<extended_ast::function_declaration>(node))
			{
				return interpret(std::get<extended_ast::function_declaration>(node), env);
			}
			else
			{
				throw std::exception("Cannot interpret non-type node");
			}
		}

		types::type interpret(const extended_ast::atom_type& identifier, const typecheck_environment& env)
		{
			return env.typeof(identifier.name.name);
		}

		types::type interpret(const extended_ast::tuple_type& tuple, const typecheck_environment& env)
		{
			types::product_type result;

			for (const auto& child : tuple.elements)
			{
				if (std::holds_alternative<extended_ast::atom_type>(child))
				{
					result.product.push_back({ "", interpret(std::get<extended_ast::atom_type>(child), env) });
				}
				else if (std::holds_alternative<extended_ast::function_type>(child))
				{
					result.product.push_back({ "", interpret(std::get<extended_ast::function_type>(child), env) });
				}
			}
			return result;
		}

		types::type interpret(const extended_ast::function_type& function, const typecheck_environment& env)
		{
			types::product_type from = std::get<types::product_type>(interpret(std::move(function.from), env));
			types::product_type to = std::get<types::product_type>(interpret(std::move(function.to), env));

			return types::function_type(types::make_unique(from), types::make_unique(to));
		}

		types::type interpret(const extended_ast::atom_declaration& atom, typecheck_environment& env)
		{
			auto res = interpret(atom.type_name, env);
			env.set_type(atom.name.name.at(0), res);
			return res;
		}

		types::type interpret(const extended_ast::function_declaration& function, typecheck_environment& env)
		{
			auto res = interpret(function.type_name, env);
			env.set_type(function.name.name.at(0), res);
			return res;
		}

		types::type interpret(const extended_ast::tuple_declaration& tuple, typecheck_environment& env)
		{
			types::product_type res;
			for (decltype(auto) elem : tuple.elements)
			{
				
				if (std::holds_alternative<extended_ast::atom_declaration>(elem))
				{
					auto& atom = std::get<extended_ast::atom_declaration>(elem);
					auto atom_type = interpret(atom, env);
					res.product.push_back({ atom.name.name.at(0), atom_type });
				}
				else if (std::holds_alternative<extended_ast::function_declaration>(elem))
				{
					auto& func = std::get<extended_ast::function_declaration>(elem);
					auto func_type = interpret(func, env);
					res.product.push_back({ func.name.name.at(0), func_type });
				}
			}
			return res;
		}
	};
}