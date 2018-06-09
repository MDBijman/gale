#pragma once
#include "fe/pipeline/lexer_stage.h"
#include "fe/pipeline/parser_stage.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/typechecker_stage.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/pipeline/interpreting_stage.h"
#include "fe/pipeline/error.h"

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
			parser(parsing_stage{})
		{}

		std::vector<utils::lexing::token> lex(const std::string& code) const
		{
			auto lex_output = lexer.lex(code);
			if (std::holds_alternative<utils::lexing::error>(lex_output))
				throw std::get<utils::lexing::error>(lex_output);
			return std::get<std::vector<utils::lexing::token>>(lex_output);
		}

		ext_ast::ast parse(std::vector<utils::lexing::token>&& tokens) 
		{
			auto parse_output = std::move(parser.parse(tokens));
			if (std::holds_alternative<fe::parse_error>(parse_output))
				throw std::get<fe::parse_error>(parse_output);
			return std::move(std::get<fe::ext_ast::ast>(parse_output));
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
	};
}