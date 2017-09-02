#pragma once
#include "lexer.h"

#include <memory>
#include <tuple>

namespace language
{
	template<typename TokenType, typename ErrorType>
	struct lexing_stage
	{
		virtual std::variant<std::vector<TokenType>, ErrorType> lex(const std::string&) = 0;
	};

	template<typename TokenType, typename TerminalType, typename ErrorType>
	struct lexer_to_parser_stage
	{
		virtual std::variant<std::vector<TerminalType>, ErrorType> convert(const std::vector<TokenType>& tokens) = 0;
	};

	template<typename TerminalType, typename CSTType, typename ErrorType>
	struct parsing_stage
	{
		virtual std::variant<CSTType, ErrorType> parse(const std::vector<TerminalType>& terminals) = 0;
	};

	template<typename CoreSyntaxTreeType, typename ExtendedAstType, typename ErrorType>
	struct cst_to_ast_stage 
	{
		virtual std::variant<ExtendedAstType, ErrorType> convert(CoreSyntaxTreeType cst) = 0;
	};

	template<typename ExtendedAstType, typename TypedAstType, typename EnvironmentType, typename ErrorType>
	struct typechecking_stage
	{
		virtual std::variant<std::tuple<TypedAstType, EnvironmentType>, ErrorType> typecheck(ExtendedAstType extended_ast, EnvironmentType env) = 0;
	};

	template<typename TypedAstType, typename CoreAstType, typename ErrorType>
	struct lowering_stage
	{
		virtual std::variant<CoreAstType, ErrorType> lower(TypedAstType extended_ast) = 0;
	};

	template<typename CoreAstType, typename ValueType, typename EnvironmentType, typename ErrorType>
	struct interpreting_stage
	{
		virtual std::variant<std::tuple<CoreAstType, ValueType, EnvironmentType>, ErrorType> interpret(CoreAstType extended_ast, EnvironmentType&& environment) = 0;
	};

	template<
		typename TokenType, typename LexError, 
		typename TerminalType, typename LexToParseError,
		typename CSTType, typename ParseError, 
		typename ExtendedAstType, typename CSTToASTError,
		typename TypedAstType, typename TypecheckError,
		typename CoreAstType, typename LowerError,
		typename ValueType, typename InterpError, 
		typename EnvironmentType>
	class pipeline
	{
	public:
		std::variant<
			std::variant<LexError, LexToParseError, ParseError, CSTToASTError, TypecheckError, LowerError, InterpError>,
			std::tuple<CoreAstType, ValueType, EnvironmentType>
		> run_to_interp(std::string&& file, EnvironmentType&& environment) const
		{
			// Lex
			auto lex_output = lexing_stage->lex(std::move(file));

			if (std::holds_alternative<LexError>(lex_output))
				return std::get<LexError>(lex_output);
			std::vector<TokenType>& tokens = std::get<std::vector<TokenType>>(lex_output);
			
			// Lex to Parse
			auto lex_to_parse_output = lexer_to_parser_stage->convert(tokens);

			if (std::holds_alternative<LexToParseError>(lex_to_parse_output))
				return std::get<LexToParseError>(lex_to_parse_output);
			std::vector<TerminalType>& converted_tokens = std::get<std::vector<TerminalType>>(lex_to_parse_output);

			// Parse
			auto parse_output = std::move(parsing_stage->parse(converted_tokens));

			if (std::holds_alternative<ParseError>(parse_output))
				return std::get<ParseError>(parse_output);
			CSTType& cst = std::get<CSTType>(parse_output);

			// CST to AST
			auto cst_to_ast_output = std::move(cst_to_ast_stage->convert(std::move(cst)));

			if (std::holds_alternative<CSTToASTError>(cst_to_ast_output))
				return std::get<CSTToASTError>(cst_to_ast_output);
			ExtendedAstType& extended_ast = std::get<ExtendedAstType>(cst_to_ast_output);

			// Typecheck
			auto typecheck_output = std::move(typechecking_stage->typecheck(std::move(extended_ast), environment));

			if (std::holds_alternative<TypecheckError>(typecheck_output))
				return std::get<TypecheckError>(typecheck_output);
			std::tuple<TypedAstType, EnvironmentType>& typecheck_results = std::get<std::tuple<TypedAstType, EnvironmentType>>(typecheck_output);

			environment = std::move(std::get<EnvironmentType>(typecheck_results));
			TypedAstType& typed_ast = std::get<TypedAstType>(typecheck_results);

			// Lower
			auto lower_output = std::move(lowering_stage->lower(std::move(typed_ast)));

			if (std::holds_alternative<LowerError>(lower_output))
				return std::get<LowerError>(lower_output);
			CoreAstType& core_ast = std::get<CoreAstType>(lower_output);

			// Interpret
			auto interp_output = interpreting_stage->interpret(std::move(core_ast), std::move(environment));

			if (std::holds_alternative<InterpError>(interp_output))
				return std::get<InterpError>(interp_output);
			return std::get<std::tuple<CoreAstType, ValueType, EnvironmentType>>(interp_output);
		}

		pipeline& lexer(lexing_stage<TokenType, LexError>* lex)
		{
			lexing_stage = lex;
			return *this;
		}

		pipeline& lexer_to_parser(lexer_to_parser_stage<TokenType, TerminalType, LexToParseError>* lex_to_parse)
		{
			lexer_to_parser_stage = lex_to_parse;
			return *this;
		}
		
		pipeline& parser(parsing_stage<TerminalType, CSTType, ParseError>* parse)
		{
			parsing_stage = std::move(parse);
			return *this;
		}

		pipeline& cst_to_ast(cst_to_ast_stage<CSTType, ExtendedAstType, CSTToASTError>* parse_to_lower)
		{
			cst_to_ast_stage = parse_to_lower;
			return *this;
		}

		pipeline& typechecker(typechecking_stage<ExtendedAstType, TypedAstType, EnvironmentType, TypecheckError>* typecheck) 
		{
			typechecking_stage = typecheck;
			return *this;
		}

		pipeline& lowerer(lowering_stage<ExtendedAstType, CoreAstType, LowerError>* lower)
		{
			lowering_stage = lower;
			return *this;
		}

		pipeline& interpreter(interpreting_stage<CoreAstType, ValueType, EnvironmentType, InterpError>* interpreter)
		{
			interpreting_stage = interpreter;
			return *this;
		}

	private:
		lexing_stage<TokenType, LexError>* lexing_stage;
		lexer_to_parser_stage<TokenType, TerminalType, LexToParseError>* lexer_to_parser_stage;
		parsing_stage<TerminalType, CSTType, ParseError>* parsing_stage;
		cst_to_ast_stage<CSTType, ExtendedAstType, CSTToASTError>* cst_to_ast_stage;
		typechecking_stage<ExtendedAstType, TypedAstType, EnvironmentType, TypecheckError>* typechecking_stage;
		lowering_stage<TypedAstType, CoreAstType, LowerError>* lowering_stage;
		interpreting_stage<CoreAstType, ValueType, EnvironmentType, InterpError>* interpreting_stage;
	};
}