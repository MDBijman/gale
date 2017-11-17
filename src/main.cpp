#include "fe/pipeline/pipeline.h"
#include "fe/pipeline/lexer_stage.h"
#include "fe/pipeline/lexer_to_parser_stage.h"
#include "fe/pipeline/parser_stage.h"
#include "fe/pipeline/cst_to_ast_stage.h"
#include "fe/pipeline/typechecker_stage.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/pipeline/interpreting_stage.h"

#include "fe/modes/project.h"
#include "fe/modes/repl.h"
#include "tests/tests.h"

#include <iostream>
#include <filesystem>

fe::pipeline create_pipeline()
{
	fe::pipeline p;

	auto lexing_stage = new fe::lexing_stage{};
	auto parsing_stage = new fe::parsing_stage{};
	// LtP stage initialization has a dependency on the values of the language terminals
	// These are initialized in the parsing stage initialization
	auto lexer_to_parser_stage = new fe::lexer_to_parser_stage{};
	auto parser_to_lowerer_stage = new fe::cst_to_ast_stage{};
	auto typechecker_stage = new fe::typechecker_stage{};
	auto lowering_stage = new fe::lowering_stage{};
	auto interpreting_stage = new fe::interpreting_stage{};

	p
		.lexer(lexing_stage)
		.lexer_to_parser(lexer_to_parser_stage)
		.parser(parsing_stage)
		.cst_to_ast(parser_to_lowerer_stage)
		.typechecker(typechecker_stage)
		.lowerer(lowering_stage)
		.interpreter(interpreting_stage);

	return p;
}


int main(int argc, char** argv)
{
	auto mode = std::string(argv[1]);
	if (argc == 2 && (mode != "test") && (mode != "project"))
	{
		std::cout << "Unknown commandline argument(s)" << std::endl;
		std::cin.get();
		return -1;
	}

	auto pipeline = create_pipeline();

	try
	{
		if (mode == "repl" || argc == 1)
		{
			fe::repl(std::move(pipeline));
		}
		else if (mode == "test")
		{
			return tests::run();
		}
		else if (mode == "project")
		{
			if (argc == 2)
			{
				std::cout << "Missing project location" << std::endl;
				std::cin.get();
				return -1;
			}
			else if (argc == 3)
			{
				std::cout << "Missing main file name" << std::endl;
				std::cin.get();
				return -1;
			}

			fe::project proj(argv[2], argv[3], std::move(pipeline));
			auto[te, re] = proj.interp();
			std::cout << te.to_string() << std::endl;
			std::cout << re.to_string() << std::endl;
		}
	}
	catch (const tools::lexing::error& e)
	{
		std::cout << "Lexing error:\n" << e.message;
	}
	catch (const fe::lex_to_parse_error& e)
	{
		std::cout << "Lex to parse error:\n" << e.message;
	}
	catch (const tools::ebnfe::error& e)
	{
		std::cout << "Parse error:\n" << e.message;
	}
	catch (const fe::cst_to_ast_error& e)
	{
		std::cout << "CST to AST conversion error:\n" << e.message;
	}
	catch (const fe::typecheck_error& e)
	{
		std::cout << "Typechecking error:\n" << e.message;
	}
	catch (const fe::lower_error& e)
	{
		std::cout << "Lowering error:\n" << e.message;
	}
	catch (const fe::interp_error& e)
	{
		std::cout << "Interp error:\n" << e.message;
	}
	catch (const fe::type_env_error& e)
	{
		std::cout << e.message << std::endl;
	}
	catch (const std::runtime_error& e)
	{
		std::cout << e.what() << std::endl;
	}
	std::cin.get();
	return 0;
}