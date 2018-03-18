#pragma once
#include "fe/pipeline/lexer_stage.h"
#include "fe/pipeline/lexer_to_parser_stage.h"
#include "fe/pipeline/parser_stage.h"
#include "fe/pipeline/cst_to_ast_stage.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/pipeline/interpreting_stage.h"
#include "fe/pipeline/error.h"
#include "fe/data/scope_environment.h"
#include "fe/data/type_environment.h"
#include "fe/data/runtime_environment.h"
#include "utils/parsing/bnf_grammar.h"

#include <memory>
#include <tuple>
#include <variant>
#include <vector>

namespace fe
{
	class pipeline 
	{
	public:
		pipeline() :
			lexer(lexing_stage{}),
			parser(parsing_stage{}),
			lex_to_parse_converter(lexer_to_parser_stage{}),
			cst_to_ast_converter(cst_to_ast_stage{}),
			lowerer(lowering_stage{}),
			interpreter(interpreting_stage{})
		{}

		std::tuple<values::unique_value, type_environment, runtime_environment, scope_environment> process(std::string&& code, type_environment tenv, runtime_environment renv, scope_environment senv) 
		{
			auto lexed = lex(std::move(code));
			auto parsed = parse(std::move(lexed));
			auto scope_env = resolve(*parsed, std::move(senv));
			auto tchecked = typecheck(std::move(parsed), std::move(tenv));
			auto lowered = lower(std::move(tchecked.first));
			auto interped = interp(std::move(lowered), std::move(renv));
			return { std::move(interped.first), std::move(tchecked.second), std::move(interped.second), std::move(scope_env) };
		}

		std::vector<utils::bnf::terminal_node> lex(std::string&& code) const
		{
			auto lex_output = lexer.lex(std::move(code));
			if (std::holds_alternative<utils::lexing::error>(lex_output))
				throw std::get<utils::lexing::error>(lex_output);
			auto tokens = std::get<std::vector<utils::lexing::token>>(lex_output);

			auto lex_to_parse_output = lex_to_parse_converter.convert(tokens);
			if (std::holds_alternative<lex_to_parse_error>(lex_to_parse_output))
				throw std::get<lex_to_parse_error>(lex_to_parse_output);
			return std::get<std::vector<utils::bnf::terminal_node>>(lex_to_parse_output);
		}

		extended_ast::unique_node parse(std::vector<utils::bnf::terminal_node>&& tokens) 
		{
			auto parse_output = std::move(parser.parse(tokens));
			if (std::holds_alternative<utils::ebnfe::error>(parse_output))
				throw std::get<utils::ebnfe::error>(parse_output);
			auto& cst = std::get<std::unique_ptr<utils::ebnfe::node>>(parse_output);

			auto cst_to_ast_output = cst_to_ast_converter.convert(std::move(cst));
			if (std::holds_alternative<cst_to_ast_error>(cst_to_ast_output))
				throw std::get<cst_to_ast_error>(cst_to_ast_output);

			return std::move(std::get<extended_ast::unique_node>(cst_to_ast_output));
		}

		scope_environment resolve(extended_ast::node& ast, scope_environment&& env)
		{
			ast.resolve(std::move(env));
			return env;
		}

		std::pair<extended_ast::unique_node, type_environment> typecheck(extended_ast::unique_node extended_ast, type_environment&& tenv) const
		{
			extended_ast->typecheck(tenv);
			return std::make_pair(std::move(extended_ast), std::move(tenv));
		}

		core_ast::unique_node lower(extended_ast::unique_node tree) const
		{
			auto lower_output = std::move(lowerer.lower(std::move(tree)));
			if (std::holds_alternative<lower_error>(lower_output))
				throw std::get<lower_error>(lower_output);
			return std::move(std::get<core_ast::unique_node>(lower_output));
		}

		std::pair<values::unique_value, runtime_environment> interp(core_ast::unique_node tree, runtime_environment&& renv) const
		{
			auto interp_output = interpreter.interpret(std::move(tree), std::move(renv));
			if (std::holds_alternative<interp_error>(interp_output))
				throw std::get<interp_error>(interp_output);
			auto interp_result = std::move(std::get<std::tuple<core_ast::unique_node, values::unique_value, runtime_environment>>(interp_output));

			return {
				std::move(std::get<values::unique_value>(interp_result)),
				std::move(std::get<runtime_environment>(interp_result))
			};
		}

	private:
		lexing_stage lexer;
		parsing_stage parser;
		lexer_to_parser_stage lex_to_parse_converter;
		cst_to_ast_stage cst_to_ast_converter;
		lowering_stage lowerer;
		interpreting_stage interpreter;
	};
}