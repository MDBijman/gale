#include "fe/pipeline/lowering_stage.h"
#include "fe/data/core_ast.h"
#include "fe/data/ext_ast.h"
#include "fe/data/types.h"
#include <tuple>
#include <unordered_map>

namespace fe::ext_ast
{
	// Helper

	enum class location_type
	{
		stack,
		reg,
		none
	};

	struct lowering_result
	{
		lowering_result(location_type l, int64_t p) : location(l)
		{
			if (l == location_type::stack)
				allocated_stack_space = p;
			else if (l == location_type::reg)
				result_register = static_cast<uint8_t>(p);
			else
				assert(!"Invalid location type");
		}
		lowering_result() : location(location_type::none) {}

		location_type location;
		uint8_t result_register;
		int64_t allocated_stack_space;
	};

	using variable_index = uint32_t;
	using label_index = uint32_t;
	using stack_label_index = uint32_t;

	struct function_context
	{
		variable_index next_idx = 0;
		uint32_t total_var_size = 0;
		uint32_t total_param_size = 0;

		std::unordered_map<variable_index,
				   std::pair<int /*offset from base*/, int /*size*/>>
		  var_positions;

		variable_index alloc_variable(uint32_t size)
		{
			auto var_id = next_idx++;
			var_positions.insert(
			  { var_id, { total_var_size + total_param_size, size } });
			total_var_size += size;
			return var_id;
		}

		variable_index alloc_param(uint32_t size)
		{
			auto var_id = next_idx++;
			var_positions.insert(
			  { var_id, { total_var_size + total_param_size, size } });
			total_param_size += size;
			return var_id;
		}

		stack_label_index next_sl_idx = 0;

		stack_label_index new_stack_label() { return next_sl_idx++; }
	};

	struct lowering_context
	{
		label_index next_label = 0;

		function_context curr_fn_context;

		variable_index alloc_variable(uint32_t size)
		{
			return curr_fn_context.alloc_variable(size);
		}
		variable_index alloc_param(uint32_t size)
		{
			return curr_fn_context.alloc_param(size);
		}
		variable_index get_offset(variable_index id)
		{
			return curr_fn_context.var_positions.at(id).first;
		}
		variable_index get_size(variable_index id)
		{
			return curr_fn_context.var_positions.at(id).second;
		}
		stack_label_index new_stack_label() { return curr_fn_context.new_stack_label(); }

		label_index new_label() { return next_label++; }
	};

	static bool has_children(node &n, ast &ast)
	{
		if (n.children_id == no_children) return false;
		return ast.get_children(n.children_id).size() > 0;
	}

	// Lowerers

	lowering_result lower_assignment(node_id p, node &n, ast &ext_ast, core_ast::ast &new_ast,
					 lowering_context &context)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto &children = ext_ast.children_of(n);
		assert(children.size() == 2);

		// a eval rhs -> result on stack
		// b get location of var
		// c move result on stack to var location
		// d dealloc from stack

		// a
		auto &value_node = ext_ast.get_node(children[1]);
		auto rhs = lower(p, value_node, ext_ast, new_ast, context);
		assert(rhs.location == location_type::stack);

		// b
		auto &id_node = ext_ast.get_node(children[0]);
		assert(id_node.kind == ext_ast::node_type::IDENTIFIER);
		auto &id_data = ext_ast.get_data<ext_ast::identifier>(
		  ext_ast
		    .get_node(
		      (*ext_ast.get_name_scope(id_node.name_scope_id)
			  .resolve_variable(ext_ast.get_data<identifier>(id_node.data_index).name,
					    ext_ast.name_scope_cb()))
			.declaration_node)
		    .data_index);

		auto var_id = id_data.index_in_function;
		auto is_param = id_data.is_parameter;

