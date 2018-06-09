#include "utils/parsing/recursive_descent_parser.h"
#include "fe/language_definition.h"
#include "fe/data/ast_data.h"
#include <optional>
#include <string_view>

namespace recursive_descent
{
	// Token Stream Reader

	void token_stream_reader::wait_for_pipe()
	{
		auto temp = std::move(curr);
		curr = in.receive();
		std::reverse(curr.begin(), curr.end());
		for (auto&& elem : temp) curr.push_back(elem);
	}

	token_stream_reader::token_stream_reader(memory::pipe<std::vector<lexing::token>>& in) : in(in) {}

	const lexing::token& token_stream_reader::peek(int n)
	{
		if (curr.size() <= n) wait_for_pipe();
		return *(curr.rbegin() + n);
	}

	lexing::token token_stream_reader::next()
	{
		if (curr.size() == 0) wait_for_pipe();
		auto temp = std::move(curr.back());
		curr.pop_back();
		return temp;
	}

	void token_stream_reader::consume(lexing::token_id t)
	{
		auto n = next();
		assert(n.value == t);
	}

	bool token_stream_reader::has_next()
	{
		return curr.size() > 0;
	}

	// Utility

	void link_child_parent(fe::node_id child, fe::node_id parent, tree& t)
	{
		t.get_node(child).parent_id = parent;
		t.get_node(parent).children.push_back(child);
	}

	// Parsing

	using namespace fe::tokens;
	using namespace fe::non_terminals;

	fe::node_id parse_expression(tree& t, token_stream_reader& ts);
	fe::node_id parse_operation(tree& t, token_stream_reader& ts);
	fe::node_id parse_statement(tree& t, token_stream_reader& ts);
	fe::node_id parse_type_operation(tree& t, token_stream_reader& ts);


	std::vector<std::string_view> split_on(std::string_view identifier, char split_on)
	{
		std::vector<std::string_view> split_identifier;

		std::string_view::iterator begin_word = identifier.begin();
		for (auto it = identifier.begin(); it != identifier.end(); it++)
		{
			if (*it == split_on)
			{
				// Read infix
				split_identifier.push_back(std::string_view(&*begin_word, 1));
				begin_word = it + 1;
				continue;
			}
			else
			{
				if (it == identifier.end() - 1)
					split_identifier.push_back(std::string_view(&*begin_word, std::distance(begin_word, it)));
			}
		}
		return split_identifier;
	};

	fe::node_id parse_identifier(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == identifier);
		auto id = t.create_node(fe::ext_ast::node_type::IDENTIFIER);

