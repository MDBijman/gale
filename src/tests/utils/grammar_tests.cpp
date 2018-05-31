#include "utils/parsing/ebnfe_parser.h"

#include <catch2/catch.hpp>
#include <vector>

TEST_CASE("an ebnfe parser should parse correctly given a set of rules", "[syntax]")
{
	utils::ebnfe::parser parser;

	parser = utils::ebnfe::parser{};

	utils::ebnfe::non_terminal Expression, Atom;
	Expression = parser.new_non_terminal();
	Atom = parser.new_non_terminal();

	utils::ebnfe::terminal id, plus, number;
	id = parser.new_terminal();
	plus = parser.new_terminal();
	number = parser.new_terminal();

	using namespace utils::ebnf::meta;

	parser.new_rule({ Expression, {
		Atom, plus, Expression, alt, Atom
	} });
	parser.new_rule({ Atom, { number, alt, id } });
	parser.new_transformation(Atom, utils::ebnfe::transformation_type::REPLACE_WITH_CHILDREN);
	parser.new_transformation(Expression, utils::ebnfe::transformation_type::REPLACE_IF_ONE_CHILD);

	using namespace utils;

	SECTION("an id as a value is parsed")
	{
		parser.generate(Expression);
		auto output_or_error = parser.parse(std::vector<utils::bnf::terminal_node>{
			{ id, "a" }, { plus, "+" }, { number, "5" }
		});

		auto& output = std::get<std::unique_ptr<utils::ebnfe::node>>(output_or_error);

		// Expression
		auto& root = std::get<utils::ebnfe::non_terminal_node>(*output);
		REQUIRE(root.value == Expression);

		// id
		{
			auto& i = std::get<ebnfe::terminal_node>(*root.children.at(0).get());
			REQUIRE(i.value == id);
			REQUIRE(i.token == "a");
		}

		// plus
		{
			auto& p = std::get<ebnfe::terminal_node>(*root.children.at(1).get());
			REQUIRE(p.value == plus);
			REQUIRE(p.token == "+");
		}

		// number
		{
			auto& num = std::get<ebnfe::terminal_node>(*root.children.at(2).get());
			REQUIRE(num.value == number);
			REQUIRE(num.token == "5");
		}
	}

	SECTION("a single id is parsed")
	{
		parser.generate(Expression);
		auto output_or_error = parser.parse(std::vector<utils::bnf::terminal_node>{
			{ id, "a" }
		});

		auto& output = std::get<std::unique_ptr<utils::ebnfe::node>>(output_or_error);

		// Expression
		auto& root = std::get<utils::ebnfe::non_terminal_node>(*output);
		REQUIRE(root.value == Expression);

		// id 
		{
			auto& i = std::get<ebnfe::terminal_node>(*root.children.at(0).get());
			REQUIRE(i.value == id);
			REQUIRE(i.token == "a");
		}
	}
}

TEST_CASE("ebnfe grammars should be correctly translated to bnf grammars", "[syntax]")
{
	utils::ebnfe::parser parser;

	parser = utils::ebnfe::parser{};
	// terminals
	auto if_kw = parser.new_terminal();
	auto elseif_kw = parser.new_terminal();
	auto else_kw = parser.new_terminal();
	auto op = parser.new_terminal();
	auto block = parser.new_terminal();

	// non terminals
	auto if_expr = parser.new_non_terminal();
	auto elseif_expr = parser.new_non_terminal();
	auto else_expr = parser.new_non_terminal();

	using namespace utils::ebnf::meta;
	parser
		.new_rule({ if_expr, { if_kw, op, block, lrb, elseif_expr, rrb, star, lsb, else_expr, rsb } })
		.new_rule({ elseif_expr, { elseif_kw, op, block } })
		.new_rule({ else_expr, { else_kw, block } });


	auto conv = [](std::vector<utils::ebnfe::terminal> ts) {
		std::vector<utils::bnf::terminal_node> output;
		for (auto&x : ts) output.push_back({ x, "" });
		return output;
	};
	parser.generate(if_expr);
	auto output_or_error = parser.parse(conv({
		if_kw, op, block, elseif_kw, op, block, elseif_kw, op, block, else_kw, block
	}));

	REQUIRE(std::holds_alternative<std::unique_ptr<utils::ebnfe::node>>(output_or_error));
}