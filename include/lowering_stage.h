#pragma once
#include "ebnfe_parser.h"
#include "extended_ast.h"
#include "core_ast.h"
#include "values.h"
#include "error.h"

#include <memory>

namespace fe
{

	class lowering_stage : public language::lowering_stage<extended_ast::node, core_ast::node, lower_error>
	{
	public:
		std::variant<core_ast::node, lower_error> lower(extended_ast::node n) override
		{
			using namespace std;

			if (holds_alternative<extended_ast::value_tuple>(n))
			{
				auto tuple = move(get<extended_ast::value_tuple>(n));
				vector<core_ast::node> children;
				for (decltype(auto) child : tuple.children)
				{
					auto lowered_child = lower(std::move(child));
					if (std::holds_alternative<lower_error>(lowered_child))
						return std::get<lower_error>(lowered_child);
					children.push_back(std::move(std::get<core_ast::node>(lowered_child)));
				}

				return core_ast::node(core_ast::tuple(move(children), tuple.type));
			}
			else if (holds_alternative<extended_ast::identifier>(n))
			{
				auto& id = get<extended_ast::identifier>(n);

				return core_ast::node(core_ast::identifier(move(id.name), id.type));
			}
			else if (holds_alternative<extended_ast::assignment>(n))
			{
				auto assignment = move(get<extended_ast::assignment>(n));

				auto res = lower(move(*assignment.value));
				if (std::holds_alternative<lower_error>(res))
					return std::get<lower_error>(res);
				auto& lowered_value = std::get<core_ast::node>(res);

				return core_ast::assignment(
					core_ast::identifier(move(assignment.id.name), assignment.id.type),
					std::make_unique<core_ast::node>(move(lowered_value))
				);
			}
			else if (holds_alternative<extended_ast::function_call>(n))
			{
				auto& fc = get<extended_ast::function_call>(n);

				auto res = lower(move(*fc.params));
				if (std::holds_alternative<lower_error>(res))
					return std::get<lower_error>(res);
				auto& lowered_params = std::get<core_ast::node>(res);

				auto id_or_error = lower(std::move(fc.id));
				if (std::holds_alternative<lower_error>(id_or_error))
					return std::get<lower_error>(id_or_error);
				auto& id = std::get<core_ast::identifier>(std::get<core_ast::node>(id_or_error));

				return core_ast::function_call(
					std::move(id),
					move(core_ast::make_unique(lowered_params)),
					std::move(fc.type)
				);
			}
			else if (holds_alternative<extended_ast::type_declaration>(n))
			{
				auto type_declaration = move(get<extended_ast::type_declaration>(n));

				auto func_param_type = get<types::product_type>(type_declaration.types.type);

				auto return_type = func_param_type;
				auto return_statement = core_ast::make_unique(
					core_ast::identifier(std::vector<std::string>{"_from"}, func_param_type)
				);

				auto parameter_names = std::vector<core_ast::identifier>();
				parameter_names.push_back(core_ast::identifier("_from", func_param_type));


				return core_ast::assignment(
					core_ast::identifier(move(type_declaration.id.name), move(type_declaration.id.type)),

					core_ast::make_unique(core_ast::function(
						std::optional<core_ast::identifier>(),
						std::move(parameter_names),
						std::move(return_statement),
						types::function_type(
							types::make_unique(types::product_type(return_type)),
							types::make_unique(types::product_type(return_type))
						)
					))
				);
			}
			else if (holds_alternative<extended_ast::integer>(n))
			{
				return core_ast::integer(get<extended_ast::integer>(n).value);
			}
			else if (holds_alternative<extended_ast::string>(n))
			{
				return core_ast::string(get<extended_ast::string>(n).value);
			}
			else if (holds_alternative<extended_ast::export_stmt>(n))
			{
				return core_ast::integer(1);
			}
			else if (holds_alternative<extended_ast::function>(n))
			{
				auto& func = std::get<extended_ast::function>(n);

				auto name = std::optional<core_ast::identifier>();
				if (func.name.has_value())
				{
					auto name_or_error = lower(std::move(func.name.value()));
					if (std::holds_alternative<lower_error>(name_or_error))
						return std::get<lower_error>(name_or_error);
					else
						name = make_optional(move(get<core_ast::identifier>(get<core_ast::node>(name_or_error))));
				}

				auto body_or_error = lower(std::move(*func.body));
				if (std::holds_alternative<lower_error>(body_or_error))
					return std::get<lower_error>(body_or_error);

				std::vector<core_ast::identifier> parameter_names;
				for (decltype(auto) identifier : func.from.elements)
				{
					if (std::holds_alternative<extended_ast::atom_declaration>(identifier))
					{
						auto& atom = std::get<extended_ast::atom_declaration>(identifier);
						parameter_names.push_back(std::get<core_ast::identifier>(std::get<core_ast::node>(lower(std::move(atom.name)))));
					}
					if (std::holds_alternative<extended_ast::function_declaration>(identifier))
					{
						auto& function = std::get<extended_ast::function_declaration>(identifier);
						parameter_names.push_back(std::get<core_ast::identifier>(std::get<core_ast::node>(lower(std::move(function.name)))));
					}
				}

				return core_ast::function{
					std::move(name),
					std::move(parameter_names),
					core_ast::make_unique(std::move(std::get<core_ast::node>(body_or_error))),
					std::move(func.type)
				};
			}
			else if (holds_alternative<extended_ast::conditional_branch>(n))
			{
				auto& branch = std::get<extended_ast::conditional_branch>(n);
				
				std::vector<core_ast::conditional_branch_path> lowered_branches;

				for (decltype(auto) branch : branch.branches)
				{
					auto lowered_branch_or_error = lower(std::move(branch));
					if (std::holds_alternative<lower_error>(lowered_branch_or_error))
						return std::get<lower_error>(lowered_branch_or_error);
					auto& lowered_branch = std::get<core_ast::node>(lowered_branch_or_error);

					lowered_branches.push_back(std::move(std::get<core_ast::conditional_branch_path>(lowered_branch)));
				}

				return core_ast::conditional_branch{
					std::move(lowered_branches),
					std::move(branch.type)
				};
			}
			else if (holds_alternative<extended_ast::conditional_branch_path>(n))
			{
				auto& branch_path = std::get<extended_ast::conditional_branch_path>(n);

				auto lowered_test_or_error = lower(std::move(*branch_path.test_path));
				if (std::holds_alternative<lower_error>(lowered_test_or_error))
					return std::get<lower_error>(lowered_test_or_error);
				auto& lowered_test = std::get<core_ast::node>(lowered_test_or_error);

				auto lowered_code_or_error = lower(std::move(*branch_path.code_path));
				if (std::holds_alternative<lower_error>(lowered_code_or_error))
					return std::get<lower_error>(lowered_code_or_error);
				auto& lowered_code = std::get<core_ast::node>(lowered_code_or_error);

				return core_ast::conditional_branch_path{
					core_ast::make_unique(std::move(lowered_test)),
					core_ast::make_unique(std::move(lowered_code))
				};
			}

			return lower_error();
		}
	};
}