		t.get_data<fe::ext_ast::identifier>(*t.get_node(id).data_index).segments = split_on(next.text, '.');
		t.get_data<fe::ext_ast::identifier>(*t.get_node(id).data_index).full = next.text;
		return id;
	}

	fe::node_id parse_module_declaration(tree& t, token_stream_reader& ts)
	{
		auto module_declaration_id = t.create_node(fe::ext_ast::node_type::MODULE_DECLARATION);

		ts.consume(module_keyword);
		link_child_parent(parse_identifier(t, ts), module_declaration_id, t);

		return module_declaration_id;
	}

	fe::node_id parse_module_imports(tree& t, token_stream_reader& ts)
	{
		auto module_imports_id = t.create_node(fe::ext_ast::node_type::IMPORT_DECLARATION);

		ts.consume(import_keyword);
		ts.consume(left_square_bracket);
		while (ts.peek().value == identifier)
			link_child_parent(parse_identifier(t, ts), module_imports_id, t);
		ts.consume(right_square_bracket);

		return module_imports_id;
	}

	fe::node_id parse_reference_type(tree& t, token_stream_reader& ts)
	{
		auto reference_type_id = t.create_node(fe::ext_ast::node_type::REFERENCE_TYPE);

		ts.consume(ref_keyword);
		link_child_parent(parse_type_operation(t, ts), reference_type_id, t);

		return reference_type_id;
	}

	fe::node_id parse_array_type(tree& t, token_stream_reader& ts)
	{
		auto array_type_id = t.create_node(fe::ext_ast::node_type::ARRAY_TYPE);

		ts.consume(left_square_bracket);
		ts.consume(right_square_bracket);
		link_child_parent(parse_type_operation(t, ts), array_type_id, t);

		return array_type_id;
	}

	fe::node_id parse_type_tuple(tree& t, token_stream_reader& ts)
	{
		auto type_tuple_id = t.create_node(fe::ext_ast::node_type::ARRAY_TYPE);

		ts.consume(left_bracket);
		link_child_parent(parse_type_operation(t, ts), type_tuple_id, t);
		while (ts.peek().value == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_type_operation(t, ts), type_tuple_id, t);
		}
		ts.consume(right_bracket);

		return type_tuple_id;
	}

	fe::node_id parse_type_expression(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();

		if (next.value == identifier)
		{
			auto type_atom_id = t.create_node(fe::ext_ast::node_type::TYPE_ATOM);
			link_child_parent(parse_identifier(t, ts), type_atom_id, t);
			return type_atom_id;
		}
		else if (next.value == left_bracket) return parse_type_tuple(t, ts);
		else throw error{ "Expected identifier or left_bracket" };
	}

	fe::node_id parse_type_operation(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();
		if (next.value == ref_keyword)         return parse_reference_type(t, ts);
		else if (next.value == left_square_bracket) return parse_array_type(t, ts);
		else
		{
			auto expression = parse_type_expression(t, ts);
			next = ts.peek();
			if (next.value == right_arrow)
			{
				auto function_type_id = t.create_node(fe::ext_ast::node_type::FUNCTION_TYPE);

				link_child_parent(expression, function_type_id, t);
				ts.consume(right_arrow);
				link_child_parent(parse_type_operation(t, ts), function_type_id, t);

				return function_type_id;
			}
			else return expression;
		}
	}

	fe::node_id parse_record_element(tree& t, token_stream_reader& ts)
	{
		auto record_element_id = t.create_node(fe::ext_ast::node_type::RECORD_ELEMENT);

		link_child_parent(parse_identifier(t, ts), record_element_id, t);
		ts.consume(colon);
		link_child_parent(parse_type_operation(t, ts), record_element_id, t);

		return record_element_id;
	}

	fe::node_id parse_record(tree& t, token_stream_reader& ts)
	{
		auto record_id = t.create_node(fe::ext_ast::node_type::RECORD);

		ts.consume(left_bracket);
		link_child_parent(parse_record_element(t, ts), record_id, t);
		while (ts.peek().value == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_record_element(t, ts), record_id, t);
		}
		ts.consume(left_bracket);

		return record_id;
	}

	fe::node_id parse_type_definition(tree& t, token_stream_reader& ts)
	{
		auto type_definition_id = t.create_node(fe::ext_ast::node_type::TYPE_DEFINITION);

		ts.consume(type_keyword);
		link_child_parent(parse_identifier(t, ts), type_definition_id, t);
		link_child_parent(parse_record(t, ts), type_definition_id, t);

		return type_definition;
	}

	fe::node_id parse_identifier_tuple(tree& t, token_stream_reader& ts)
	{
		auto identifier_tuple_id = t.create_node(fe::ext_ast::node_type::IDENTIFIER_TUPLE);

		ts.consume(left_bracket);
		link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		ts.consume(comma);
		link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		while (ts.peek().value == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		}
		ts.consume(right_bracket);

		return identifier_tuple_id;
	}

	fe::node_id parse_assignable(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();
		if (next.value == identifier)   return parse_identifier(t, ts);
		else if (next.value == left_bracket) return parse_identifier_tuple(t, ts);
		else throw error{ "Expected identifier or left bracket" };
	}

	fe::node_id parse_declaration(tree& t, token_stream_reader& ts)
	{
		auto declaration_id = t.create_node(fe::ext_ast::node_type::DECLARATION);

		ts.consume(let_keyword);
		link_child_parent(parse_assignable(t, ts), declaration_id, t);
		ts.consume(colon);
		link_child_parent(parse_type_operation(t, ts), declaration_id, t);
		ts.consume(equals);
		link_child_parent(parse_operation(t, ts), declaration_id, t);

		return declaration_id;
	}

	fe::node_id parse_assignment(tree& t, token_stream_reader& ts)
	{
		auto assignment_id = t.create_node(fe::ext_ast::node_type::ASSIGNMENT);

		link_child_parent(parse_identifier(t, ts), assignment_id, t);
		ts.consume(equals);
		link_child_parent(parse_operation(t, ts), assignment_id, t);

		return assignment_id;
	}

	std::vector<fe::node_id> parse_block_elements(tree& t, token_stream_reader& ts)
	{
		std::vector<fe::node_id> res;

		res.push_back(parse_statement(t, ts));
		ts.consume(semicolon);

		while (ts.peek().value != right_curly_bracket)
		{
			// Either result expression or statement
			auto element_id = parse_statement(t, ts);
			auto& element_node = t.get_node(element_id);

			auto first_after_element = ts.peek();
			if (first_after_element.value == semicolon)
			{
				res.push_back(element_id);
				ts.consume(semicolon);
			}
			else if (first_after_element.value == right_curly_bracket)
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

		ts.consume(left_curly_bracket);
		auto elements = parse_block_elements(t, ts);
		for (auto element_id : elements) link_child_parent(element_id, block_id, t);
		ts.consume(right_curly_bracket);

		return block_id;
	}

	fe::node_id parse_while_loop(tree& t, token_stream_reader& ts)
	{
		auto while_id = t.create_node(fe::ext_ast::node_type::WHILE_LOOP);

		ts.consume(while_keyword);
		ts.consume(left_bracket);
		link_child_parent(parse_operation(t, ts), while_id, t);
		ts.consume(right_bracket);
		link_child_parent(parse_block(t, ts), while_id, t);

		return while_id;
	}

	fe::node_id parse_word(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == word);
		auto id = t.create_node(fe::ext_ast::node_type::STRING);
		t.get_data<fe::string>(*t.get_node(id).data_index).value = next.text.substr(1, next.text.size() - 2);
		return id;
	}

	fe::node_id parse_array_value(tree& t, token_stream_reader& ts)
	{
		auto array_value_id = t.create_node(fe::ext_ast::node_type::ARRAY_VALUE);
		ts.consume(left_square_bracket);
		link_child_parent(parse_operation(t, ts), array_value_id, t);

		while (ts.peek().value == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_operation(t, ts), array_value_id, t);
		}
		return array_value_id;
	}

	fe::node_id parse_number(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == number);
		auto id = t.create_node(fe::ext_ast::node_type::NUMBER);
		t.get_data<fe::number>(*t.get_node(id).data_index).value = std::strtoll(next.text.data(), nullptr, 10);
		return id;
	}

	fe::node_id parse_true(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == true_keyword);
		auto id = t.create_node(fe::ext_ast::node_type::BOOLEAN);
		t.get_data<fe::boolean>(*t.get_node(id).data_index).value = true;
		return id;
	}

	fe::node_id parse_false(tree& t, token_stream_reader& ts)
	{
		auto next = ts.next();
		assert(next.value == false_keyword);
		auto id = t.create_node(fe::ext_ast::node_type::BOOLEAN);
		t.get_data<fe::boolean>(*t.get_node(id).data_index).value = false;
		return id;
	}

	std::pair<fe::node_id, fe::node_id> parse_elseif_expr(tree& t, token_stream_reader& ts)
	{
		ts.consume(elseif_keyword);
		ts.consume(left_bracket);
		auto test_id = parse_operation(t, ts);
		ts.consume(right_bracket);
		auto body_id = parse_block(t, ts);
		return std::make_pair(test_id, body_id);
	}

	fe::node_id parse_else_expr(tree& t, token_stream_reader& ts)
	{
		ts.consume(else_keyword);
		return parse_block(t, ts);
	}

	fe::node_id parse_if_expr(tree& t, token_stream_reader& ts)
	{
		auto if_expr_id = t.create_node(fe::ext_ast::node_type::IF_STATEMENT);

		ts.consume(if_keyword);
		ts.consume(left_bracket);
		link_child_parent(parse_operation(t, ts), if_expr_id, t);
		ts.consume(right_bracket);
		link_child_parent(parse_block(t, ts), if_expr_id, t);
		while (ts.peek().value == elseif_keyword)
		{
			ts.consume(elseif_keyword);
			auto elseif = parse_elseif_expr(t, ts);
			link_child_parent(elseif.first, if_expr_id, t);
			link_child_parent(elseif.second, if_expr_id, t);
		}

		if (ts.peek().value == else_keyword)
		{
			ts.consume(else_keyword);
			link_child_parent(parse_else_expr(t, ts), if_expr_id, t);
		}

		return if_expr_id;
	}

	fe::node_id parse_function(tree& t, token_stream_reader& ts)
	{
		auto function_id = t.create_node(fe::ext_ast::node_type::FUNCTION);

		ts.consume(backslash);
		link_child_parent(parse_assignable(t, ts), function_id, t);
		ts.consume(fat_right_arrow);
		link_child_parent(parse_expression(t, ts), function_id, t);

		return function_id;
	}

	fe::node_id parse_expression(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek().value;
		if (next == word) return parse_word(t, ts);
		else if (next == left_bracket)
		{
			// () becomes empty value tuple
			ts.consume(left_bracket);
			if (ts.peek().value == right_bracket)
			{
				ts.consume(right_bracket);
				return t.create_node(fe::ext_ast::node_type::TUPLE);
			}

			auto op_id = parse_operation(t, ts);

			// ( op ) becomes brackets, not a tuple
			if (ts.peek().value == right_bracket)
			{
				ts.consume(right_bracket);
				return op_id;
			}

			// ( op , op (, op)* ) becomes value tuple
			auto tuple_id = t.create_node(fe::ext_ast::node_type::TUPLE);
			while (ts.peek().value == comma)
			{
				ts.consume(comma);
				link_child_parent(parse_operation(t, ts), tuple_id, t);
			}
			ts.consume(right_bracket);

			return tuple_id;
		}
		else if (next == left_curly_bracket) return parse_block(t, ts);
		else if (next == left_square_bracket) return parse_array_value(t, ts);
		else if (next == number) return parse_number(t, ts);
		else if (next == identifier) return parse_identifier(t, ts);
		else if (next == true_keyword) return parse_true(t, ts);
		else if (next == false_keyword) return parse_false(t, ts);
		else if (next == if_keyword) return parse_if_expr(t, ts);
		else if (next == backslash) return parse_function(t, ts);
		else throw error{ "Expression error" };
	}

	fe::node_id parse_function_call(tree& t, token_stream_reader& ts)
	{
		if (ts.peek().value != identifier)
			return parse_expression(t, ts);

		auto identifier_id = parse_identifier(t, ts);

		auto next = ts.peek().value;
		if (next == word
			|| next == left_bracket
			|| next == left_curly_bracket
			|| next == left_square_bracket
			|| next == number
			|| next == identifier
			|| next == true_keyword
			|| next == false_keyword
			|| next == if_keyword
			|| next == backslash)
		{
			auto function_call_id = t.create_node(fe::ext_ast::node_type::FUNCTION_CALL);

			link_child_parent(identifier_id, function_call_id, t);
			link_child_parent(parse_function_call(t, ts), function_call_id, t);
			return function_call_id;
		}

		return identifier_id;
	}

	fe::node_id parse_reference(tree& t, token_stream_reader& ts)
	{
		if (ts.peek().value != ref_keyword)
			return parse_function_call(t, ts);

		auto reference_id = t.create_node(fe::ext_ast::node_type::REFERENCE);

		ts.consume(ref_keyword);
		link_child_parent(parse_reference(t, ts), reference_id, t);

		return reference_id;
	}

	fe::node_id parse_term(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_reference(t, ts);

		if (ts.peek().value == mul)
		{
			auto term_id = t.create_node(fe::ext_ast::node_type::MULTIPLICATION);

			link_child_parent(lhs_id, term_id, t);
			ts.consume(plus);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}
		else if (ts.peek().value == fe::tokens::div)
		{
			auto term_id = t.create_node(fe::ext_ast::node_type::DIVISION);

			link_child_parent(lhs_id, term_id, t);
			ts.consume(fe::tokens::div);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}
		else if (ts.peek().value == percentage)
		{
			auto term_id = t.create_node(fe::ext_ast::node_type::MODULO);

			link_child_parent(lhs_id, term_id, t);
			ts.consume(percentage);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}

		return lhs_id;
	}

	fe::node_id parse_arithmetic(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_term(t, ts);

		if (ts.peek().value == plus)
		{
			auto arithmetic_id = t.create_node(fe::ext_ast::node_type::ADDITION);

			link_child_parent(lhs_id, arithmetic_id, t);
			ts.consume(plus);
			link_child_parent(parse_arithmetic(t, ts), arithmetic_id, t);

			return arithmetic_id;
		}
		else if (ts.peek().value == minus)
		{
			auto arithmetic_id = t.create_node(fe::ext_ast::node_type::SUBTRACTION);

			link_child_parent(lhs_id, arithmetic_id, t);
			ts.consume(minus);
			link_child_parent(parse_arithmetic(t, ts), arithmetic_id, t);

			return arithmetic_id;
		}

		return lhs_id;
	}

	fe::node_id parse_comparison(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_arithmetic(t, ts);

		if (ts.peek().value == two_equals)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::EQUALITY);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(two_equals);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == lteq)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::LESS_OR_EQ);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(lteq);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == left_angle_bracket)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::LESS_THAN);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(left_angle_bracket);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == gteq)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::GREATER_OR_EQ);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(gteq);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek().value == right_angle_bracket)
		{
			auto comparison_id = t.create_node(fe::ext_ast::node_type::GREATER_THAN);

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(right_angle_bracket);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}

		return lhs_id;
	}

	fe::node_id parse_logical(tree& t, token_stream_reader& ts)
	{
		auto lhs_id = parse_comparison(t, ts);

		if (ts.peek().value == and_keyword)
		{
			auto logical_id = t.create_node(fe::ext_ast::node_type::AND);

			link_child_parent(lhs_id, logical_id, t);
			ts.consume(and_keyword);
			link_child_parent(parse_logical(t, ts), logical_id, t);

			return logical_id;
		}
		else if (ts.peek().value == or_keyword)
		{
			auto logical_id = t.create_node(fe::ext_ast::node_type::OR);

			link_child_parent(lhs_id, logical_id, t);
			ts.consume(or_keyword);
			link_child_parent(parse_logical(t, ts), logical_id, t);

			return logical_id;
		}

		return lhs_id;
	}

	fe::node_id parse_match_branch(tree& t, token_stream_reader& ts)
	{
		auto branch_id = t.create_node(fe::ext_ast::node_type::MATCH_BRANCH);

		ts.consume(vertical_line);
		link_child_parent(parse_operation(t, ts), branch_id, t);
		ts.consume(right_arrow);
		link_child_parent(parse_operation(t, ts), branch_id, t);

		return branch_id;
	}

	fe::node_id parse_match(tree& t, token_stream_reader& ts)
	{
		auto logical_id = parse_logical(t, ts);
		if (ts.peek().value != match_keyword)
			return logical_id;

		auto match_id = t.create_node(fe::ext_ast::node_type::MATCH);
		link_child_parent(logical_id, match_id, t);

		ts.consume(match_keyword);
		ts.consume(left_curly_bracket);
		while (ts.peek().value == vertical_line)
		{
			link_child_parent(parse_match_branch(t, ts), match_id, t);
		}
		ts.consume(right_curly_bracket);

		return match_id;
	}

	fe::node_id parse_operation(tree& t, token_stream_reader& ts)
	{
		return parse_match(t, ts);
	}

	fe::node_id parse_statement(tree& t, token_stream_reader& ts)
	{
		auto next = ts.peek();
		if (next.value == type_keyword)       return parse_type_definition(t, ts);
		else if (next.value == let_keyword)   return parse_declaration(t, ts);
		else if (next.value == identifier
			&& ts.peek(1).value == equals)  return parse_assignment(t, ts);
		else if (next.value == while_keyword) return parse_while_loop(t, ts);
		else    /* operation */                return parse_operation(t, ts);
	}

	fe::node_id parse_stmt_semicln(tree& t, token_stream_reader& ts)
	{
		auto stmt = parse_statement(t, ts);
		ts.consume(semicolon);
		return stmt;
	}

	fe::node_id parse_file(tree& t, token_stream_reader& ts)
	{
		auto file_node = t.create_node(fe::ext_ast::node_type::BLOCK);
		t.set_root_id(file_node);

		if (ts.peek().value == module_keyword) link_child_parent(parse_module_declaration(t, ts), file_node, t);

		if (ts.peek().value == import_keyword) link_child_parent(parse_module_imports(t, ts), file_node, t);

		while (ts.peek().value != lexing::end_of_input)
			link_child_parent(parse_stmt_semicln(t, ts), file_node, t);

		ts.consume(lexing::end_of_input);

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

	std::variant<tree, error> parse(token_stream_reader in)
	{
		tree t(fe::ext_ast::ast_allocation_hints{300150,0,0,100000,0,0,100000});

		try
		{
			parse_file(t, in);
			return t;
		}
		catch (error e)
		{
			return e;
		}
	}
}