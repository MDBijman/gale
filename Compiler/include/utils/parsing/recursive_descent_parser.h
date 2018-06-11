#pragma once
#include "fe/data/ext_ast.h"
#include "utils/lexing/lexer.h"
#include "utils/memory/pipe.h"

namespace recursive_descent
{
	class token_stream_reader
	{
		memory::pipe<std::vector<lexing::token>>& in;
		std::vector<lexing::token> curr;
		void wait_for_pipe();

	public:
		token_stream_reader(memory::pipe<std::vector<lexing::token>>& in);

		const lexing::token& peek(int n = 0);

		lexing::token next();

		void consume(lexing::token_kind t);

		bool has_next();
	};

	using tree = fe::ext_ast::ast;
	using non_terminal = uint64_t;

	struct error
	{
		std::string message;
	};

	void generate();
	std::variant<tree, error> parse(token_stream_reader in);
}