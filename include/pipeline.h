#pragma once
#include <memory>
#include <lexer.h>

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

	template<typename ExtendedAstType, typename CoreAstType>
	struct lowering_stage
	{
		virtual CoreAstType lower(ExtendedAstType ast) = 0;
	};

	template<typename CoreAstType, typename ValueType>
	struct interpreting_stage
	{
		virtual ValueType interpret(CoreAstType ast) = 0;
	};

	template<typename TokenType, typename TerminalType, typename CSTType, typename ExtendedAstType, typename CoreAstType, typename ValueType>
	class pipeline
	{
	public:
		ValueType process(const std::string& file)
		{
			std::vector<TokenType> tokens = lexing_stage->lex(file);
			std::vector<TerminalType> converted_tokens = lexer_to_parser_stage->convert(tokens);
			CSTType cst = std::move(parsing_stage->parse(converted_tokens));
			ExtendedAstType ast = std::move(cst_to_ast_stage->convert(std::move(cst)));
			CoreAstType lowered_ast = std::move(lowering_stage->lower(std::move(ast)));
			return interpreting_stage->interpret(std::move(lowered_ast));
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

		pipeline& parser_to_lowerer(cst_to_ast_stage<CSTType, ExtendedAstType>* parse_to_lower)
		{
			cst_to_ast_stage = parse_to_lower;
			return *this;
		}

		pipeline& lowerer(lowering_stage<ExtendedAstType, CoreAstType>* lower)
		{
			lowering_stage = lower;
			return *this;
		}

		pipeline& interpreter(interpreting_stage<CoreAstType, ValueType>* interpreter)
		{
			interpreting_stage = interpreter;
			return *this;
		}

	private:
		lexing_stage<TokenType>* lexing_stage;
		lexer_to_parser_stage<TokenType, TerminalType>* lexer_to_parser_stage;
		parsing_stage<TerminalType, CSTType>* parsing_stage;
		cst_to_ast_stage<CSTType, ExtendedAstType>* cst_to_ast_stage;
		lowering_stage<ExtendedAstType, CoreAstType>* lowering_stage;
		interpreting_stage<CoreAstType, ValueType>* interpreting_stage;
	};
}