#include "lexer_generator.h"
#include "parser.h"
#include "ebnf_lexer.h"
#include "lexer.h"
#include <functional>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string_view>

namespace lexer_generator
{

	language_lexer generate(const std::string& specification_location)
	{
		// Parse rules
		std::ifstream in(specification_location, std::ios::in | std::ios::binary);
		if (!in) throw std::exception("Could not open file");

		std::string contents;
		in.seekg(0, std::ios::end);
		contents.resize(in.tellg());
		in.seekg(0, std::ios::beg);
		in.read(&contents[0], contents.size());
		in.close();
		std::string_view contents_view = contents;

		lexer::lexer lexer{
			lexer::lexer_rules{{
					"(", ")", "+", "*", "-", "\'", "::=", "|", 
			}}
		};

		auto res = lexer.parse("()+*");

		std::vector<ebnf::terminal> tokens;
		state_machine machine(new ebnf::state_decider(contents_view, contents_view.begin(), &tokens));
		while (!machine.is_finished())
		{
			machine.current_state()->run(machine);
		}


		ebnf::rules mapping;
		{
			mapping.insert(
			{
				ebnf::non_terminal::RULE,
				{
					{ebnf::terminal::IDENTIFIER,ebnf::terminal::ASSIGNMENT, ebnf::non_terminal::RHS_ALTERNATION,ebnf::terminal::END_OF_RULE }
				}
			});

			mapping.insert(
			{
				ebnf::non_terminal::RULESET,
				{
					{ ebnf::non_terminal::RULE, ebnf::non_terminal::RULESET },
					{ebnf::terminal::END_OF_INPUT }
				}
			});

			mapping.insert(
			{
				ebnf::non_terminal::TERMINAL,
				{
					{ebnf::terminal::STRING },
					{ebnf::terminal::IDENTIFIER }
				}
			});

			mapping.insert(
			{
				ebnf::non_terminal::PRIMARY,
				{
					{ ebnf::non_terminal::TERMINAL, ebnf::non_terminal::OPTIONAL_MULTIPLIER  },
					{ebnf::terminal::BEGIN_GROUP, ebnf::non_terminal::RHS_ALTERNATION,ebnf::terminal::END_GROUP  },
				}
			});

			mapping.insert({
				ebnf::non_terminal::RHS_EXCEPTION,
				{
					{ ebnf::non_terminal::PRIMARY, ebnf::non_terminal::OPTIONAL_EXCEPTION, },
				}
			});

			mapping.insert({
				ebnf::non_terminal::OPTIONAL_EXCEPTION,
				{
					{ebnf::terminal::EXCEPTION, ebnf::non_terminal::RHS_ALTERNATION },
					{ebnf::terminal::EPSILON }
				}
			});

			mapping.insert({
				ebnf::non_terminal::RHS_ALTERNATION,
				{
					{ ebnf::non_terminal::RHS_EXCEPTION, ebnf::non_terminal::OPTIONAL_ALTERNATION, },
				}
			});

			mapping.insert({
				ebnf::non_terminal::OPTIONAL_MULTIPLIER,
				{
					{ebnf::terminal::ZERO_OR_MORE },
					{ebnf::terminal::ONE_OR_MORE },
					{ebnf::terminal::EPSILON },
				}
			});

			mapping.insert({
				ebnf::non_terminal::OPTIONAL_ALTERNATION,
				{
					{ebnf::terminal::ALTERNATION_SIGN, ebnf::non_terminal::RHS_ALTERNATION },
					{ebnf::terminal::EPSILON }
				}
			});

			mapping.insert({
				ebnf::non_terminal::ZERO_OR_MORE_ALTERNATION,
				{
					{ebnf::terminal::ALTERNATION_SIGN, ebnf::non_terminal::PRIMARY, ebnf::non_terminal::ZERO_OR_MORE_ALTERNATION },
					{ebnf::terminal::EPSILON },
				}
			});
		}


		ebnf::parser parser(mapping);

		try
		{
			auto ast = parser.parse(tokens);

		
			std::function<void(int, ast::node<ebnf::symbol>*)> print = [&print](int indentation, ast::node<ebnf::symbol>* node) -> void {
				for (int i = 0; i < indentation; i++)
					std::cout << ' ';
				if (node->t.is_terminal())
					std::cout << (int)node->t.get_terminal() << '\n';
				else
					std::cout << (int)node->t.get_non_terminal() << '\n';
				for(auto child : node->children)
					print(indentation + 1, child);
				return;
			};
			print(0, ast);

			
			delete ast;
		}
		catch (std::runtime_error e)
		{
			std::cerr << e.what() << std::endl;
		}
	}
}