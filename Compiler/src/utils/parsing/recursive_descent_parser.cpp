#include "utils/parsing/recursive_descent_parser.h"
#include "fe/language_definition.h"
#include <optional>

namespace recursive_descent
{
	class token_stream
	{
		const std::vector<utils::bnf::terminal_node>& tokens;
		std::vector<utils::bnf::terminal_node>::const_iterator curr;

	public:
		token_stream(const std::vector<utils::bnf::terminal_node>& tokens) :
			tokens(tokens), curr(tokens.begin()) {}

		const utils::bnf::terminal_node* peek()
		{
			assert(has_next());
			return &*curr;
		}

		const utils::bnf::terminal_node* peek_n(int n)
		{
			assert(curr + n != tokens.end());
			return &*(curr + n);
		}

		const utils::bnf::terminal_node* next()
		{
			assert(has_next());
			return &*curr++;
		}

		void consume(utils::bnf::terminal t)
		{
			assert((*next()).first == t);
		}

		bool has_next()
		{
			return curr != tokens.end();
		}
	};

	void link_child_parent(utils::bnf::tree::index child, utils::bnf::tree::index parent, tree& t)
	{
		t.get_node(child).parent = parent;
		t.get_non_terminal(t.get_node(parent).value_id).second.push_back(child);
	}

	using namespace fe::terminals;
	using namespace fe::non_terminals;

	utils::bnf::node_id parse_expression(tree& t, token_stream& ts);
	utils::bnf::node_id parse_operation(tree& t, token_stream& ts);
	utils::bnf::node_id parse_statement(tree& t, token_stream& ts);
	utils::bnf::node_id parse_type_operation(tree& t, token_stream& ts);

	utils::bnf::node_id parse_identifier(tree& t, token_stream& ts)
	{
		auto next = ts.next();
		assert(next->first == identifier);
		return t.create_terminal(*next);
	}

	utils::bnf::node_id parse_module_declaration(tree& t, token_stream& ts)
	{
		auto module_declaration_id = t.create_non_terminal({ module_declaration, {} });

		ts.consume(module_keyword);
		link_child_parent(parse_identifier(t, ts), module_declaration_id, t);

		return module_declaration_id;
	}

	utils::bnf::node_id parse_module_imports(tree& t, token_stream& ts)
	{
		auto module_imports_id = t.create_non_terminal({ module_imports, {} });

		ts.consume(import_keyword);
		ts.consume(left_square_bracket);
		while (ts.peek()->first == identifier)
			link_child_parent(t.create_terminal(*ts.next()), module_imports_id, t);
		ts.consume(right_square_bracket);

		return module_imports_id;
	}

	utils::bnf::node_id parse_reference_type(tree& t, token_stream& ts)
	{
		auto reference_type_id = t.create_non_terminal({ reference_type, {} });

		ts.consume(ref_keyword);
		link_child_parent(parse_type_operation(t, ts), reference_type_id, t);

		return reference_type_id;
	}

	utils::bnf::node_id parse_array_type(tree& t, token_stream& ts)
	{
		auto array_type_id = t.create_non_terminal({ array_type, {} });

		ts.consume(left_square_bracket);
		ts.consume(right_square_bracket);
		link_child_parent(parse_type_operation(t, ts), array_type_id, t);

		return array_type_id;
	}

	utils::bnf::node_id parse_type_tuple(tree& t, token_stream& ts)
	{
		auto type_tuple_id = t.create_non_terminal({ type_tuple, {} });

		ts.consume(left_bracket);
		link_child_parent(parse_type_operation(t, ts), type_tuple_id, t);
		while (ts.peek()->first == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_type_operation(t, ts), type_tuple_id, t);
		}
		ts.consume(right_bracket);

