#include "utils/parsing/recursive_descent_parser.h"
#include "fe/language_definition.h"
#include "fe/data/ast_data.h"
#include <optional>
#include <string_view>

namespace recursive_descent
{
	// Token Stream Reader

	token_stream_reader::token_stream_reader(std::vector<lexing::token>& in) : in(in), curr(in.begin()) {}

	const lexing::token& token_stream_reader::peek(int n)
	{
		assert(curr + n != in.end());
		return *(curr + n);
	}

	lexing::token token_stream_reader::next()
	{
		assert(curr != in.end());
		return *curr++;
	}

	void token_stream_reader::consume(lexing::token_kind t)
	{
		auto n = next();
		assert(n.value == t);
	}

	bool token_stream_reader::has_next()
	{
		return curr != in.end();
	}

	// Utility

	void link_child_parent(fe::node_id child, fe::node_id parent, tree& t)
	{
		t.get_node(child).parent_id = parent;
		t.children_of(parent).push_back(child);
	}

	// Parsing

	using lexing::token_kind;
	using namespace fe::non_terminals;

	fe::node_id parse_expression(tree& t, token_stream_reader& ts);
	fe::node_id parse_operation(tree& t, token_stream_reader& ts);
	fe::node_id parse_statement(tree& t, token_stream_reader& ts);
	fe::node_id parse_type_operation(tree& t, token_stream_reader& ts);
	fe::node_id parse_sum_type(tree& t, token_stream_reader& ts);
	fe::node_id parse_number(tree& t, token_stream_reader& ts);

	std::vector<std::string> split_on(std::string id, char split_on)
	{
		std::vector<std::string> split_identifier;

		std::string::iterator begin_word = id.begin();
		for (auto it = id.begin(); it != id.end(); it++)
		{
			if (*it == split_on)
			{
				// Read infix
				split_identifier.push_back(std::string(&*begin_word, std::distance(begin_word, it)));
				begin_word = it + 1;
				continue;
			}
			else if(it == id.end())
			{
				split_identifier.push_back(std::string(&*begin_word, std::distance(begin_word, it)));
			}
		}
		split_identifier.push_back(std::string(&*begin_word, std::distance(begin_word, id.end())));
		return split_identifier;
	};

	fe::node_id parse_identifier(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == lexing::token_kind::IDENTIFIER);
		auto id = t.create_node(fe::ext_ast::node_type::IDENTIFIER);

