#pragma once
#include <memory>
#include <lexer.h>
#include <tuple>

namespace language
{
	template<typename TokenType>
	struct lexing_stage
	{
		virtual std::vector<TokenType> lex(const std::string&) = 0;
	};

	template<typename TokenType, typename TerminalType>
	struct lexer_to_parser_stage
	{
		virtual std::vector<TerminalType> convert(const std::vector<TokenType>& tokens) = 0;
	};

	template<typename TerminalType, typename CSTType>
	struct parsing_stage
	{
		virtual CSTType parse(const std::vector<TerminalType>& terminals) = 0;
	};

	template<typename CoreSyntaxTreeType, typename ExtendedAstType>
	struct cst_to_ast_stage 
	{
		virtual ExtendedAstType convert(CoreSyntaxTreeType cst) = 0;
	};

	template<typename ExtendedAstType, typename TypedAstType, typename EnvironmentType>
	struct typechecking_stage
	{
		virtual std::tuple<TypedAstType, EnvironmentType> typecheck(ExtendedAstType extended_ast, EnvironmentType env) = 0;
	};

	template<typename TypedAstType, typename CoreAstType>
	struct lowering_stage
	{
		virtual CoreAstType lower(TypedAstType extended_ast) = 0;
	};

	template<typename CoreAstType, typename ValueType, typename EnvironmentType>
	struct interpreting_stage
	{
		virtual std::tuple<CoreAstType, ValueType, EnvironmentType> interpret(CoreAstType extended_ast, EnvironmentType&& environment) = 0;
	};

	template<typename TokenType, typename TerminalType, typename CSTType, typename ExtendedAstType, typename TypedAstType, typename CoreAstType, typename ValueType, typename EnvironmentType>
	class pipeline
	{
	public:
		std::tuple<CoreAstType, ValueType, EnvironmentType> run_to_interp(std::string&& file, EnvironmentType&& environment)
		{
			std::vector<TokenType> tokens = lexing_stage->lex(file);
			
			std::vector<TerminalType> converted_tokens = lexer_to_parser_stage->convert(tokens);
			
			CSTType cst = std::move(parsing_stage->parse(converted_tokens));
			
			ExtendedAstType extended_ast = std::move(cst_to_ast_stage->convert(std::move(cst)));
			
			TypedAstType typed_ast;
			std::tie(typed_ast, environment) = std::move(typechecking_stage->typecheck(std::move(extended_ast), environment));

			CoreAstType lowered_ast = std::move(lowering_stage->lower(std::move(typed_ast)));
	
			return interpreting_stage->interpret(std::move(lowered_ast), std::move(environment));
		}

		pipeline& lexer(lexing_stage<TokenType>* lex)
		{
			lexing_stage = lex;
			return *this;
		}

		pipeline& lexer_to_parser(lexer_to_parser_stage<TokenType, TerminalType>* lex_to_parse)
		{
			lexer_to_parser_stage = lex_to_parse;
			return *this;
		}
		
		pipeline& parser(parsing_stage<TerminalType, CSTType>* parse)
		{
			parsing_stage = std::move(parse);
			return *this;
		}

		pipeline& cst_to_ast(cst_to_ast_stage<CSTType, ExtendedAstType>* parse_to_lower)
		{
			cst_to_ast_stage = parse_to_lower;
			return *this;
		}

		pipeline& typechecker(typechecking_stage<ExtendedAstType, TypedAstType, EnvironmentType>* typecheck) 
		{
			typechecking_stage = typecheck;
			return *this;
		}

		pipeline& lowerer(lowering_stage<ExtendedAstType, CoreAstType>* lower)
		{
			lowering_stage = lower;
			return *this;
		}

		pipeline& interpreter(interpreting_stage<CoreAstType, ValueType, EnvironmentType>* interpreter)
		{
			interpreting_stage = interpreter;
			return *this;
		}

	private:
		lexing_stage<TokenType>* lexing_stage;
		lexer_to_parser_stage<TokenType, TerminalType>* lexer_to_parser_stage;
		parsing_stage<TerminalType, CSTType>* parsing_stage;
		cst_to_ast_stage<CSTType, ExtendedAstType>* cst_to_ast_stage;
		typechecking_stage<ExtendedAstType, TypedAstType, EnvironmentType>* typechecking_stage;
		lowering_stage<TypedAstType, CoreAstType>* lowering_stage;
		interpreting_stage<CoreAstType, ValueType, EnvironmentType>* interpreting_stage;
	};
}