		// d
		auto pop = new_ast.create_node(core_ast::node_type::POP, p);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(pop).data_index).val =
		  context.get_size(var_id);

		auto to = new_ast.create_node(
		  is_param ? core_ast::node_type::PARAM : core_ast::node_type::VARIABLE, pop);
		new_ast.get_node_data<core_ast::var_data>(to) = { context.get_offset(var_id),
								  context.get_size(var_id) };

		return lowering_result();
	}

	lowering_result lower_tuple(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				    lowering_context &context)
	{
		assert(n.kind == node_type::TUPLE);
		auto &children = ast.children_of(n);
		auto tuple = new_ast.create_node(core_ast::node_type::TUPLE, p);

		int64_t stack_size = 0;
		for (auto child : children)
		{
			auto res = lower(tuple, ast.get_node(child), ast, new_ast, context);
			stack_size += res.allocated_stack_space;
		}

		return lowering_result(location_type::stack, stack_size);
	}

	lowering_result lower_block(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				    lowering_context &context)
	{
		assert(n.kind == node_type::BLOCK);
		auto block = new_ast.create_node(core_ast::node_type::BLOCK, p);
		new_ast.get_node(block).size = 0;
		auto &children = ast.children_of(n);

		for (auto it = children.begin(); it != children.end(); it++)
		{
			auto &n = ast.get_node(*it);
			auto res = lower(block, n, ast, new_ast, context);

			if (n.kind == node_type::BLOCK_RESULT)
			{
				assert(res.allocated_stack_space <= 8);
				assert(it == children.end() - 1);
				new_ast.get_node(block).size = res.allocated_stack_space;
			}
			else if (res.location == location_type::stack)
			{
				auto dealloc =
				  new_ast.create_node(core_ast::node_type::STACK_DEALLOC, block);
				new_ast
				  .get_data<core_ast::size>(*new_ast.get_node(dealloc).data_index)
				  .val = res.allocated_stack_space;
			}
		}

		return lowering_result(location_type::stack, *new_ast.get_node(block).size);
	}

	lowering_result lower_block_result(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					   lowering_context &context)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		auto &child = ast.get_node(children[0]);
		return lower(p, child, ast, new_ast, context);
	}

	lowering_result lower_function(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				       lowering_context &context)
	{
		assert(n.kind == node_type::FUNCTION);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto fn_before = context.curr_fn_context;
		context.curr_fn_context = function_context();

		auto function_id = new_ast.create_node(core_ast::node_type::FUNCTION, p);
		// Assignment of function_id data is done in lower_declaration, except local size

		// Parameters
		auto &param_node = ast.get_node(children[0]);
		size_t in_size = 0;
		if (param_node.kind == node_type::IDENTIFIER)
		{
			in_size =
			  (*ast.get_type_scope(param_node.type_scope_id)
			      .resolve_variable(ast.get_data<identifier>(param_node.data_index),
						ast.type_scope_cb()))
			    .type.calculate_size();
			auto var_id = context.alloc_param(in_size);
			ast.get_data<ext_ast::identifier>(param_node.data_index).index_in_function =
			  var_id;
			ast.get_data<ext_ast::identifier>(param_node.data_index).is_parameter =
			  true;
		}
		else if (param_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			for (auto &id : ast.get_children(param_node.children_id))
			{
				auto &id_node = ast.get_node(id);
				auto param_size = (*ast.get_type_scope(id_node.type_scope_id)
						      .resolve_variable(ast.get_data<identifier>(
									  id_node.data_index),
									ast.type_scope_cb()))
						    .type.calculate_size();
				auto var_id = context.alloc_param(param_size);
				ast.get_data<ext_ast::identifier>(id_node.data_index)
				  .index_in_function = var_id;
				ast.get_data<ext_ast::identifier>(id_node.data_index).is_parameter =
				  true;
				in_size += param_size;
			}
		}
		else
			throw std::runtime_error("Error: parameter node type invalid");

		auto ret = new_ast.create_node(core_ast::node_type::RET, function_id);
		new_ast.get_node_data<core_ast::return_data>(ret).in_size = in_size;

		auto block = new_ast.create_node(core_ast::node_type::BLOCK, ret);

		// Body
		auto &body_node = ast.get_node(children[1]);
		auto new_body = lower(block, body_node, ast, new_ast, context);

		assert(new_body.location == location_type::stack);
		new_ast.get_node(block).size = new_body.allocated_stack_space;
		new_ast.get_node_data<core_ast::return_data>(ret).out_size =
		  new_body.allocated_stack_space;

		new_ast.get_node_data<core_ast::function_data>(function_id).locals_size =
		  context.curr_fn_context.total_var_size;
		new_ast.get_node_data<core_ast::return_data>(ret).frame_size =
		  context.curr_fn_context.total_var_size + context.curr_fn_context.total_param_size;

		context.curr_fn_context = fn_before;

		// #todo functions should be root-scope only
		return lowering_result();
	}

	lowering_result lower_while_loop(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					 lowering_context &context)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto lbl_test_id = context.new_label();
		auto lbl_after_id = context.new_label();

		// Label before test
		auto lbl_test = new_ast.create_node(core_ast::node_type::LABEL, p);
		new_ast.get_node_data<core_ast::label>(lbl_test).id = lbl_test_id;

		// Generate test and jmp after
		lower(p, ast.get_node(children[0]), ast, new_ast, context);
		auto jmp = new_ast.create_node(core_ast::node_type::JZ, p);
		new_ast.get_node_data<core_ast::label>(jmp).id = lbl_after_id;

		// Generate body and jmp back
		lower(p, ast.get_node(children[1]), ast, new_ast, context);
		auto jmp_test = new_ast.create_node(core_ast::node_type::JMP, p);
		new_ast.get_node_data<core_ast::label>(jmp_test).id = lbl_test_id;

		// Label after
		auto lbl_after = new_ast.create_node(core_ast::node_type::LABEL, p);
		new_ast.get_node_data<core_ast::label>(lbl_after).id = lbl_after_id;

		return lowering_result();
	}

	lowering_result lower_if_statement(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					   lowering_context &context)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		auto &children = ast.children_of(n);
		assert(children.size() >= 2);

		bool has_else = children.size() % 2 == 1;

		auto after_lbl = context.new_label();

		int64_t size = 0;
		for (int i = 0; i < children.size() - 1; i += 2)
		{
			auto lbl = context.new_label();
			auto test_res = lower(p, ast.get_node(children[i]), ast, new_ast, context);

			// Does not count towards allocation size since JZ consumes the byte
			assert(test_res.allocated_stack_space == 1);

			auto jump = new_ast.create_node(core_ast::node_type::JZ, p);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(jump).data_index).id =
			  lbl;

			auto branch_block = new_ast.create_node(core_ast::node_type::BLOCK, p);
			auto body_res =
			  lower(branch_block, ast.get_node(children[i + 1]), ast, new_ast, context);
			new_ast.get_node(branch_block).size = body_res.allocated_stack_space;

			// Each branch must allocate the same amount of space
			if (i == 0)
				size = body_res.allocated_stack_space;
			else
				assert(body_res.allocated_stack_space == size);

			auto jump_end = new_ast.create_node(core_ast::node_type::JMP, p);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(jump_end).data_index)
			  .id = after_lbl;

			auto label = new_ast.create_node(core_ast::node_type::LABEL, p);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(label).data_index).id =
			  lbl;
		}

		if (has_else)
		{
			auto branch_block = new_ast.create_node(core_ast::node_type::BLOCK, p);
			auto body_res =
			  lower(branch_block, ast.get_node(children.back()), ast, new_ast, context);
			assert(body_res.allocated_stack_space == size);
			new_ast.get_node(branch_block).size = body_res.allocated_stack_space;
		}

		auto after_label = new_ast.create_node(core_ast::node_type::LABEL, p);
		new_ast.get_data<core_ast::label>(*new_ast.get_node(after_label).data_index).id =
		  after_lbl;

		return lowering_result(location_type::stack, size);
	}

	/*
	 * Always returns the size of the tested input.
	 * Returns a node id if the ast generated has a boolean result indicating a match.
	 * If false, no bool is left, and a match should be assumed.
	 * E.g. the pattern "| _ ->" does not require any testing.
	 */
	std::pair<std::optional<node_id>, int32_t>
	generate_pattern_test(node &n, ast &ast, core_ast::ast &new_ast, stack_label_index sl,
			      int32_t offset, types::type &curr_type, lowering_context &context)
	{
		switch (n.kind)
		{
		case node_type::FUNCTION_CALL:
		{
			auto &children = ast.get_children(n.children_id);
			auto &id_node = ast.get_node(children[0]);
			auto &id = ast.get_data<identifier>(id_node.data_index);

			auto eq = new_ast.create_node(core_ast::node_type::EQ);

			// Push dynamic tag bit onto the stack
			auto move = new_ast.create_node(core_ast::node_type::PUSH, eq);
			new_ast.get_node_data<core_ast::size>(move).val = 1;
			auto sd = new_ast.create_node(core_ast::node_type::RELATIVE_OFFSET, move);
			new_ast.get_node_data<core_ast::relative_offset>(
			  sd) = { sl, -1 }; // #fixme account for offset

			// Push static tag bit onto the stack
			auto tag = dynamic_cast<types::sum_type *>(&curr_type)->index_of(id.full);
			auto num = new_ast.create_node(core_ast::node_type::NUMBER, eq);
			new_ast.get_node_data<number>(num) = { static_cast<uint32_t>(tag),
							       number_type::UI8 };

			auto &exact_type = dynamic_cast<types::sum_type *>(&curr_type)->sum.at(tag);
			auto &inner = *dynamic_cast<types::nominal_type *>(exact_type.get())->inner;

			assert(children.size() == 2);
			auto param_test = generate_pattern_test(
			  ast.get_node(children[1]), ast, new_ast, sl, offset - 1, inner, context);
			if (param_test.first)
			{
				auto new_and = new_ast.create_node(core_ast::node_type::AND);
				new_ast.link_child_parent(eq, new_and);
				new_ast.link_child_parent(*param_test.first, new_and);
				return std::pair(new_and, 1 + param_test.second);
			}

			return std::pair(eq, 1 + param_test.second);
		}
		case node_type::IDENTIFIER:
		{
			auto &id = ast.get_data<identifier>(n.data_index);
			auto size = (*ast.get_type_scope(n.type_scope_id)
					.resolve_variable(id, ast.type_scope_cb()))
				      .type.calculate_size();
			id.referenced_stack_label = { sl, offset - size };
			return std::pair(std::nullopt, size);
		}
		case node_type::TUPLE:
		{
			auto &children = ast.get_children(n.children_id);
			auto &product = dynamic_cast<types::product_type *>(&curr_type)->product;
			if (children.size() == 0) return std::pair(std::nullopt, 0);

			std::optional<node_id> root;
			int32_t size_sum = 0;
			for (int i = 0; i < children.size(); i++)
			{
				auto pattern_test =
				  generate_pattern_test(ast.get_node(children[i]), ast, new_ast, sl,
							offset - size_sum, *product[i], context);
				size_sum += pattern_test.second;
				if (pattern_test.first)
				{
					if (!root) { root = pattern_test.first; }
					else
					{
						auto new_and =
						  new_ast.create_node(core_ast::node_type::AND);
						new_ast.link_child_parent(*root, new_and);
						new_ast.link_child_parent(*pattern_test.first,
									  new_and);
						root = new_and;
					}
				}
			}
			return std::pair(root, size_sum);
			break;
		}
		case node_type::NUMBER:
		{
			auto &num = ast.get_data<number>(n.data_index);

			uint8_t num_byte_size = number_size(num.type);

			auto eq = new_ast.create_node(core_ast::node_type::EQ);

			// Push dynamic tag bit onto the stack
			auto move = new_ast.create_node(core_ast::node_type::PUSH, eq);
			new_ast.get_node_data<core_ast::size>(move).val = num_byte_size;
			auto sd = new_ast.create_node(core_ast::node_type::RELATIVE_OFFSET, move);
			new_ast.get_node_data<core_ast::relative_offset>(
			  sd) = { sl, offset - num_byte_size };

			// Create number
			auto new_num = new_ast.create_node(core_ast::node_type::NUMBER, eq);
			new_ast.get_node_data<number>(new_num) = { num.value, num.type };

			return std::pair(eq, num_byte_size);
			break;
		}
		case node_type::BOOLEAN:
		{
			auto &bool_data = ast.get_data<boolean>(n.data_index);
			int32_t size = 1;

			auto eq = new_ast.create_node(core_ast::node_type::EQ);

			// Push dynamic tag bit onto the stack
			auto move = new_ast.create_node(core_ast::node_type::PUSH, eq);
			new_ast.get_node_data<core_ast::size>(move).val = size;
			auto sd = new_ast.create_node(core_ast::node_type::RELATIVE_OFFSET, move);
			new_ast.get_node_data<core_ast::relative_offset>(sd) = { sl,
										 offset - size };

			// Create number
			auto new_bool = new_ast.create_node(core_ast::node_type::BOOLEAN, eq);
			new_ast.get_node_data<boolean>(new_bool) = { bool_data.value };

			return std::pair(eq, size);
			break;
		}
		default: throw std::runtime_error("Invalid pattern"); break;
		}
	}

	lowering_result lower_match(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				    lowering_context &context)
	{
		assert(n.kind == node_type::MATCH);
		auto &children = ast.children_of(n);
		assert(children.size() >= 2);

		// Create stack label for bindings within patterns
		auto stack_lbl = new_ast.create_node(core_ast::node_type::STACK_LABEL, p);
		auto stack_lbl_idx = context.new_stack_label();
		new_ast.get_node_data<core_ast::stack_label>(stack_lbl).id = stack_lbl_idx;

		// Lower subject
		auto expression_node = ast.get_node(children[0]);
		assert(expression_node.kind == node_type::IDENTIFIER);
		lower(p, expression_node, ast, new_ast, context);

		auto &expression_type =
		  ast.get_type_scope(expression_node.type_scope_id)
		    .resolve_variable(ast.get_data<identifier>(expression_node.data_index),
				      ast.type_scope_cb())
		    ->type;
		auto &sum_type =
		  *dynamic_cast<types::product_type *>(&expression_type)->product[1].get();
		auto expression_size = expression_type.calculate_size();

		auto lbl_after = context.new_label();

		// Lower branches
		for (auto i = 1; i < children.size(); i++)
		{
			auto branch = ast.get_node(children[i]);
			assert(branch.kind == node_type::MATCH_BRANCH);
			auto &branch_children = ast.get_children(branch.children_id);
			assert(branch_children.size() == 2);

			auto lbl_false_test = context.new_label();

			auto &pattern = ast.get_node(branch_children[0]);
			auto pat_id = generate_pattern_test(pattern, ast, new_ast, stack_lbl_idx, 0,
							    sum_type, context);
			assert(pat_id.first);
			new_ast.link_child_parent(*pat_id.first, p);

			auto jump = new_ast.create_node(core_ast::node_type::JZ, p);
			new_ast.get_node_data<core_ast::label>(jump).id = lbl_false_test;

			auto &body = ast.get_node(branch_children[1]);
			lower(p, body, ast, new_ast, context);

			auto skip_rest = new_ast.create_node(core_ast::node_type::JMP, p);
			new_ast.get_node_data<core_ast::label>(skip_rest).id = lbl_after;

			auto lbl_node = new_ast.create_node(core_ast::node_type::LABEL, p);
			new_ast.get_node_data<core_ast::label>(lbl_node).id = lbl_false_test;
		}

		auto after_node = new_ast.create_node(core_ast::node_type::LABEL, p);
		new_ast.get_node_data<core_ast::label>(after_node).id = lbl_after;

		auto dealloc = new_ast.create_node(core_ast::node_type::STACK_DEALLOC, p);
		new_ast.get_node_data<core_ast::size>(dealloc).val = expression_size;

		return lowering_result();
	}

	lowering_result lower_id(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				 lowering_context &context)
	{
		// Assumes variable in current stack frame

		assert(n.kind == node_type::IDENTIFIER);
		assert(!has_children(n, ast));

		auto &id = ast.get_data<identifier>(n.data_index);
		auto size =
		  (*ast.get_type_scope(n.type_scope_id).resolve_variable(id, ast.type_scope_cb()))
		    .type.calculate_size();
		auto &id_data = ast.get_data<identifier>(
		  ast
		    .get_node((*ast.get_name_scope(n.name_scope_id)
				  .resolve_variable(id, ast.name_scope_cb()))
				.declaration_node)
		    .data_index);

		auto read = new_ast.create_node(core_ast::node_type::PUSH, p);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(read).data_index).val = size;

		if (id_data.referenced_stack_label.has_value())
		{
			// Resolves source address
			auto param_ref =
			  new_ast.create_node(core_ast::node_type::RELATIVE_OFFSET, read);
			new_ast.get_node_data<core_ast::relative_offset>(
			  param_ref) = { id_data.referenced_stack_label->first,
					 id_data.referenced_stack_label->second };
		}
		else
		{
			// Resolves source address
			auto param_ref =
			  new_ast.create_node(id_data.is_parameter ? core_ast::node_type::PARAM
								   : core_ast::node_type::VARIABLE,
					      read);
			new_ast.get_node_data<core_ast::var_data>(
			  param_ref) = { context.get_offset(id_data.index_in_function),
					 context.get_size(id_data.index_in_function) };
		}

		return lowering_result(location_type::stack, size);
	}

	lowering_result lower_string(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				     lowering_context &context)
	{
		assert(n.kind == node_type::STRING);
		assert(!has_children(n, ast));
		auto &str_data = ast.get_data<string>(n.data_index);

		// #todo do something?
		auto str = new_ast.create_node(core_ast::node_type::STRING, p);
		auto &str_node = new_ast.get_node(str);
		new_ast.get_data<string>(*str_node.data_index) = str_data;

		assert(!"nyi");
		return lowering_result();
	}

	lowering_result lower_boolean(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				      lowering_context &context)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(!has_children(n, ast));
		auto &bool_data = ast.get_data<boolean>(n.data_index);

		auto bool_id = new_ast.create_node(core_ast::node_type::BOOLEAN, p);
		auto &bool_node = new_ast.get_node(bool_id);
		new_ast.get_data<boolean>(*bool_node.data_index) = bool_data;

		return lowering_result(location_type::stack, 1);
	}

	lowering_result lower_number(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				     lowering_context &context)
	{
		assert(n.kind == node_type::NUMBER);
		assert(!has_children(n, ast));
		auto &num_data = ast.get_data<number>(n.data_index);

		auto num_id = new_ast.create_node(core_ast::node_type::NUMBER, p);
		auto &num_node = new_ast.get_node(num_id);
		new_ast.get_data<number>(*num_node.data_index) = num_data;

		if (num_data.type == number_type::I8 || num_data.type == number_type::UI8)
			return lowering_result(location_type::stack, 1);
		else if (num_data.type == number_type::I16 || num_data.type == number_type::UI16)
			return lowering_result(location_type::stack, 2);
		else if (num_data.type == number_type::I32 || num_data.type == number_type::UI32)
			return lowering_result(location_type::stack, 4);
		else if (num_data.type == number_type::I64 || num_data.type == number_type::UI64)
			return lowering_result(location_type::stack, 8);

		throw std::runtime_error("Unknown number type");
	}

	lowering_result lower_function_call(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					    lowering_context &context)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto fun_id = new_ast.create_node(core_ast::node_type::FUNCTION_CALL, p);

		auto &name = ast.get_data<identifier>(ast.get_node(children[0]).data_index);

		auto &function_type = ast.get_type_scope(n.type_scope_id)
					.resolve_variable(name, ast.type_scope_cb())
					->type;

		auto output_size = function_type.calculate_size();
		auto input_size =
		  dynamic_cast<types::function_type *>(&function_type)->from->calculate_size();

		new_ast.get_node_data<core_ast::function_call_data>(fun_id) =
		  core_ast::function_call_data(name.full, input_size, output_size);

		new_ast.get_node_data<core_ast::function_call_data>(fun_id).out_size = output_size;

		auto param = lower(fun_id, ast.get_node(children[1]), ast, new_ast, context);

		new_ast.get_node_data<core_ast::function_call_data>(fun_id).in_size =
		  param.allocated_stack_space;

		new_ast.get_node(fun_id).size = output_size;

		return lowering_result(location_type::stack, output_size);
	}

	lowering_result lower_array_access(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					   lowering_context &context)
	{
		assert(n.kind == node_type::ARRAY_ACCESS);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		// Literal of element size
		auto &id_node = ast.get_node(children[0]);
		auto type = ast.get_type_scope(id_node.type_scope_id)
			      .resolve_variable(ast.get_data<identifier>(id_node.data_index),
						ast.type_scope_cb());
		auto as_array = dynamic_cast<types::array_type *>(&type->type);
		size_t element_size = as_array->element_type->calculate_size();

		assert(id_node.kind == ext_ast::node_type::IDENTIFIER);
		auto &id_data = ast.get_data<ext_ast::identifier>(
		  ast
		    .get_node(
		      (*ast.get_name_scope(id_node.name_scope_id)
			  .resolve_variable(ast.get_data<identifier>(id_node.data_index).name,
					    ast.name_scope_cb()))
			.declaration_node)
		    .data_index);

		auto var_id = id_data.index_in_function;
		auto is_param = id_data.is_parameter;

		// Multiply index with element size to get actual location
		auto mul_node = new_ast.create_node(core_ast::node_type::MUL, p);
		// Index
		auto idx_node = lower(mul_node, ast.get_node(children[1]), ast, new_ast, context);
		auto num_node = new_ast.create_node(core_ast::node_type::NUMBER, mul_node);
		new_ast.get_node_data<number>(num_node) = { static_cast<int64_t>(element_size),
							    number_type::UI64 };

		auto move = new_ast.create_node(core_ast::node_type::PUSH, p);
		new_ast.get_node_data<core_ast::size>(move).val = element_size;

		auto from = new_ast.create_node(is_param ? core_ast::node_type::DYNAMIC_PARAM
							 : core_ast::node_type::DYNAMIC_VARIABLE,
						move);
		new_ast.get_node_data<core_ast::var_data>(from) = { context.get_offset(var_id),
								    context.get_size(var_id) };

		return lowering_result(location_type::stack, element_size);
	}

	lowering_result lower_module_declaration(node_id p, node &n, ast &ast,
						 core_ast::ast &new_ast, lowering_context &context)
	{
		assert(n.kind == node_type::MODULE_DECLARATION);
		assert(ast.children_of(n).size() == 1);
		return lowering_result();
	}

	lowering_result lower_import_declaration(node_id p, node &n, ast &ast,
						 core_ast::ast &new_ast, lowering_context &context)
	{
		assert(n.kind == node_type::IMPORT_DECLARATION);
		return lowering_result();
	}

	lowering_result lower_export_stmt(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					  lowering_context &context)
	{
		assert(n.kind == node_type::EXPORT_STMT);
		return lowering_result();
	}

	lowering_result lower_declaration(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					  lowering_context &context)
	{
		assert(n.kind == node_type::DECLARATION);
		auto &children = ast.children_of(n);
		assert(children.size() == 3);

		auto &lhs_node = ast.get_node(children[0]);

		auto &type_node = ast.get_node(children[1]);
		lower(p, type_node, ast, new_ast, context);

		auto &rhs_node = ast.get_node(children[2]);
		auto rhs = lower(p, rhs_node, ast, new_ast, context);

		// Function declaration
		if (lhs_node.kind == node_type::IDENTIFIER && rhs_node.kind == node_type::FUNCTION)
		{
			// Put location in register
			auto &identifier_data =
			  ast.get_data<ext_ast::identifier>(lhs_node.data_index);
			auto &function_data = new_ast.get_data<core_ast::function_data>(
			  *new_ast.get_node(new_ast.get_node(p).children.back()).data_index);
			function_data.name = identifier_data.full;
			types::function_type *func_type = dynamic_cast<types::function_type *>(
			  &(*ast.get_type_scope(n.type_scope_id)
			       .resolve_variable(identifier_data, ast.type_scope_cb()))
			     .type);
			function_data.in_size = func_type->from->calculate_size();
			function_data.out_size = func_type->to->calculate_size();

			return lowering_result();
		}
		// Variable declaration
		else if (lhs_node.kind == node_type::IDENTIFIER)
		{
			auto &identifier_data =
			  ast.get_data<ext_ast::identifier>(lhs_node.data_index);
			uint32_t variable_size = static_cast<uint32_t>(rhs.allocated_stack_space);
			auto var_id = context.alloc_variable(variable_size);
			identifier_data.index_in_function = var_id;

			// Pop value into result variable
			auto pop = new_ast.create_node(core_ast::node_type::POP, p);
			new_ast.get_node_data<core_ast::size>(pop).val = variable_size;
			auto r = new_ast.create_node(core_ast::node_type::VARIABLE, pop);
			new_ast.get_node_data<core_ast::var_data>(r) = { context.get_offset(var_id),
									 variable_size };

			return lowering_result();
		}
		// Tuple declaration
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			// Tuple declarations are implemented by declaring n seperate variables and
			// popping each individually off the stack. Popping happens from
			// right-to-left since the value of the rightmost id/variable is at the top
			// of the stack.

			auto &ids = ast.children_of(lhs_node);
			std::vector<variable_index> var_idxs;
			for (auto it = ids.begin(); it != ids.end(); it++)
			{
				auto &node = ast.get_node(*it);
				auto &id = ast.get_data<identifier>(node.data_index);
				assert(node.kind == node_type::IDENTIFIER);
				auto &identifier_data =
				  ast.get_data<ext_ast::identifier>(ast.get_node(*it).data_index);

				auto size = ast.get_type_scope(node.type_scope_id)
					      .resolve_variable(id, ast.type_scope_cb())
					      ->type.calculate_size();

				auto var_idx = context.alloc_variable(size);
				identifier_data.index_in_function = var_idx;
				var_idxs.push_back(var_idx);
			}

			for (auto it = var_idxs.rbegin(); it != var_idxs.rend(); it++)
			{
				// Pop value into result variable
				auto pop = new_ast.create_node(core_ast::node_type::POP, p);
				new_ast.get_node_data<core_ast::size>(pop).val =
				  context.get_size(*it);
				auto r = new_ast.create_node(core_ast::node_type::VARIABLE, pop);
				new_ast.get_node_data<core_ast::var_data>(
				  r) = { context.get_offset(*it), context.get_size(*it) };
			}

			return lowering_result();
		}

		throw std::runtime_error("error");
	}

	lowering_result lower_type_definition(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					      lowering_context &context)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto &name_id = ast.get_node(children[0]);
		auto &name_data = ast.get_data<identifier>(name_id.data_index);
		auto size = (*ast.get_type_scope(name_id.type_scope_id)
				.resolve_variable(name_data, ast.type_scope_cb()))
			      .type.calculate_size();

		// #todo replace current fn context!

		auto fn = new_ast.create_node(core_ast::node_type::FUNCTION, p);
		auto &fn_data =
		  new_ast.get_data<core_ast::function_data>(*new_ast.get_node(fn).data_index);
		fn_data.in_size = size;
		fn_data.out_size = size;
		fn_data.name = name_data.name;
		assert(size <= 8);

		auto input_var = context.alloc_variable(size);

		auto ret = new_ast.create_node(core_ast::node_type::RET, fn);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(ret).data_index).val = size;
		new_ast.get_node(ret).size = size;

		{
			auto move = new_ast.create_node(core_ast::node_type::PUSH, ret);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(move).data_index).val =
			  size;

			// Resolve source (param at offset 0)
			auto from = new_ast.create_node(core_ast::node_type::VARIABLE, move);
			new_ast.get_node_data<core_ast::var_data>(
			  from) = { context.get_offset(input_var), context.get_size(input_var) };
		}

		return lowering_result();
	}

	lowering_result lower_type_atom(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					lowering_context &context)
	{
		assert(n.kind == node_type::ATOM_TYPE);
		return lowering_result();
	}

	lowering_result lower_sum_type(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				       lowering_context &context)
	{
		assert(n.kind == node_type::SUM_TYPE);
		auto &children = ast.children_of(n);

		auto fn_context = context.curr_fn_context;

		for (int i = 0; i < children.size(); i += 2)
		{
			auto &node = ast.get_node(children[i]);
			identifier &id = ast.get_data<identifier>(node.data_index);

			auto lookup = ast.get_type_scope(n.type_scope_id)
					.resolve_variable(id, ast.type_scope_cb());

			auto in_size =
			  dynamic_cast<types::sum_type *>(
			    dynamic_cast<types::product_type *>(
			      dynamic_cast<types::function_type *>(&lookup->type)->to.get())
			      ->product.at(1)
			      .get())
			    ->sum.at(i / 2)
			    ->calculate_size();
			auto out_size = lookup->type.calculate_size();

			// Create function

			auto fn = new_ast.create_node(core_ast::node_type::FUNCTION, p);
			auto &fn_data = new_ast.get_data<core_ast::function_data>(
			  *new_ast.get_node(fn).data_index);
			fn_data.in_size = in_size;
			fn_data.out_size = in_size + 1; /*output is written to space allocated by caller*/
			fn_data.name = id.name;
			fn_data.locals_size = 0;
			context.curr_fn_context = function_context();

			auto input_var = context.alloc_param(in_size);

			auto ret = new_ast.create_node(core_ast::node_type::RET, fn);
			new_ast.get_data<core_ast::return_data>(
			  *new_ast.get_node(ret).data_index) = { in_size, 1 + in_size,
								 1 + in_size };
			auto block = new_ast.create_node(core_ast::node_type::BLOCK, ret);

			{
				auto tag = new_ast.create_node(core_ast::node_type::NUMBER, block);
				new_ast.get_node_data<number>(tag) = { i / 2, number_type::UI8 };

				auto move = new_ast.create_node(core_ast::node_type::PUSH, block);
				new_ast.get_node_data<core_ast::size>(move).val = in_size;
				// Resolve source (param at offset 0)
				auto from = new_ast.create_node(core_ast::node_type::PARAM, move);
				new_ast.get_node_data<core_ast::var_data>(from) = {
					context.get_offset(input_var), context.get_size(input_var)
				};
			}
		}

		context.curr_fn_context = fn_context;

		return lowering_result();
	}

	lowering_result lower_reference(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					lowering_context &context)
	{
		assert(n.kind == node_type::REFERENCE);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);

		assert(!"nyi");
		return lowering_result();
	}

	lowering_result lower_array_value(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					  lowering_context &context)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(n.data_index == no_data);

		auto arr = new_ast.create_node(core_ast::node_type::TUPLE, p);
		auto &children = ast.children_of(n);
		int64_t size_sum = 0;
		for (auto child : children)
		{
			auto new_child = lower(arr, ast.get_node(child), ast, new_ast, context);
			size_sum += new_child.allocated_stack_space;
		}

		return lowering_result(location_type::stack, size_sum);
	}

	lowering_result lower_binary_op(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
					lowering_context &context)
	{
		assert(ext_ast::is_binary_op(n.kind));
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		core_ast::node_type new_node_type;
		// By default the number of stack bytes is the largest of the two operands
		// Boolean operators however reduce this to a single byte
		int32_t stack_bytes = 0;
		switch (n.kind)
		{
		case node_type::ADDITION: new_node_type = core_ast::node_type::ADD; break;
		case node_type::SUBTRACTION: new_node_type = core_ast::node_type::SUB; break;
		case node_type::MULTIPLICATION: new_node_type = core_ast::node_type::MUL; break;
		case node_type::DIVISION: new_node_type = core_ast::node_type::DIV; break;
		case node_type::MODULO: new_node_type = core_ast::node_type::MOD; break;
		case node_type::EQUALITY:
			new_node_type = core_ast::node_type::EQ;
			stack_bytes = 1;
			break;
		case node_type::GREATER_OR_EQ:
			new_node_type = core_ast::node_type::GEQ;
			stack_bytes = 1;
			break;
		case node_type::GREATER_THAN:
			new_node_type = core_ast::node_type::GT;
			stack_bytes = 1;
			break;
		case node_type::LESS_OR_EQ:
			new_node_type = core_ast::node_type::LEQ;
			stack_bytes = 1;
			break;
		case node_type::LESS_THAN:
			new_node_type = core_ast::node_type::LT;
			stack_bytes = 1;
			break;
		case node_type::AND:
			new_node_type = core_ast::node_type::AND;
			stack_bytes = 1;
			break;
		case node_type::OR:
			new_node_type = core_ast::node_type::OR;
			stack_bytes = 1;
			break;
		default: throw lower_error{ "Unknown binary op" };
		}

		auto new_node = new_ast.create_node(new_node_type, p);

		// #todo or short circuiting

		// AND short circuiting
		if (n.kind == node_type::AND)
		{
			auto short_circuit_lbl = context.new_label();
			auto finish_lbl = context.new_label();

			// Generate LHS expression
			auto lhs_block = new_ast.create_node(core_ast::node_type::BLOCK, new_node);
			auto lhs =
			  lower(lhs_block, ast.get_node(children[0]), ast, new_ast, context);

			// If the result was false, jump over the RHS expression and push a 0
			auto jump = new_ast.create_node(core_ast::node_type::JZ, lhs_block);
			new_ast.get_node_data<core_ast::label>(jump).id = short_circuit_lbl;
			auto push_true =
			  new_ast.create_node(core_ast::node_type::BOOLEAN, lhs_block);
			new_ast.get_node_data<boolean>(push_true).value = true;

			// If the result was true, evaluate RHS and && results together and jump to
			// finish
			auto rhs_block = new_ast.create_node(core_ast::node_type::BLOCK, new_node);
			auto rhs =
			  lower(rhs_block, ast.get_node(children[1]), ast, new_ast, context);
			auto jump_finish = new_ast.create_node(core_ast::node_type::JMP, rhs_block);
			new_ast.get_node_data<core_ast::label>(jump_finish).id = finish_lbl;

			// If we short circuited, we push a 0 since it was consumed by the jump
			auto jump_target =
			  new_ast.create_node(core_ast::node_type::LABEL, rhs_block);
			new_ast.get_node_data<core_ast::label>(jump_target).id = short_circuit_lbl;
			// Push false twice since the && is generated in bytecode later, this is
			// slightly inefficient
			auto push_false =
			  new_ast.create_node(core_ast::node_type::BOOLEAN, rhs_block);
			new_ast.get_node_data<boolean>(push_false).value = false;
			auto push_false_again =
			  new_ast.create_node(core_ast::node_type::BOOLEAN, rhs_block);
			new_ast.get_node_data<boolean>(push_false_again).value = false;

			auto finish_target =
			  new_ast.create_node(core_ast::node_type::LABEL, rhs_block);
			new_ast.get_node_data<core_ast::label>(finish_target).id = finish_lbl;
		}
		// OR short circuiting
		else if (n.kind == node_type::OR)
		{
			auto short_circuit_lbl = context.new_label();
			auto finish_lbl = context.new_label();

			// Generate LHS expression
			auto lhs_block = new_ast.create_node(core_ast::node_type::BLOCK, new_node);
			auto lhs =
			  lower(lhs_block, ast.get_node(children[0]), ast, new_ast, context);

			// If the result was true, jump over the RHS expression and push a 1
			auto jump = new_ast.create_node(core_ast::node_type::JNZ, lhs_block);
			new_ast.get_node_data<core_ast::label>(jump).id = short_circuit_lbl;
			auto push_true =
			  new_ast.create_node(core_ast::node_type::BOOLEAN, lhs_block);
			new_ast.get_node_data<boolean>(push_true).value = false;

			// If the result was false, evaluate RHS and || results together and jump to
			// finish
			auto rhs_block = new_ast.create_node(core_ast::node_type::BLOCK, new_node);
			auto rhs =
			  lower(rhs_block, ast.get_node(children[1]), ast, new_ast, context);
			auto jump_finish = new_ast.create_node(core_ast::node_type::JMP, rhs_block);
			new_ast.get_node_data<core_ast::label>(jump_finish).id = finish_lbl;

			// If we short circuited, we push a 1 since it was consumed by the jump
			auto jump_target =
			  new_ast.create_node(core_ast::node_type::LABEL, rhs_block);
			new_ast.get_node_data<core_ast::label>(jump_target).id = short_circuit_lbl;
			// Push true twice since the || is generated in bytecode later, this is
			// slightly inefficient
			auto push_false =
			  new_ast.create_node(core_ast::node_type::BOOLEAN, rhs_block);
			new_ast.get_node_data<boolean>(push_false).value = true;
			auto push_false_again =
			  new_ast.create_node(core_ast::node_type::BOOLEAN, rhs_block);
			new_ast.get_node_data<boolean>(push_false_again).value = true;

			auto finish_target =
			  new_ast.create_node(core_ast::node_type::LABEL, rhs_block);
			new_ast.get_node_data<core_ast::label>(finish_target).id = finish_lbl;
		}
		else
		{
			auto lhs_block = new_ast.create_node(core_ast::node_type::BLOCK, new_node);
			auto lhs =
			  lower(lhs_block, ast.get_node(children[0]), ast, new_ast, context);
			auto rhs_block = new_ast.create_node(core_ast::node_type::BLOCK, new_node);
			auto rhs =
			  lower(rhs_block, ast.get_node(children[1]), ast, new_ast, context);

			// If we hadnt set stack_bytes to 1, then we have a number operator
			if (stack_bytes == 0)
			{
				stack_bytes = lhs.allocated_stack_space > rhs.allocated_stack_space
						? lhs.allocated_stack_space
						: rhs.allocated_stack_space;
			}
		}
		return lowering_result(location_type::stack, stack_bytes);
	}

	lowering_result lower_unary_op(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
				       lowering_context &context)
	{
		assert(ext_ast::is_unary_op(n.kind));
		auto &children = ast.children_of(n);
		assert(children.size() == 1);

		assert(n.kind == ext_ast::node_type::NOT);

		core_ast::node_type new_node_type = core_ast::node_type::NOT;
		// Can only apply to boolean result, so a single byte
		int32_t stack_bytes = 1;

		auto new_node = new_ast.create_node(new_node_type, p);
		auto child_block = new_ast.create_node(core_ast::node_type::BLOCK, new_node);
		auto child = lower(child_block, ast.get_node(children[0]), ast, new_ast, context);

		return lowering_result(location_type::stack, stack_bytes);
	}

	lowering_result lower(node_id p, node &n, ast &ast, core_ast::ast &new_ast,
			      lowering_context &context)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT:
			return lower_assignment(p, n, ast, new_ast, context);
			break;
		case node_type::TUPLE: return lower_tuple(p, n, ast, new_ast, context); break;
		case node_type::BLOCK: return lower_block(p, n, ast, new_ast, context); break;
		case node_type::BLOCK_RESULT:
			return lower_block_result(p, n, ast, new_ast, context);
			break;
		case node_type::FUNCTION: return lower_function(p, n, ast, new_ast, context); break;
		case node_type::WHILE_LOOP:
			return lower_while_loop(p, n, ast, new_ast, context);
			break;
		case node_type::IF_STATEMENT:
			return lower_if_statement(p, n, ast, new_ast, context);
			break;
		case node_type::MATCH: return lower_match(p, n, ast, new_ast, context); break;
		case node_type::IDENTIFIER: return lower_id(p, n, ast, new_ast, context); break;
		case node_type::STRING: return lower_string(p, n, ast, new_ast, context); break;
		case node_type::BOOLEAN: return lower_boolean(p, n, ast, new_ast, context); break;
		case node_type::NUMBER: return lower_number(p, n, ast, new_ast, context); break;
		case node_type::FUNCTION_CALL:
			return lower_function_call(p, n, ast, new_ast, context);
			break;
		case node_type::ARRAY_ACCESS:
			return lower_array_access(p, n, ast, new_ast, context);
			break;
		case node_type::MODULE_DECLARATION:
			return lower_module_declaration(p, n, ast, new_ast, context);
			break;
		case node_type::IMPORT_DECLARATION:
			return lower_import_declaration(p, n, ast, new_ast, context);
			break;
		case node_type::EXPORT_STMT:
			return lower_export_stmt(p, n, ast, new_ast, context);
			break;
		case node_type::DECLARATION:
			return lower_declaration(p, n, ast, new_ast, context);
			break;
		case node_type::TYPE_DEFINITION:
			return lower_type_definition(p, n, ast, new_ast, context);
			break;
		case node_type::ATOM_TYPE:
			return lower_type_atom(p, n, ast, new_ast, context);
			break;
		case node_type::SUM_TYPE: return lower_sum_type(p, n, ast, new_ast, context); break;
		case node_type::REFERENCE:
			return lower_reference(p, n, ast, new_ast, context);
			break;
		case node_type::ARRAY_VALUE:
			return lower_array_value(p, n, ast, new_ast, context);
			break;
		default:
			if (ext_ast::is_binary_op(n.kind))
				return lower_binary_op(p, n, ast, new_ast, context);
			if (ext_ast::is_unary_op(n.kind))
				return lower_unary_op(p, n, ast, new_ast, context);
			if (ext_ast::is_type_node(n.kind)) return lowering_result();

			assert(!"Node type not lowerable");
			throw std::runtime_error("Fatal Error - Node type not lowerable");
		}
	}

	core_ast::ast lower(ast &ast)
	{
		auto &root = ast.get_node(ast.root_id());
		assert(root.kind == node_type::BLOCK);
		auto &children = ast.children_of(root);

		lowering_context context;
		core_ast::ast new_ast(core_ast::node_type::BLOCK);
		auto root_block = new_ast.root_id();

		auto bootstrap =
		  new_ast.create_node(core_ast::node_type::FUNCTION_CALL, root_block);
		new_ast.get_node_data<core_ast::function_call_data>(bootstrap) =
		  core_ast::function_call_data("main", 0, 0);
		new_ast.create_node(core_ast::node_type::TUPLE, bootstrap);
		new_ast.get_node(bootstrap).size = 0;

		auto main = new_ast.create_node(core_ast::node_type::FUNCTION, root_block);
		new_ast.get_node_data<core_ast::function_data>(main) = core_ast::function_data{
			"main", 0, 0, 0 /*these are all temporary*/
		};
		auto block = new_ast.create_node(core_ast::node_type::BLOCK, main);

		for (auto child : children)
		{ auto new_child = lower(block, ast.get_node(child), ast, new_ast, context); }

		new_ast.get_node_data<core_ast::function_data>(main).locals_size =
		  context.curr_fn_context.total_var_size;

		auto ret = new_ast.create_node(core_ast::node_type::RET, block);
		new_ast.get_node_data<core_ast::return_data>(ret) = { 0, 0, 0 };
		auto num = new_ast.create_node(core_ast::node_type::TUPLE, ret);

		return new_ast;
	}
} // namespace fe::ext_ast