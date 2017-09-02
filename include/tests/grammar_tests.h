#pragma once
#include "ebnfe_parser.h"

#include <vector>
#include <assert.h>

namespace tests
{
	struct id_parsing_tests
	{
		tools::ebnfe::parser parser;
		tools::ebnfe::terminal id, plus, number;
		tools::ebnfe::non_terminal id_rule, disambiguation, arithmetic, expression;

		id_parsing_tests()
		{
			parser = tools::ebnfe::parser{};
			id_rule = parser.new_non_terminal();
			disambiguation = parser.new_non_terminal();

			// 3 possibilities
			arithmetic = parser.new_non_terminal();
			expression = parser.new_non_terminal();
			// Or epsilon

			id = parser.new_terminal();
			plus = parser.new_terminal();
			number = parser.new_terminal();

			using namespace tools::ebnf::meta;
			parser.new_rule({ id_rule, { id, disambiguation } });
			parser.new_rule({ disambiguation, {
					tools::ebnf::epsilon, alt,
					plus, expression, alt,
					expression
				}
			});
			parser.new_rule({ expression, { number, alt, id } });
		}

		void run_all()
		{
			test_id_as_arithmetic();
			test_id_as_function_call();
			test_id_as_expression();
		}

		void test_id_as_arithmetic()
		{
			auto output_or_error = parser.parse(id_rule, std::vector<tools::bnf::terminal_node>{
				{ id, "a" }, { plus, "+" }, { number, "5" }
			});

			using std::move;
			using namespace tools;

			auto output = move(std::get<std::unique_ptr<tools::ebnfe::node>>(output_or_error));

			// Id rule
			auto root = move(std::get<tools::ebnfe::non_terminal_node>(*output));
			assert(root.value == id_rule);

			// Id 
			{
				auto id_t = std::get<ebnfe::terminal_node>(move(*root.children.at(0).release()));
				assert(id_t.value == id);
				assert(id_t.token == "a");
			}

			// Disambiguation
			{
				auto disambiguation_nt = std::get<ebnfe::non_terminal_node>(move(*root.children.at(1).release()));
				assert(disambiguation_nt.value == disambiguation);

				// Plus 
				{
					auto plus_t = std::get<ebnfe::terminal_node>(move(*disambiguation_nt.children.at(0).release()));
					assert(plus_t.value == plus);
				}

				// Expression 
				{
					auto expression_nt = std::get<ebnfe::non_terminal_node>(move(*disambiguation_nt.children.at(1).release()));
					assert(expression_nt.value == expression);

					// Number
					{
						auto number_t = std::get<ebnfe::terminal_node>(move(*expression_nt.children.at(0).release()));
						assert(number_t.value == number);
						assert(number_t.token == "5");
					}

				}
			}
		}

		void test_id_as_function_call()
		{
			auto output_or_error = parser.parse(id_rule, std::vector<tools::bnf::terminal_node>{
				{ id, "a" }, { number, "5" }
			});

			using std::move;
			using namespace tools;

			auto output = move(std::get<std::unique_ptr<tools::ebnfe::node>>(output_or_error));

			// Id rule
			auto root = move(std::get<tools::ebnfe::non_terminal_node>(*output));
			assert(root.value == id_rule);

			// Id 
			{
				auto id_t = std::get<ebnfe::terminal_node>(move(*root.children.at(0).release()));
				assert(id_t.value == id);
				assert(id_t.token == "a");
			}

			// Disambiguation
			{
				auto disambiguation_nt = std::get<ebnfe::non_terminal_node>(move(*root.children.at(1).release()));
				assert(disambiguation_nt.value == disambiguation);

				// Expression 
				{
					auto expression_nt = std::get<ebnfe::non_terminal_node>(move(*disambiguation_nt.children.at(0).release()));
					assert(expression_nt.value == expression);

					// Number
					{
						auto number_t = std::get<ebnfe::terminal_node>(move(*expression_nt.children.at(0).release()));
						assert(number_t.value == number);
						assert(number_t.token == "5");
					}

				}
			}
		}

		void test_id_as_expression()
		{
			auto output_or_error = parser.parse(id_rule, std::vector<tools::bnf::terminal_node>{
				{ id, "a" }
			});

			using std::move;
			using namespace tools;

			auto output = move(std::get<std::unique_ptr<tools::ebnfe::node>>(output_or_error));

			// Id rule
			auto root = move(std::get<tools::ebnfe::non_terminal_node>(*output));
			assert(root.value == id_rule);

			// Id 
			{
				auto id_t = std::get<ebnfe::terminal_node>(move(*root.children.at(0).release()));
				assert(id_t.value == id);
				assert(id_t.token == "a");
			}

			// Disambiguation
			{
				auto disambiguation_nt = std::get<ebnfe::non_terminal_node>(move(*root.children.at(1).release()));
				assert(disambiguation_nt.value == disambiguation);

				// Epsilon 
				{
					auto epsilon_t = std::get<ebnfe::terminal_node>(move(*disambiguation_nt.children.at(0).release()));
					assert(epsilon_t.value == tools::bnf::epsilon);
				}
			}
		}
	};
}