		auto module_name = split_on(std::string(next.text), '.');
		t.get_data<fe::ext_ast::identifier>(t.get_node(id).data_index).name = module_name.back();
		t.get_data<fe::ext_ast::identifier>(t.get_node(id).data_index).module_path = std::vector<std::string>(module_name.begin(), module_name.end() - 1);
		t.get_data<fe::ext_ast::identifier>(t.get_node(id).data_index).full = next.text;
		return id;
	}

	fe::node_id parse_module_declaration(tree& t, token_stream_reader& ts)
	{
		auto module_declaration_id = t.create_node(fe::ext_ast::node_type::MODULE_DECLARATION);

		ts.consume(token_kind::MODULE_KEYWORD);
		link_child_parent(parse_identifier(t, ts), module_declaration_id, t);

		return module_declaration_id;
	}

	fe::node_id parse_module_imports(tree& t, token_stream_reader& ts)
	{
		auto module_imports_id = t.create_node(fe::ext_ast::node_type::IMPORT_DECLARATION);

		ts.consume(token_kind::IMPORT_KEYWORD);
		ts.consume(token_kind::LEFT_SQUARE_BRACKET);
		while (ts.peek().value == token_kind::IDENTIFIER)
			link_child_parent(parse_identifier(t, ts), module_imports_id, t);
		ts.consume(token_kind::RIGHT_SQUARE_BRACKET);

		return module_imports_id;
	}

	fe::node_id parse_reference_type(tree& t, token_stream_reader& ts)
	{
		auto reference_type_id = t.create_node(fe::ext_ast::node_type::REFERENCE_TYPE);

		ts.consume(token_kind::REF_KEYWORD);
		link_child_parent(parse_type_operation(t, ts), reference_type_id, t);

		return reference_type_id;
	}

	fe::node_id parse_array_type(tree& t, token_stream_reader& ts)
	{
		auto array_type_id = t.create_node(fe::ext_ast::node_type::ARRAY_TYPE);

		ts.consume(token_kind::LEFT_SQUARE_BRACKET);
		link_child_parent(parse_sum_type(t, ts), array_type_id, t);
		ts.consume(token_kind::SEMICOLON);
		link_child_parent(parse_number(t, ts), array_type_id, t);
		ts.consume(token_kind::RIGHT_SQUARE_BRACKET);

		return array_type_id;
	}

	fe::node_id parse_type_tuple(tree& t, token_stream_reader& ts)
	{
		auto type_tuple_id = t.create_node(fe::ext_ast::node_type::TUPLE_TYPE);

		ts.consume(token_kind::LEFT_BRACKET);
		link_child_parent(parse_sum_type(t, ts), type_tuple_id, t);
		while (ts.peek().value == token_kind::COMMA)
		{
			ts.consume(token_kind::COMMA);
			link_child_parent(parse_sum_type(t, ts), type_tuple_id, t);
		}
		ts.consume(token_kind::RIGHT_BRACKET);

		return type_tuple_id;
	}

	fe::node_id parse_type_expression(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();

		if (next.value == token_kind::IDENTIFIER)
		{
			auto type_atom_id = t.create_node(fe::ext_ast::node_type::ATOM_TYPE);
			link_child_parent(parse_identifier(t, ts), type_atom_id, t);
			return type_atom_id;
		}
		else if (next.value == token_kind::LEFT_BRACKET)
		{
			return parse_type_tuple(t, ts);
		}
		else if (next.value == token_kind::LEFT_SQUARE_BRACKET)
		{
			return parse_array_type(t, ts);
		}
		else
		{
			throw error{ "Expected identifier or left_bracket" };
		}
	}

	fe::node_id parse_type_operation(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();
		if (next.value == token_kind::REF_KEYWORD)
		{
			return parse_reference_type(t, ts);
		}
		else
		{
			auto expression = parse_type_expression(t, ts);

			next = ts.peek();
			if (next.value == token_kind::RIGHT_ARROW)
			{
				auto function_type_id = t.create_node(fe::ext_ast::node_type::FUNCTION_TYPE);
				link_child_parent(expression, function_type_id, t);

				ts.consume(token_kind::RIGHT_ARROW);
				link_child_parent(parse_type_operation(t, ts), function_type_id, t);

				return function_type_id;
			}
			else
			{
				return expression;
			}
		}
	}

	fe::node_id parse_sum_type(tree& t, token_stream_reader& ts)
	{
		auto maybe_colon = ts.peek(1);
		if (maybe_colon.value != token_kind::COLON)
			return parse_type_operation(t, ts);

		auto sum = t.create_node(fe::ext_ast::node_type::SUM_TYPE);

		auto id = parse_identifier(t, ts);
		link_child_parent(id, sum, t);
		ts.consume(token_kind::COLON);
		auto op = parse_type_operation(t, ts);
		link_child_parent(op, sum, t);

		while (ts.peek().value == token_kind::VERTICAL_LINE)
		{
			ts.consume(token_kind::VERTICAL_LINE);

			auto next_id = parse_identifier(t, ts);
			link_child_parent(next_id, sum, t);
			ts.consume(token_kind::COLON);
			auto next_op = parse_type_operation(t, ts);
			link_child_parent(next_op, sum, t);
		}

		return sum;
	}

	fe::node_id parse_type_definition(tree& t, token_stream_reader& ts)
	{
		auto type_definition_id = t.create_node(fe::ext_ast::node_type::TYPE_DEFINITION);

		ts.consume(token_kind::TYPE_KEYWORD);
		link_child_parent(parse_identifier(t, ts), type_definition_id, t);
		ts.consume(token_kind::EQUALS);
		link_child_parent(parse_sum_type(t, ts), type_definition_id, t);

		return type_definition_id;
	}

	fe::node_id parse_identifier_tuple(tree& t, token_stream_reader& ts)
	{
		auto identifier_tuple_id = t.create_node(fe::ext_ast::node_type::IDENTIFIER_TUPLE);

		ts.consume(token_kind::LEFT_BRACKET);
		link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		ts.consume(token_kind::COMMA);
		link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		while (ts.peek().value == token_kind::COMMA)
		{
			ts.consume(token_kind::COMMA);
			link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		}
		ts.consume(token_kind::RIGHT_BRACKET);

		return identifier_tuple_id;
	}

	fe::node_id parse_assignable(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();
		if (next.value == token_kind::IDENTIFIER)   return parse_identifier(t, ts);
		else if (next.value == token_kind::LEFT_BRACKET) return parse_identifier_tuple(t, ts);
		else throw error{ "Expected token_kind::IDENTIFIER or left bracket" };
	}

	fe::node_id parse_declaration(tree& t, token_stream_reader& ts)
	{
		auto declaration_id = t.create_node(fe::ext_ast::node_type::DECLARATION);

		ts.consume(token_kind::LET_KEYWORD);
		link_child_parent(parse_assignable(t, ts), declaration_id, t);
		ts.consume(token_kind::COLON);
		link_child_parent(parse_sum_type(t, ts), declaration_id, t);
		ts.consume(token_kind::EQUALS);
		link_child_parent(parse_operation(t, ts), declaration_id, t);

		return declaration_id;
	}

	fe::node_id parse_assignment(tree& t, token_stream_reader& ts)
	{
		auto assignment_id = t.create_node(fe::ext_ast::node_type::ASSIGNMENT);

		link_child_parent(parse_identifier(t, ts), assignment_id, t);
		ts.consume(token_kind::EQUALS);
		link_child_parent(parse_operation(t, ts), assignment_id, t);

		return assignment_id;
	}

	std::vector<fe::node_id> parse_block_elements(tree& t, token_stream_reader& ts)
	{
		std::vector<fe::node_id> res;

		while (ts.peek().value != token_kind::RIGHT_CURLY_BRACKET)
		{
			// Either result expression or statement
			auto element_id = parse_statement(t, ts);
			auto& element_node = t.get_node(element_id);

			auto first_after_element = ts.peek();
			if (first_after_element.value == token_kind::SEMICOLON)
			{
				res.push_back(element_id);
				ts.consume(token_kind::SEMICOLON);
			}
			else if (first_after_element.value == token_kind::RIGHT_CURLY_BRACKET)
			{
				if (element_node.kind == fe::ext_ast::node_type::TYPE_DEFINITION
					|| element_node.kind == fe::ext_ast::node_type::DECLARATION
					|| element_node.kind == fe::ext_ast::node_type::WHILE_LOOP
					|| element_node.kind == fe::ext_ast::node_type::ASSIGNMENT)
					throw error{ "Expected expression as block result, not a statement" };

				auto block_res = t.create_node(fe::ext_ast::node_type::BLOCK_RESULT);
				link_child_parent(element_id, block_res, t);
				res.push_back(block_res);
			}
			else
			{
				throw error{ "Expected semicolon or end of block" };
			}
		}

		return res;
	}

	fe::node_id parse_block(tree& t, token_stream_reader& ts)
	{
		auto block_id = t.create_node(fe::ext_ast::node_type::BLOCK);

		ts.consume(token_kind::LEFT_CURLY_BRACKET);
		auto elements = parse_block_elements(t, ts);
		for (auto element_id : elements) link_child_parent(element_id, block_id, t);
		ts.consume(token_kind::RIGHT_CURLY_BRACKET);

		return block_id;
	}

	fe::node_id parse_while_loop(tree& t, token_stream_reader& ts)
	{
		auto while_id = t.create_node(fe::ext_ast::node_type::WHILE_LOOP);

		ts.consume(token_kind::WHILE_KEYWORD);
		ts.consume(token_kind::LEFT_BRACKET);
		link_child_parent(parse_operation(t, ts), while_id, t);
		ts.consume(token_kind::RIGHT_BRACKET);
		link_child_parent(parse_block(t, ts), while_id, t);

		return while_id;
	}

	fe::node_id parse_word(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == token_kind::WORD);
		auto id = t.create_node(fe::ext_ast::node_type::STRING);
		t.get_data<fe::string>(t.get_node(id).data_index).value = next.text.substr(1, next.text.size() - 2);
		return id;
	}

	fe::node_id parse_array_value(tree& t, token_stream_reader& ts)
	{
		auto array_value_id = t.create_node(fe::ext_ast::node_type::ARRAY_VALUE);
		ts.consume(token_kind::LEFT_SQUARE_BRACKET);
		link_child_parent(parse_operation(t, ts), array_value_id, t);

		while (ts.peek().value == token_kind::COMMA)
		{
			ts.consume(token_kind::COMMA);
			link_child_parent(parse_operation(t, ts), array_value_id, t);
		}
		ts.consume(token_kind::RIGHT_SQUARE_BRACKET);
		return array_value_id;
	}

	fe::node_id parse_number(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == token_kind::NUMBER);
		auto id = t.create_node(fe::ext_ast::node_type::NUMBER);
		t.get_data<fe::number>(t.get_node(id).data_index).value = std::strtoll(next.text.data(), nullptr, 10);
		return id;
	}

	fe::node_id parse_true(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == token_kind::TRUE_KEYWORD);
		auto id = t.create_node(fe::ext_ast::node_type::BOOLEAN);
		t.get_data<fe::boolean>(t.get_node(id).data_index).value = true;
		return id;
	}

	fe::node_id parse_false(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == token_kind::FALSE_KEYWORD);
		auto id = t.create_node(fe::ext_ast::node_type::BOOLEAN);
		t.get_data<fe::boolean>(t.get_node(id).data_index).value = false;
		return id;
	}

	std::pair<fe::node_id, fe::node_id> parse_elseif_expr(tree& t, token_stream_reader& ts)
	{
		ts.consume(token_kind::ELSEIF_KEYWORD);
		ts.consume(token_kind::LEFT_BRACKET);
		auto test_id = parse_operation(t, ts);
		ts.consume(token_kind::RIGHT_BRACKET);
		auto body_id = parse_block(t, ts);
		return std::make_pair(test_id, body_id);
	}

	fe::node_id parse_else_expr(tree& t, token_stream_reader& ts)
	{
		ts.consume(token_kind::ELSE_KEYWORD);
		return parse_block(t, ts);
	}

	fe::node_id parse_if_expr(tree& t, token_stream_reader& ts)
	{
		auto if_expr_id = t.create_node(fe::ext_ast::node_type::IF_STATEMENT);

		ts.consume(token_kind::IF_KEYWORD);
		ts.consume(token_kind::LEFT_BRACKET);
		link_child_parent(parse_operation(t, ts), if_expr_id, t);
		ts.consume(token_kind::RIGHT_BRACKET);
		link_child_parent(parse_block(t, ts), if_expr_id, t);
		while (ts.peek().value == token_kind::ELSEIF_KEYWORD)
		{
			auto elseif = parse_elseif_expr(t, ts);
			link_child_parent(elseif.first, if_expr_id, t);
			link_child_parent(elseif.second, if_expr_id, t);
		}

		if (ts.peek().value == token_kind::ELSE_KEYWORD)
		{
			link_child_parent(parse_else_expr(t, ts), if_expr_id, t);
		}

		return if_expr_id;
	}

	fe::node_id parse_function(tree& t, token_stream_reader& ts)
	{
		auto function_id = t.create_node(fe::ext_ast::node_type::FUNCTION);

		ts.consume(token_kind::BACKSLASH);
		link_child_parent(parse_assignable(t, ts), function_id, t);
		ts.consume(token_kind::FAT_RIGHT_ARROW);
		link_child_parent(parse_expression(t, ts), function_id, t);

		return function_id;
	}

	fe::node_id parse_expression(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek().value;
		if (next == token_kind::WORD) return parse_word(t, ts);
		else if (next == token_kind::LEFT_BRACKET)
		{
			// () becomes empty value tuple
			ts.consume(token_kind::LEFT_BRACKET);
			if (ts.peek().value == token_kind::RIGHT_BRACKET)
			{
				ts.consume(token_kind::RIGHT_BRACKET);
				return t.create_node(fe::ext_ast::node_type::TUPLE);
			}

			auto op_id = parse_operation(t, ts);

			// ( op ) becomes brackets, not a tuple
			if (ts.peek().value == token_kind::RIGHT_BRACKET)
			{
				ts.consume(token_kind::RIGHT_BRACKET);
				return op_id;
			}

			// ( op , op (, op)* ) becomes value tuple
			auto tuple_id = t.create_node(fe::ext_ast::node_type::TUPLE);
			link_child_parent(op_id, tuple_id, t);
			while (ts.peek().value == token_kind::COMMA)
			{
				ts.consume(token_kind::COMMA);
				link_child_parent(parse_operation(t, ts), tuple_id, t);
			}
			ts.consume(token_kind::RIGHT_BRACKET);

			return tuple_id;
		}
		else if (next == token_kind::LEFT_CURLY_BRACKET) return parse_block(t, ts);
		else if (next == token_kind::LEFT_SQUARE_BRACKET) return parse_array_value(t, ts);
		else if (next == token_kind::NUMBER) return parse_number(t, ts);
		else if (next == token_kind::IDENTIFIER) return parse_identifier(t, ts);
		else if (next == token_kind::TRUE_KEYWORD) return parse_true(t, ts);
		else if (next == token_kind::FALSE_KEYWORD) return parse_false(t, ts);
		else if (next == token_kind::IF_KEYWORD) return parse_if_expr(t, ts);
		else if (next == token_kind::BACKSLASH) return parse_function(t, ts);
		else throw error{ "Expression error" };
	}

	fe::node_id parse_function_call(tree& t, token_stream_reader& ts)
	{
		if (ts.peek().value != token_kind::IDENTIFIER)
			return parse_expression(t, ts);

		auto identifier_id = parse_identifier(t, ts);

		auto next = ts.peek().value;
		if (next == token_kind::WORD
			|| next == token_kind::LEFT_BRACKET
			|| next == token_kind::LEFT_CURLY_BRACKET
			|| next == token_kind::LEFT_SQUARE_BRACKET
			|| next == token_kind::NUMBER
			|| next == token_kind::IDENTIFIER
			|| next == token_kind::TRUE_KEYWORD
			|| next == token_kind::FALSE_KEYWORD
			|| next == token_kind::IF_KEYWORD
			|| next == token_kind::BACKSLASH)
		{
			auto function_call_id = t.create_node(fe::ext_ast::node_type::FUNCTION_CALL);

			link_child_parent(identifier_id, function_call_id, t);
			link_child_parent(parse_function_call(t, ts), function_call_id, t);
			return function_call_id;
		}

		return identifier_id;
	}

	fe::node_id parse_array_access(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_function_call(t, ts);

		if (ts.peek().value != token_kind::ARRAY_ACCESS)
			return lhs_id;

		ts.consume(token_kind::ARRAY_ACCESS);

		auto access_id = t.create_node(fe::ext_ast::node_type::ARRAY_ACCESS);

		link_child_parent(lhs_id, access_id, t);
		link_child_parent(parse_array_access(t, ts), access_id, t);
		return access_id;
	}

	fe::node_id parse_reference(tree& t, token_stream_reader& ts)
	{
		if (ts.peek().value != token_kind::REF_KEYWORD)
			return parse_array_access(t, ts);

		auto reference_id = t.create_node(fe::ext_ast::node_type::REFERENCE);

		ts.consume(token_kind::REF_KEYWORD);
		link_child_parent(parse_reference(t, ts), reference_id, t);

		return reference_id;
	}

	fe::node_id parse_term(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_reference(t, ts);

		if (ts.peek().value == token_kind::MUL)
		{
			auto term_id = t.create_node(fe::ext_ast::node_type::MULTIPLICATION);

			link_child_parent(lhs_id, term_id, t);
			ts.consume(token_kind::MUL);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}
		else if (ts.peek().value == token_kind::DIV)
		{
			auto term_id = t.create_node(fe::ext_ast::node_type::DIVISION);

			link_child_parent(lhs_id, term_id, t);
			ts.consume(token_kind::DIV);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}
		else if (ts.peek().value == token_kind::PERCENTAGE)
		{
			auto term_id = t.create_node(fe::ext_ast::node_type::MODULO);

			link_child_parent(lhs_id, term_id, t);
			ts.consume(token_kind::PERCENTAGE);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}

		return lhs_id;
	}

	fe::node_id parse_arithmetic(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_term(t, ts);

		if (ts.peek().value == token_kind::PLUS)
		{
			auto arithmetic_id = t.create_node(fe::ext_ast::node_type::ADDITION);

			link_child_parent(lhs_id, arithmetic_id, t);
			ts.consume(token_kind::PLUS);
			link_child_parent(parse_arithmetic(t, ts), arithmetic_id, t);

			return arithmetic_id;
		}
		else if (ts.peek().value == token_kind::MINUS)
		{
			auto arithmetic_id = t.create_node(fe::ext_ast::node_type::SUBTRACTION);

			link_child_parent(lhs_id, arithmetic_id, t);
			ts.consume(token_kind::MINUS);
			link_child_parent(parse_arithmetic(t, ts), arithmetic_id, t);

			return arithmetic_id;
		}

		return lhs_id;
	}

	fe::node_id parse_comparison(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_arithmetic(t, ts);

		if (ts.peek().value == token_kind::TWO_EQUALS)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::EQUALITY);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(token_kind::TWO_EQUALS);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == token_kind::LTEQ)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::LESS_OR_EQ);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(token_kind::LTEQ);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == token_kind::LEFT_ANGLE_BRACKET)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::LESS_THAN);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(token_kind::LEFT_ANGLE_BRACKET);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == token_kind::GTEQ)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::GREATER_OR_EQ);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(token_kind::GTEQ);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == token_kind::RIGHT_ANGLE_BRACKET)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::GREATER_THAN);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(token_kind::RIGHT_ANGLE_BRACKET);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}

		return lhs_id;
	}

	fe::node_id parse_logical(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_comparison(t, ts);

		if (ts.peek().value == token_kind::AND)
		{
			auto logical_id = t.create_node(fe::ext_ast::node_type::AND);

			link_child_parent(lhs_id, logical_id, t);
			ts.consume(token_kind::AND);
			link_child_parent(parse_logical(t, ts), logical_id, t);

			return logical_id;
		}
		else if (ts.peek().value == token_kind::OR)
		{
			auto logical_id = t.create_node(fe::ext_ast::node_type::OR);

			link_child_parent(lhs_id, logical_id, t);
			ts.consume(token_kind::OR);
			link_child_parent(parse_logical(t, ts), logical_id, t);

			return logical_id;
		}

		return lhs_id;
	}

	fe::node_id parse_match_branch(tree& t, token_stream_reader& ts)
	{
		auto branch_id = t.create_node(fe::ext_ast::node_type::MATCH_BRANCH);

		ts.consume(token_kind::VERTICAL_LINE);
		link_child_parent(parse_operation(t, ts), branch_id, t);
		ts.consume(token_kind::RIGHT_ARROW);
		link_child_parent(parse_operation(t, ts), branch_id, t);

		return branch_id;
	}

	fe::node_id parse_match(tree& t, token_stream_reader& ts)
	{
		auto logical_id = parse_logical(t, ts);
		if (ts.peek().value != token_kind::MATCH_KEYWORD)
			return logical_id;

		auto match_id = t.create_node(fe::ext_ast::node_type::MATCH);
		link_child_parent(logical_id, match_id, t);

		ts.consume(token_kind::MATCH_KEYWORD);
		ts.consume(token_kind::LEFT_CURLY_BRACKET);
		while (ts.peek().value == token_kind::VERTICAL_LINE)
		{
			link_child_parent(parse_match_branch(t, ts), match_id, t);
		}
		ts.consume(token_kind::RIGHT_CURLY_BRACKET);

		return match_id;
	}

	fe::node_id parse_operation(tree& t, token_stream_reader& ts)
	{
		return parse_match(t, ts);
	}

	fe::node_id parse_statement(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();
		if (next.value == token_kind::TYPE_KEYWORD)       return parse_type_definition(t, ts);
		else if (next.value == token_kind::LET_KEYWORD)   return parse_declaration(t, ts);
		else if (next.value == token_kind::IDENTIFIER
			&& ts.peek(1).value == token_kind::EQUALS)  return parse_assignment(t, ts);
		else if (next.value == token_kind::WHILE_KEYWORD) return parse_while_loop(t, ts);
		else    /* operation */                return parse_operation(t, ts);
	}

	fe::node_id parse_stmt_semicln(tree& t, token_stream_reader& ts)
	{
		auto stmt = parse_statement(t, ts);
		ts.consume(token_kind::SEMICOLON);
		return stmt;
	}

	fe::node_id parse_file(tree& t, token_stream_reader& ts)
	{
		auto file_node = t.create_node(fe::ext_ast::node_type::BLOCK);
		t.set_root_id(file_node);

		if (ts.peek().value == token_kind::MODULE_KEYWORD) link_child_parent(parse_module_declaration(t, ts), file_node, t);

		if (ts.peek().value == token_kind::IMPORT_KEYWORD) link_child_parent(parse_module_imports(t, ts), file_node, t);

		while (ts.peek().value != token_kind::END_OF_INPUT)
			link_child_parent(parse_stmt_semicln(t, ts), file_node, t);

		ts.consume(token_kind::END_OF_INPUT);

		return file_node;
	}

	void generate()
	{
		using namespace fe::non_terminals;
		non_terminal last_non_terminal = 1;
		file = last_non_terminal++;
		statement = last_non_terminal++;
		declaration = last_non_terminal++;
		expression = last_non_terminal++;
		value_tuple = last_non_terminal++;
		function = last_non_terminal++;
		match = last_non_terminal++;
		match_branch = last_non_terminal++;
		type_expression = last_non_terminal++;
		type_tuple = last_non_terminal++;
		function_type = last_non_terminal++;
		type_definition = last_non_terminal++;
		module_declaration = last_non_terminal++;
		block = last_non_terminal++;
		function_call = last_non_terminal++;
		type_atom = last_non_terminal++;
		reference_type = last_non_terminal++;
		array_type = last_non_terminal++;
		reference = last_non_terminal++;
		array_value = last_non_terminal++;
		operation = last_non_terminal++;
		term = last_non_terminal++;
		addition = last_non_terminal++;
		subtraction = last_non_terminal++;
		division = last_non_terminal++;
		multiplication = last_non_terminal++;
		brackets = last_non_terminal++;
		module_imports = last_non_terminal++;
		while_loop = last_non_terminal++;
		arithmetic = last_non_terminal++;
		equality = last_non_terminal++;
		type_operation = last_non_terminal++;
		type_modifiers = last_non_terminal++;
		assignable = last_non_terminal++;
		identifier_tuple = last_non_terminal++;
		assignment = last_non_terminal++;
		greater_than = last_non_terminal++;
		modulo = last_non_terminal++;
		less_or_equal = last_non_terminal++;
		comparison = last_non_terminal++;
		greater_or_equal = last_non_terminal++;
		less_than = last_non_terminal++;
		if_expr = last_non_terminal++;
		elseif_expr = last_non_terminal++;
		else_expr = last_non_terminal++;
		stmt_semicln = last_non_terminal++;
		block_elements = last_non_terminal++;
		block_result = last_non_terminal++;
		record = last_non_terminal++;
		record_element = last_non_terminal++;
		logical = last_non_terminal++;
		and_expr = last_non_terminal++;
		or_expr = last_non_terminal++;
	}

	std::variant<tree, error> parse(std::vector<lexing::token>& in)
	{
		tree t(fe::ext_ast::ast_allocation_hints{ 3000012,1000006,0,0,1000006,0,0,1000002 });

		try
		{
			parse_file(t, token_stream_reader(in));
			return t;
		}
		catch (error e)
		{
			return e;
		}
	}
}