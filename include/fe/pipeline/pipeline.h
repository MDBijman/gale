#pragma once
#include "fe/pipeline/lexer_stage.h"
#include "fe/pipeline/lexer_to_parser_stage.h"
#include "fe/pipeline/parser_stage.h"
#include "fe/pipeline/cst_to_ast_stage.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/typechecker_stage.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/pipeline/interpreting_stage.h"
#include "fe/pipeline/error.h"

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
			cst_to_ast_converter(cst_to_ast_stage{})
		{}

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

		ext_ast::ast parse(std::vector<utils::bnf::terminal_node>&& tokens) 
		{
			auto parse_output = std::move(parser.parse(tokens));
			if (std::holds_alternative<utils::ebnfe::error>(parse_output))
				throw std::get<utils::ebnfe::error>(parse_output);
			auto& cst = std::get<utils::bnf::tree>(parse_output);

			auto cst_to_ast_output = cst_to_ast_converter.convert(std::move(cst));
			if (std::holds_alternative<cst_to_ast_error>(cst_to_ast_output))
				throw std::get<cst_to_ast_error>(cst_to_ast_output);

			return std::move(std::get<ext_ast::ast>(cst_to_ast_output));
		}

		void typecheck(ext_ast::ast& ast) const
		{
			auto root = ast.root_id();
			auto& root_node = ast.get_node(root);
			ext_ast::resolve(root_node, ast);
			ext_ast::typecheck(root_node, ast);
		}

		core_ast::ast lower(ext_ast::ast& ast) const
		{
			return ext_ast::lower(ast);
		}

		values::unique_value interp(core_ast::ast& n) const
		{
			auto res = core_ast::interpret(n);
			if (std::holds_alternative<interp_error>(res))
				throw std::get<interp_error>(res);

			return std::move(std::get<values::unique_value>(res));
		}

	private:
		lexing_stage lexer;
		parsing_stage parser;
		lexer_to_parser_stage lex_to_parse_converter;
		cst_to_ast_stage cst_to_ast_converter;
	};
}