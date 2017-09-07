#pragma once
#include "lexer.h"

#include <memory>
#include <tuple>

namespace language
{
	template<typename Token, typename ErrorType>
	struct lexing_stage
	{
		virtual std::variant<std::vector<Token>, ErrorType> lex(const std::string&) = 0;
	};

	template<typename Token, typename Terminal, typename ErrorType>
	struct lexer_to_parser_stage
	{
		virtual std::variant<std::vector<Terminal>, ErrorType> convert(const std::vector<Token>& tokens) = 0;
	};

	template<typename Terminal, typename CST, typename ErrorType>
	struct parsing_stage
	{
		virtual std::variant<CST, ErrorType> parse(const std::vector<Terminal>& terminals) = 0;
	};

	template<typename CoreSyntaxTreeType, typename ExtendedAst, typename ErrorType>
	struct cst_to_ast_stage 
	{
		virtual std::variant<ExtendedAst, ErrorType> convert(CoreSyntaxTreeType cst) = 0;
	};

	template<typename ExtendedAst, typename TypedAst, typename TypecheckEnvironment, typename ErrorType>
	struct typechecking_stage
	{
		virtual std::variant<std::tuple<TypedAst, TypecheckEnvironment>, ErrorType> typecheck(ExtendedAst extended_ast, TypecheckEnvironment env) = 0;
	};

	template<typename TypedAst, typename CoreAst, typename ErrorType>
	struct lowering_stage
	{
		virtual std::variant<CoreAst, ErrorType> lower(TypedAst extended_ast) = 0;
	};

	template<typename CoreAst, typename Value, typename RuntimeEnvironment, typename ErrorType>
	struct interpreting_stage
	{
		virtual std::variant<std::tuple<CoreAst, Value, RuntimeEnvironment>, ErrorType> interpret(CoreAst extended_ast, RuntimeEnvironment&& environment) = 0;
	};

	template<
		typename Token, typename LexError, 
		typename Terminal, typename LexToParseError,
		typename CST, typename ParseError, 
		typename ExtendedAst, typename CSTToASTError,
		typename TypedAst, typename TypecheckError,
		typename CoreAst, typename LowerError,
		typename Value, typename InterpError, 
		typename TypecheckEnvironment, typename RuntimeEnvironment>
	class pipeline
	{
	public:
		std::variant<
			std::variant<LexError, LexToParseError, ParseError, CSTToASTError, TypecheckError, LowerError, InterpError>,
			std::tuple<Value, TypecheckEnvironment, RuntimeEnvironment>
		> process(std::string&& code, TypecheckEnvironment&& typecheck_environment, RuntimeEnvironment&& runtime_environment) const
		{
			// Lex
			auto lex_output = lexing_stage->lex(std::move(code));

			if (std::holds_alternative<LexError>(lex_output))
				return std::get<LexError>(lex_output);
			std::vector<Token>& tokens = std::get<std::vector<Token>>(lex_output);

			// Lex to Parse
			auto lex_to_parse_output = lexer_to_parser_stage->convert(tokens);

			if (std::holds_alternative<LexToParseError>(lex_to_parse_output))
				return std::get<LexToParseError>(lex_to_parse_output);
			std::vector<Terminal>& converted_tokens = std::get<std::vector<Terminal>>(lex_to_parse_output);

			// Parse
			auto parse_output = std::move(parsing_stage->parse(converted_tokens));

			if (std::holds_alternative<ParseError>(parse_output))
				return std::get<ParseError>(parse_output);
			CST& cst = std::get<CST>(parse_output);

			// CST to AST
			auto cst_to_ast_output = std::move(cst_to_ast_stage->convert(std::move(cst)));

			if (std::holds_alternative<CSTToASTError>(cst_to_ast_output))
				return std::get<CSTToASTError>(cst_to_ast_output);
			ExtendedAst& extended_ast = std::get<ExtendedAst>(cst_to_ast_output);

			// Typecheck
			auto typecheck_output = std::move(typechecking_stage->typecheck(std::move(extended_ast), typecheck_environment));

			if (std::holds_alternative<TypecheckError>(typecheck_output))
				return std::get<TypecheckError>(typecheck_output);
			std::tuple<TypedAst, TypecheckEnvironment>& typecheck_results = std::get<std::tuple<TypedAst, TypecheckEnvironment>>(typecheck_output);

			typecheck_environment = std::move(std::get<TypecheckEnvironment>(typecheck_results));
			TypedAst& typed_ast = std::get<TypedAst>(typecheck_results);

			// Lower
			auto lower_output = std::move(lowering_stage->lower(std::move(typed_ast)));
			if (std::holds_alternative<LowerError>(lower_output))
				return std::get<LowerError>(lower_output);
			CoreAst& core_ast = std::get<CoreAst>(lower_output);

			// Interpret
			auto interp_output = interpreting_stage->interpret(std::move(core_ast), std::move(runtime_environment));
			if (std::holds_alternative<InterpError>(interp_output))
				return std::get<InterpError>(interp_output);
			auto interp_result = std::move(std::get<std::tuple<CoreAst, Value, RuntimeEnvironment>>(interp_output));

			return std::make_tuple<Value, TypecheckEnvironment, RuntimeEnvironment>(
				std::move(std::get<Value>(interp_result)),
				std::move(typecheck_environment),
				std::move(std::get<RuntimeEnvironment>(interp_result))
			);
		}

		pipeline& lexer(lexing_stage<Token, LexError>* lex)
		{
			lexing_stage = lex;
			return *this;
		}

		pipeline& lexer_to_parser(lexer_to_parser_stage<Token, Terminal, LexToParseError>* lex_to_parse)
		{
			lexer_to_parser_stage = lex_to_parse;
			return *this;
		}

		pipeline& parser(parsing_stage<Terminal, CST, ParseError>* parse)
		{
			parsing_stage = std::move(parse);
			return *this;
		}

		pipeline& cst_to_ast(cst_to_ast_stage<CST, ExtendedAst, CSTToASTError>* parse_to_lower)
		{
			cst_to_ast_stage = parse_to_lower;
			return *this;
		}

		pipeline& typechecker(typechecking_stage<ExtendedAst, TypedAst, TypecheckEnvironment, TypecheckError>* typecheck)
		{
			typechecking_stage = typecheck;
			return *this;
		}

		pipeline& lowerer(lowering_stage<ExtendedAst, CoreAst, LowerError>* lower)
		{
			lowering_stage = lower;
			return *this;
		}

		pipeline& interpreter(interpreting_stage<CoreAst, Value, RuntimeEnvironment, InterpError>* interpreter)
		{
			interpreting_stage = interpreter;
			return *this;
		}

	private:
		lexing_stage<Token, LexError>* lexing_stage;
		lexer_to_parser_stage<Token, Terminal, LexToParseError>* lexer_to_parser_stage;
		parsing_stage<Terminal, CST, ParseError>* parsing_stage;
		cst_to_ast_stage<CST, ExtendedAst, CSTToASTError>* cst_to_ast_stage;
		typechecking_stage<ExtendedAst, TypedAst, TypecheckEnvironment, TypecheckError>* typechecking_stage;
		lowering_stage<TypedAst, CoreAst, LowerError>* lowering_stage;
		interpreting_stage<CoreAst, Value, RuntimeEnvironment, InterpError>* interpreting_stage;
	};
}