#pragma once
#include "fe/pipeline/lexer_stage.h"
#include "fe/pipeline/parser_stage.h"
#include "fe/pipeline/resolution_stage.h"
#include "fe/pipeline/typechecker_stage.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/pipeline/error.h"

#include <memory>
#include <tuple>
#include <variant>
#include <vector>
#include <thread>

namespace fe
{
	class pipeline 
	{
	public:
		pipeline() :
			lexer(lexing_stage{}),
			parser(parsing_stage{})
		{}

		ext_ast::ast parse(const std::string& code) 
		{
			auto res = lexer.lex(code);
			auto parse_output = parser.parse(std::get<std::vector<lexing::token>>(res));

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

	private:
		lexing_stage lexer;
		parsing_stage parser;
	};
}