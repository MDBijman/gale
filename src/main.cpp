#include "pipeline.h"
#include "ebnfe_parser.h"
#include "reader.h"

#include "lexer_stage.h"
#include "lexer_to_parser_stage.h"
#include "parser_stage.h"
#include "cst_to_ast_stage.h"
#include "typechecker_stage.h"
#include "lowering_stage.h"
#include "interpreting_stage.h"

#include "values.h"

#include "language_module.h"
#include "std_input.h"
#include "std_types.h"
#include "core_operations.h"
#include "tests.h"
#include <iostream>

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
		.parser(std::move(parsing_stage))
		.cst_to_ast(parser_to_lowerer_stage)
		.typechecker(typechecker_stage)
		.lowerer(lowering_stage)
		.interpreter(interpreting_stage);

	return p;
}

void process_error(std::variant<tools::lexing::error, fe::lex_to_parse_error, tools::ebnfe::error, fe::cst_to_ast_error, fe::typecheck_error, fe::lower_error, fe::interp_error> error)
{
	if (std::holds_alternative<tools::lexing::error>(error))
	{
		std::cout << "Lexing error:\n\t" << std::get<tools::lexing::error>(error).message;
	}
	else if (std::holds_alternative<fe::lex_to_parse_error>(error))
	{
		std::cout << "Lex to parse error:\n\t" << std::get<fe::lex_to_parse_error>(error).message;
	}
	else if (std::holds_alternative<tools::ebnfe::error>(error))
	{
		std::cout << "Parse error:\n\t" << std::get<tools::ebnfe::error>(error).message;
	}
	else if (std::holds_alternative<fe::cst_to_ast_error>(error))
	{
		std::cout << "CST to AST error:\n\t" << std::get<fe::cst_to_ast_error>(error).message;
	}
	else if (std::holds_alternative<fe::typecheck_error>(error))
	{
		std::cout << "Typecheck error:\n\t" << std::get<fe::typecheck_error>(error).message;
	}
	else if (std::holds_alternative<fe::lower_error>(error))
	{
		std::cout << "Lower error:\n\t" << std::get<fe::lower_error>(error).message;
	}
	else if (std::holds_alternative<fe::interp_error>(error))
	{
		std::cout << "Interp error:\n\t" << std::get<fe::interp_error>(error).message;
	}
	std::cout << "\n";
}

int main(int argc, char** argv)
{
	if (argc == 2)
	{
		std::string argument = argv[1];

		if (argument == "test")
		{
			tests::run_all();
		}
		else
		{
			std::cout << "Unknown commandline argument(s)" << std::endl;
		}

		std::cin.get();
		return 0;
	}

	auto pipeline = create_pipeline();

	// Load modules
	auto environment = fe::environment{};

	environment.extend(fe::core::operations::load());
	environment.add_module("language", fe::language_module::load(pipeline));

	auto stdlib = fe::environment{};
	stdlib.extend(fe::stdlib::input::load());
	stdlib.extend(fe::stdlib::types::load());
	environment.add_module("std", stdlib);


	while (1) {
		std::cout << ">>> ";
		std::string code;
		std::getline(std::cin, code);

		// Debug

		if (code == "1")
			code = "load snippets/testing.fe";

		// End Debug

		if (code == "")
			continue;

		if (code == "env")
		{
			std::cout << environment.to_string() << std::endl;
			continue;
		}

		if (code == "exit")
		{
			exit(0);
		}

		if (code.size() > 4 && code.substr(0, 4) == "load")
		{
			auto filename = code.substr(5, code.size() - 4);
			auto file_or_error = tools::files::read_file(filename);
			if (std::holds_alternative<std::exception>(file_or_error))
			{
				std::cout << "File not found\n";
				continue;
			}
	
			code = std::get<std::string>(file_or_error);
		}

		auto result_or_error = pipeline.run_to_interp(std::move(code), fe::environment(environment));

		if (std::holds_alternative<std::variant<tools::lexing::error, fe::lex_to_parse_error, tools::ebnfe::error, fe::cst_to_ast_error, fe::typecheck_error, fe::lower_error, fe::interp_error>>(result_or_error))
		{
			process_error(std::get<std::variant<tools::lexing::error, fe::lex_to_parse_error, tools::ebnfe::error, fe::cst_to_ast_error, fe::typecheck_error, fe::lower_error, fe::interp_error>>(result_or_error));
		}
		else
		{
			auto result = std::move(std::get<std::tuple<fe::core_ast::node, fe::values::value, fe::environment>>(result_or_error));

			std::cout << std::visit(fe::values::to_string, std::get<fe::values::value>(result)) << std::endl;

			auto& result_environment = std::get<fe::environment>(result);
			if (result_environment.get_module_name() != "")
			{
				std::string module_name = result_environment.get_module_name();
				environment.add_module(std::move(module_name), std::move(result_environment));
			}
			else
			{
				environment.extend(std::move(result_environment));
			}
		}
	}
}