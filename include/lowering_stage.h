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
			auto helper = [this](auto&& x) {
				return lower(std::move(x));
			};

			return std::visit(helper, std::move(n));
		}

		std::variant<core_ast::node, lower_error> lower(extended_ast::tuple&& tuple)
		{
			std::vector<core_ast::node> children;
			for (decltype(auto) child : tuple.children)
			{
				auto lowered_child = lower(std::move(child));
				if (std::holds_alternative<lower_error>(lowered_child))
					return std::get<lower_error>(lowered_child);
				children.push_back(std::move(std::get<core_ast::node>(lowered_child)));
			}

			return core_ast::node(core_ast::tuple(move(children), tuple.type));
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::block&& block)
		{
			std::vector<core_ast::node> children;
			for (decltype(auto) child : block.children)
			{
				auto lowered_child = lower(std::move(child));
				if (std::holds_alternative<lower_error>(lowered_child))
					return std::get<lower_error>(lowered_child);
				children.push_back(std::move(std::get<core_ast::node>(lowered_child)));
			}

			return core_ast::block(move(children), block.type);
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::module_declaration&& n)
		{
			return core_ast::no_op();
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::identifier&& id)
		{
			return core_ast::node(core_ast::identifier(std::move(id.name), id.type));
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::assignment&& assignment)
		{
			auto res = lower(move(*assignment.value));
			if (std::holds_alternative<lower_error>(res))
				return std::get<lower_error>(res);
			auto& lowered_value = std::get<core_ast::node>(res);

			return core_ast::assignment(
				core_ast::identifier(move(assignment.id.name), assignment.id.type),
				std::make_unique<core_ast::node>(move(lowered_value))
			);
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::function_call&& fc)
		{
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
		std::variant<core_ast::node, lower_error> lower(extended_ast::type_declaration&& type_declaration)
		{
			auto func_param_type = std::get<types::product_type>(type_declaration.types.type);

			auto return_type = func_param_type;
			auto return_statement = core_ast::make_unique(
				core_ast::identifier(std::vector<std::string>{"_arg0"}, func_param_type)
			);

			auto parameter_name = core_ast::identifier("_arg0", func_param_type);


			return core_ast::assignment(
				core_ast::identifier(move(type_declaration.id.name), move(type_declaration.id.type)),

				core_ast::make_unique(core_ast::function(
					std::optional<core_ast::identifier>(),
					parameter_name,
					std::move(return_statement),
					types::function_type(
						types::make_unique(types::product_type(return_type)),
						types::make_unique(types::product_type(return_type))
					)
				))
			);
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::integer&& integer)
		{
			return core_ast::integer(integer.value);
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::string&& string)
		{
			return core_ast::string(string.value);
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::export_stmt&& n)
		{
			return core_ast::no_op();
		}
		std::variant<core_ast::node, lower_error> lower(extended_ast::function&& func)
		{
			auto name = std::optional<core_ast::identifier>();
			if (func.name.has_value())
			{
				auto name_or_error = lower(std::move(func.name.value()));
				if (std::holds_alternative<lower_error>(name_or_error))
					return std::get<lower_error>(name_or_error);
				else
					name = std::make_optional(std::move(std::get<core_ast::identifier>(std::get<core_ast::node>(name_or_error))));
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
		std::variant<core_ast::node, lower_error> lower(extended_ast::conditional_branch&& branch)
		{
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
		std::variant<core_ast::node, lower_error> lower(extended_ast::conditional_branch_path&& branch_path)
		{
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
	};
}