		return type_tuple_id;
	}

	utils::bnf::node_id parse_type_expression(tree& t, token_stream& ts)
	{
		auto next = ts.peek();

		if (next->first == identifier)
		{
			auto type_atom_id = t.create_non_terminal({ type_atom, {} });
			link_child_parent(parse_identifier(t, ts), type_atom_id, t);
			return type_atom_id;
		}
		else if (next->first == left_bracket) return parse_type_tuple(t, ts);
		else throw error{"Expected identifier or left_bracket"};
	}

	utils::bnf::node_id parse_type_operation(tree& t, token_stream& ts)
	{
		auto next = ts.peek();
		if      (next->first == ref_keyword)         return parse_reference_type(t, ts);
		else if (next->first == left_square_bracket) return parse_array_type(t, ts);
		else
		{
			auto expression = parse_type_expression(t, ts);
			next = ts.peek();
			if (next->first == right_arrow)
			{
				auto function_type_id = t.create_non_terminal({ function_type, {} });

				link_child_parent(expression, function_type_id, t);
				ts.consume(right_arrow);
				link_child_parent(parse_type_operation(t, ts), function_type_id, t);

				return function_type_id;
			}
			else return expression;
		}
	}

	utils::bnf::node_id parse_record_element(tree& t, token_stream& ts)
	{
		auto record_element_id = t.create_non_terminal({ record_element, {} });

		link_child_parent(parse_identifier(t, ts), record_element_id, t);
		ts.consume(colon);
		link_child_parent(parse_type_operation(t, ts), record_element_id, t);

		return record_element_id;
	}

	utils::bnf::node_id parse_record(tree& t, token_stream& ts)
	{
		auto record_id = t.create_non_terminal({ record, {} });

		ts.consume(left_bracket);
		link_child_parent(parse_record_element(t, ts), record_id, t);
		while (ts.peek()->first == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_record_element(t, ts), record_id, t);
		}
		ts.consume(left_bracket);

		return record_id;
	}

	utils::bnf::node_id parse_type_definition(tree& t, token_stream& ts)
	{
		auto type_definition_id = t.create_non_terminal({ type_definition, {} });

		ts.consume(type_keyword);
		link_child_parent(parse_identifier(t, ts), type_definition_id, t);
		link_child_parent(parse_record(t, ts), type_definition_id, t);

		return type_definition;
	}

	utils::bnf::node_id parse_identifier_tuple(tree& t, token_stream& ts)
	{
		auto identifier_tuple_id = t.create_non_terminal({ identifier_tuple, {} });

		ts.consume(left_bracket);
		link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		ts.consume(comma);
		link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		while (ts.peek()->first == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_identifier(t, ts), identifier_tuple_id, t);
		}
		ts.consume(right_bracket);

		return identifier_tuple_id;
	}

	utils::bnf::node_id parse_assignable(tree& t, token_stream& ts)
	{
		auto next = ts.peek();
		if      (next->first == identifier)   return parse_identifier(t, ts);
		else if (next->first == left_bracket) return parse_identifier_tuple(t, ts);
		else throw error{ "Expected identifier or left bracket" };
	}

	utils::bnf::node_id parse_declaration(tree& t, token_stream& ts)
	{
		auto declaration_id = t.create_non_terminal({ declaration, {} });

		ts.consume(let_keyword);
		link_child_parent(parse_assignable(t, ts), declaration_id, t);
		ts.consume(colon);
		link_child_parent(parse_type_operation(t, ts), declaration_id, t);
		ts.consume(equals);
		link_child_parent(parse_operation(t, ts), declaration_id, t);

		return declaration_id;
	}

	utils::bnf::node_id parse_assignment(tree& t, token_stream& ts)
	{
		auto assignment_id = t.create_non_terminal({ assignment, {} });

		link_child_parent(parse_identifier(t, ts), assignment_id, t);
		ts.consume(equals);
		link_child_parent(parse_operation(t, ts), assignment_id, t);

		return assignment_id;
	}

	std::vector<utils::bnf::node_id> parse_block_elements(tree& t, token_stream& ts)
	{
		std::vector<utils::bnf::node_id> res;

		res.push_back(parse_statement(t, ts));
		ts.consume(semicolon);

		while (ts.peek()->first != right_curly_bracket)
		{
			// Either result expression or statement
			auto first_of_element = ts.peek();
			auto second_of_element = ts.peek_n(1);
			auto element = parse_statement(t, ts);
			auto first_after_element = ts.peek();

			if (first_after_element->first == semicolon)
			{
				res.push_back(element);
				ts.consume(semicolon);
			}
			else if (first_after_element->first == right_curly_bracket)
			{
				if (first_of_element->first == type_keyword
					|| first_of_element->first == let_keyword
					|| first_of_element->first == while_keyword
					|| (first_of_element->first == identifier
						&& second_of_element->first == equals))
					throw error{ "Expected expression as block result, not a statement" };

				auto block_res = t.create_non_terminal({ block_result, {} });
				link_child_parent(element, block_res, t);
				res.push_back(block_res);
			}
			else
			{
				throw error{ "Expected semicolon or end of block" };
			}
		}

		return res;
	}

	utils::bnf::node_id parse_block(tree& t, token_stream& ts)
	{
		auto block_id = t.create_non_terminal({ block, {} });

		ts.consume(left_curly_bracket);
		auto elements = parse_block_elements(t, ts);
		for (auto element_id : elements) link_child_parent(element_id, block_id, t);
		ts.consume(right_curly_bracket);

		return block_id;
	}

	utils::bnf::node_id parse_while_loop(tree& t, token_stream& ts)
	{
		auto while_id = t.create_non_terminal({ while_loop, {} });

		ts.consume(while_keyword);
		ts.consume(left_bracket);
		link_child_parent(parse_operation(t, ts), while_id, t);
		ts.consume(right_bracket);
		link_child_parent(parse_block(t, ts), while_id, t);

		return while_id;
	}

	utils::bnf::node_id parse_word(tree& t, token_stream& ts)
	{
		auto next = ts.next();
		assert(next->first == word);
		return t.create_terminal(*next);
	}

	utils::bnf::node_id parse_array_value(tree& t, token_stream& ts)
	{
		auto array_value_id = t.create_non_terminal({ array_value, {} });
		ts.consume(left_square_bracket);
		link_child_parent(parse_operation(t, ts), array_value_id, t);

		while (ts.peek()->first == comma)
		{
			ts.consume(comma);
			link_child_parent(parse_operation(t, ts), array_value_id, t);
		}
		return array_value_id;
	}

	utils::bnf::node_id parse_number(tree& t, token_stream& ts)
	{
		auto next = ts.next();
		assert(next->first == number);
		return t.create_terminal(*next);
	}

	utils::bnf::node_id parse_true(tree& t, token_stream& ts)
	{
		auto next = ts.next();
		assert(next->first == true_keyword);
		return t.create_terminal(*next);
	}

	utils::bnf::node_id parse_false(tree& t, token_stream& ts)
	{
		auto next = ts.next();
		assert(next->first == false_keyword);
		return t.create_terminal(*next);
	}

	std::pair<utils::bnf::node_id, utils::bnf::node_id> parse_elseif_expr(tree& t, token_stream& ts)
	{
		ts.consume(elseif_keyword);
		ts.consume(left_bracket);
		auto test_id = parse_operation(t, ts);
		ts.consume(right_bracket);
		auto body_id = parse_block(t, ts);
		return std::make_pair(test_id, body_id);
	}

	utils::bnf::node_id parse_else_expr(tree& t, token_stream& ts)
	{
		ts.consume(else_keyword);
		return parse_block(t, ts);
	}

	utils::bnf::node_id parse_if_expr(tree& t, token_stream& ts)
	{
		auto if_expr_id = t.create_non_terminal({ if_expr, {} });

		ts.consume(if_keyword);
		ts.consume(left_bracket);
		link_child_parent(parse_operation(t, ts), if_expr_id, t);
		ts.consume(right_bracket);
		link_child_parent(parse_block(t, ts), if_expr_id, t);
		while(ts.peek()->first == elseif_keyword)
		{
			ts.consume(elseif_keyword);
			auto elseif = parse_elseif_expr(t, ts);
			link_child_parent(elseif.first, if_expr_id, t);
			link_child_parent(elseif.second, if_expr_id, t);
		}

		if (ts.peek()->first == else_keyword)
		{
			ts.consume(else_keyword);
			link_child_parent(parse_else_expr(t, ts), if_expr_id, t);
		}

		return if_expr_id;
	}

	utils::bnf::node_id parse_function(tree& t, token_stream& ts)
	{
		auto function_id = t.create_non_terminal({ function, {} });

		ts.consume(backslash);
		link_child_parent(parse_assignable(t, ts), function_id, t);
		ts.consume(fat_right_arrow);
		link_child_parent(parse_expression(t, ts), function_id, t);

		return function_id;
	}

	utils::bnf::node_id parse_expression(tree& t, token_stream& ts)
	{
		auto next = ts.peek()->first;
		if (next == word) return parse_word(t, ts);
		else if (next == left_bracket)
		{
			// () becomes empty value tuple
			ts.consume(left_bracket);
			if (ts.peek()->first == right_bracket)
			{
				ts.consume(right_bracket);
				return t.create_non_terminal({ value_tuple, {} });
			}

			auto op_id = parse_operation(t, ts);

			// ( op ) becomes brackets, not a tuple
			if (ts.peek()->first == right_bracket)
			{
				ts.consume(right_bracket);
				return op_id;
			}

			// ( op , op (, op)* ) becomes value tuple
			auto tuple_id = t.create_non_terminal({ value_tuple, {} });
			while (ts.peek()->first == comma)
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

	utils::bnf::node_id parse_function_call(tree& t, token_stream& ts)
	{
		if (ts.peek()->first != identifier)
			return parse_expression(t, ts);

		auto identifier_id = parse_identifier(t, ts);

		auto next = ts.peek()->first;
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
			auto function_call_id = t.create_non_terminal({ function_call, {} });

			link_child_parent(identifier_id, function_call_id, t);
			link_child_parent(parse_function_call(t, ts), function_call_id, t);
			return function_call_id;
		}

		return identifier_id;
	}

	utils::bnf::node_id parse_reference(tree& t, token_stream& ts)
	{
		if (ts.peek()->first != ref_keyword)
			return parse_function_call(t, ts);

		auto reference_id = t.create_non_terminal({ reference, {} });

		ts.consume(ref_keyword);
		link_child_parent(parse_reference(t, ts), reference_id, t);

		return reference_id;
	}

	utils::bnf::node_id parse_term(tree& t, token_stream& ts)
	{
		auto lhs_id = parse_reference(t, ts);

		if (ts.peek()->first == mul)
		{
			auto term_id = t.create_non_terminal({ multiplication, {} });

			link_child_parent(lhs_id, term_id, t);
			ts.consume(plus);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}
		else if (ts.peek()->first == fe::terminals::div)
		{
			auto term_id = t.create_non_terminal({ division, {} });

			link_child_parent(lhs_id, term_id, t);
			ts.consume(fe::terminals::div);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}
		else if (ts.peek()->first == percentage)
		{
			auto term_id = t.create_non_terminal({ modulo, {} });

			link_child_parent(lhs_id, term_id, t);
			ts.consume(percentage);
			link_child_parent(parse_term(t, ts), term_id, t);

			return term_id;
		}

		return lhs_id;
	}

	utils::bnf::node_id parse_arithmetic(tree& t, token_stream& ts)
	{
		auto lhs_id = parse_term(t, ts);

		if (ts.peek()->first == plus)
		{
			auto arithmetic_id = t.create_non_terminal({ addition, {} });

			link_child_parent(lhs_id, arithmetic_id, t);
			ts.consume(plus);
			link_child_parent(parse_arithmetic(t, ts), arithmetic_id, t);

			return arithmetic_id;
		}
		else if (ts.peek()->first == minus)
		{
			auto arithmetic_id = t.create_non_terminal({ subtraction, {} });

			link_child_parent(lhs_id, arithmetic_id, t);
			ts.consume(minus);
			link_child_parent(parse_arithmetic(t, ts), arithmetic_id, t);

			return arithmetic_id;
		}

		return lhs_id;
	}

	utils::bnf::node_id parse_comparison(tree& t, token_stream& ts)
	{
		auto lhs_id = parse_arithmetic(t, ts);

		if (ts.peek()->first == two_equals)
		{
			auto comparison_id = t.create_non_terminal({ equality, {} });

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(two_equals);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek()->first == lteq)
		{
			auto comparison_id = t.create_non_terminal({ less_or_equal, {} });

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(lteq);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek()->first == left_angle_bracket)
		{
			auto comparison_id = t.create_non_terminal({ less_than, {} });

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(left_angle_bracket);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek()->first == gteq)
		{
			auto comparison_id = t.create_non_terminal({ greater_or_equal, {} });

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(gteq);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}
		else if (ts.peek()->first == right_angle_bracket)
		{
			auto comparison_id = t.create_non_terminal({ greater_than, {} });

			link_child_parent(lhs_id, comparison_id, t);
			ts.consume(right_angle_bracket);
			link_child_parent(parse_comparison(t, ts), comparison_id, t);

			return comparison_id;
		}

		return lhs_id;
	}

	utils::bnf::node_id parse_logical(tree& t, token_stream& ts)
	{
		auto lhs_id = parse_comparison(t, ts);

		if (ts.peek()->first == and_keyword)
		{
			auto logical_id = t.create_non_terminal({ and_expr, {} });

			link_child_parent(lhs_id, logical_id, t);
			ts.consume(and_keyword);
			link_child_parent(parse_logical(t, ts), logical_id, t);

			return logical_id;
		}
		else if (ts.peek()->first == or_keyword)
		{
			auto logical_id = t.create_non_terminal({ or_expr, {} });

			link_child_parent(lhs_id, logical_id, t);
			ts.consume(or_keyword);
			link_child_parent(parse_logical(t, ts), logical_id, t);

			return logical_id;
		}

		return lhs_id;
	}

	utils::bnf::node_id parse_match_branch(tree& t, token_stream& ts)
	{
		auto branch_id = t.create_non_terminal({ match_branch, {} });

		ts.consume(vertical_line);
		link_child_parent(parse_operation(t, ts), branch_id, t);
		ts.consume(right_arrow);
		link_child_parent(parse_operation(t, ts), branch_id, t);

		return branch_id;
	}

	utils::bnf::node_id parse_match(tree& t, token_stream& ts)
	{
		auto logical_id = parse_logical(t, ts);
		if (ts.peek()->first != match_keyword)
			return logical_id;

		auto match_id = t.create_non_terminal({ match, {} });
		link_child_parent(logical_id, match_id, t);

		ts.consume(match_keyword);
		ts.consume(left_curly_bracket);
		while (ts.peek()->first == vertical_line)
		{
			link_child_parent(parse_match_branch(t, ts), match_id, t);
		}
		ts.consume(right_curly_bracket);

		return match_id;
	}

	utils::bnf::node_id parse_operation(tree& t, token_stream& ts)
	{
		return parse_match(t, ts);
	}

	utils::bnf::node_id parse_statement(tree& t, token_stream& ts)
	{
		auto next = ts.peek();
		if (next->first == type_keyword)       return parse_type_definition(t, ts);
		else if (next->first == let_keyword)   return parse_declaration(t, ts);
		else if (next->first == identifier 
			&& ts.peek_n(1)->first == equals)  return parse_assignment(t, ts);
		else if (next->first == while_keyword) return parse_while_loop(t, ts);
		else    /* operation */                return parse_operation(t, ts);
	}

	utils::bnf::node_id parse_stmt_semicln(tree& t, token_stream& ts)
	{
		auto stmt = parse_statement(t, ts);
		ts.consume(semicolon);
		return stmt;
	}

	utils::bnf::node_id parse_file(tree& t, token_stream& ts)
	{
		auto file_node = t.create_non_terminal({ file, {} });
		t.set_root(file_node);

		if (ts.peek()->first == module_keyword) link_child_parent(parse_module_declaration(t, ts), file_node, t);

		if (ts.peek()->first == import_keyword) link_child_parent(parse_module_imports(t, ts), file_node, t);

		while (ts.peek()->first != utils::bnf::end_of_input)
			link_child_parent(parse_stmt_semicln(t, ts), file_node, t);

		ts.consume(utils::bnf::end_of_input);
		
		return file_node;
	}

	void generate()
	{
		using namespace fe::terminals;
		using namespace fe::non_terminals;
		utils::bnf::non_terminal last_non_terminal = 1;
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

		utils::bnf::terminal last_terminal = 1;
		identifier = last_terminal++;
		equals = last_terminal++;
		left_bracket = last_terminal++;
		right_bracket = last_terminal++;
		number = last_terminal++;
		word = last_terminal++;
		type_keyword = last_terminal++;
		left_curly_bracket = last_terminal++;
		right_curly_bracket = last_terminal++;
		right_arrow = last_terminal++;
		comma = last_terminal++;
		left_square_bracket = last_terminal++;
		right_square_bracket = last_terminal++;
		match_keyword = last_terminal++;
		vertical_line = last_terminal++;
		module_keyword = last_terminal++;
		public_keyword = last_terminal++;
		ref_keyword = last_terminal++;
		let_keyword = last_terminal++;
		semicolon = last_terminal++;
		plus = last_terminal++;
		minus = last_terminal++;
		mul = last_terminal++;
		fe::terminals::div = last_terminal++;
		left_angle_bracket = last_terminal++;
		right_angle_bracket = last_terminal++;
		colon = last_terminal++;
		dot = last_terminal++;
		import_keyword = last_terminal++;
		while_keyword = last_terminal++;
		two_equals = last_terminal++;
		true_keyword = last_terminal++;
		false_keyword = last_terminal++;
		percentage = last_terminal++;
		lteq = last_terminal++;
		gteq = last_terminal++;
		if_keyword = last_terminal++;
		backslash = last_terminal++;
		fat_right_arrow = last_terminal++;
		else_keyword = last_terminal++;
		elseif_keyword = last_terminal++;
		and_keyword = last_terminal++;
		or_keyword = last_terminal++;
	}

	std::variant<tree, error> parse(const std::vector<utils::bnf::terminal_node>& in)
	{
		tree t;
		token_stream ts{ in };

		try
		{
			parse_file(t, ts);
			return t;
		}
		catch (error e)
		{
			return e;
		}
	}
}