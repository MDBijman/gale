#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/data/types.h"
#include <unordered_map>
#include <tuple>

namespace fe::ext_ast
{
	// Helper

	enum class location_type
	{
		stack, reg, none
	};

	struct lowering_result
	{
		lowering_result(location_type l, int64_t p) : location(l)
		{
			if (l == location_type::stack) allocated_stack_space = p;
			else if (l == location_type::reg) result_register = static_cast<uint8_t>(p);
			else assert(!"Invalid location type");
		}
		lowering_result() : location(location_type::none) {}

		location_type location;
		uint8_t result_register;
		int64_t allocated_stack_space;
	};

	using variable_index = uint32_t;
	using label_index = uint32_t;

	struct function_context
	{
		variable_index next_variable = 0;
		variable_index next_param = 0;
		int total_var_size = 0;
		int total_param_size = 0;

		std::unordered_map<variable_index, std::pair<int/*offset from base*/, int/*size*/>> var_positions;

		variable_index alloc_variable(int size)
		{
			auto var_id = next_variable++;
			var_positions.insert({ var_id, { total_var_size, size } });
			total_var_size += size;
			return var_id;
		}

		variable_index alloc_param(int size)
		{
			auto var_id = next_param++;
			var_positions.insert({ var_id, { total_param_size, size } });
			total_param_size += size;
			return var_id;
		}
	};

	struct lowering_context
	{
		label_index next_label = 0;

		function_context curr_fn_context;

		variable_index alloc_variable(int size) { return curr_fn_context.alloc_variable(size); }
		variable_index alloc_param(int size) { return curr_fn_context.alloc_param(size); }
		variable_index get_offset(variable_index id) { return curr_fn_context.var_positions[id].first; }
		variable_index get_size(variable_index id) { return curr_fn_context.var_positions[id].second; }

		label_index new_label() { return next_label++; }
	};

	static bool has_children(node& n, ast& ast)
	{
		if (n.children_id == no_children) return false;
		return ast.get_children(n.children_id).size() > 0;
	}

	// Lowerers

	lowering_result lower_assignment(node_id p, node& n, ast& ext_ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto& children = ext_ast.children_of(n);
		assert(children.size() == 2);

		// a eval rhs -> result on stack
		// b get location of var
		// c move result on stack to var location
		// d dealloc from stack

		// a
		auto& value_node = ext_ast.get_node(children[1]);
		auto rhs = lower(p, value_node, ext_ast, new_ast, context);
		assert(rhs.location == location_type::stack);

		// b
		auto& id_node = ext_ast.get_node(children[0]);
		assert(id_node.kind == ext_ast::node_type::IDENTIFIER);
		auto& id_data = ext_ast.get_data<ext_ast::identifier>(ext_ast.get_node((*ext_ast
			.get_name_scope(id_node.name_scope_id)
			.resolve_variable(ext_ast.get_data<identifier>(id_node.data_index).segments[0], ext_ast.name_scope_cb()))
			.declaration_node).data_index);

		auto var_id = id_data.index_in_function;
		auto is_param = id_data.is_parameter;

		if (is_param)
		{
			assert(!"check if param");
		}
		else
		{
			// d
			auto pop = new_ast.create_node(core_ast::node_type::POP, p);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(pop).data_index).val = context.get_size(var_id);

			auto to = new_ast.create_node(core_ast::node_type::VARIABLE, pop);
			new_ast.get_node_data<core_ast::var_data>(to) = { context.get_offset(var_id), context.get_size(var_id) };
		}

		return lowering_result();
	}

	lowering_result lower_tuple(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TUPLE);
		auto& children = ast.children_of(n);
		auto tuple = new_ast.create_node(core_ast::node_type::TUPLE, p);

		int64_t stack_size = 0;
		for (auto child : children)
		{
			auto res = lower(tuple, ast.get_node(child), ast, new_ast, context);
			stack_size += res.allocated_stack_space;
		}

		return lowering_result(location_type::stack, stack_size);
	}

	lowering_result lower_block(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BLOCK);
		auto block = new_ast.create_node(core_ast::node_type::BLOCK, p);
		new_ast.get_node(block).size = 0;
		auto& children = ast.children_of(n);

		lowering_context nested_context = context;

		for (auto it = children.begin(); it != children.end(); it++)
		{
			auto& n = ast.get_node(*it);
			auto res = lower(block, n, ast, new_ast, nested_context);

			if (n.kind == node_type::BLOCK_RESULT)
			{
				assert(res.allocated_stack_space <= 8);
				assert(it == children.end() - 1);
				new_ast.get_node(block).size = res.allocated_stack_space;
			}
			else if (res.location == location_type::stack)
			{
				auto dealloc = new_ast.create_node(core_ast::node_type::STACK_DEALLOC, block);
				new_ast.get_data<core_ast::size>(*new_ast.get_node(dealloc).data_index).val = res.allocated_stack_space;
			}
		}

		context.next_label = nested_context.next_label;
		context.curr_fn_context.total_var_size += nested_context.curr_fn_context.total_var_size;

		return lowering_result(location_type::stack, *new_ast.get_node(block).size);
	}

	lowering_result lower_block_result(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		auto& child = ast.get_node(children[0]);
		return lower(p, child, ast, new_ast, context);
	}

	lowering_result lower_function(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::FUNCTION);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto fn_before = context.curr_fn_context;
		context.curr_fn_context = function_context();

		auto function_id = new_ast.create_node(core_ast::node_type::FUNCTION, p);
		// Assignment of function_id data is done in lower_declaration, except local size

		// Parameters
		auto& param_node = ast.get_node(children[0]);
		size_t in_size = 0;
		if (param_node.kind == node_type::IDENTIFIER)
		{
			in_size = (*ast
				.get_type_scope(param_node.type_scope_id)
				.resolve_variable(ast.get_data<identifier>(param_node.data_index), ast.type_scope_cb()))
				.type.calculate_size();
			auto var_id = context.alloc_param(in_size);
			ast.get_data<ext_ast::identifier>(param_node.data_index).index_in_function = var_id;
			ast.get_data<ext_ast::identifier>(param_node.data_index).is_parameter = true;
		}
		else if (param_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"nyi");
			// #todo generate set each identifier of the tuple to value of the rhs
		}
		else throw std::runtime_error("Error: parameter node type invalid");

		auto ret = new_ast.create_node(core_ast::node_type::RET, function_id);
		new_ast.get_data<core_ast::return_data>(*new_ast.get_node(ret).data_index) = core_ast::return_data{ in_size };

		auto block = new_ast.create_node(core_ast::node_type::BLOCK, ret);

		// Body
		auto& body_node = ast.get_node(children[1]);
		auto new_body = lower(block, body_node, ast, new_ast, context);

		assert(new_body.location == location_type::stack);
		new_ast.get_node(block).size = new_body.allocated_stack_space;
		new_ast.get_node_data<core_ast::return_data>(ret).size = new_body.allocated_stack_space;

		new_ast.get_node_data<core_ast::function_data>(function_id).locals_size = context.curr_fn_context.total_var_size;

		context.curr_fn_context = fn_before;

		// #todo functions should be root-scope only
		return lowering_result();
	}

	lowering_result lower_while_loop(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		auto& children = ast.children_of(n);
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

	lowering_result lower_if_statement(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		auto& children = ast.children_of(n);
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
			new_ast.get_data<core_ast::label>(*new_ast.get_node(jump).data_index).id = lbl;

			auto branch_block = new_ast.create_node(core_ast::node_type::BLOCK, p);
			auto body_res = lower(branch_block, ast.get_node(children[i + 1]), ast, new_ast, context);
			new_ast.get_node(branch_block).size = body_res.allocated_stack_space;

			// Each branch must allocate the same amount of space
			if (i == 0) size = body_res.allocated_stack_space;
			else assert(body_res.allocated_stack_space == size);

			auto jump_end = new_ast.create_node(core_ast::node_type::JMP, p);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(jump_end).data_index).id = after_lbl;

			auto label = new_ast.create_node(core_ast::node_type::LABEL, p);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(label).data_index).id = lbl;
		}

		if (has_else)
		{
			auto branch_block = new_ast.create_node(core_ast::node_type::BLOCK, p);
			auto body_res = lower(branch_block, ast.get_node(children.back()), ast, new_ast, context);
			assert(body_res.allocated_stack_space == size);
			new_ast.get_node(branch_block).size = body_res.allocated_stack_space;
		}

		auto after_label = new_ast.create_node(core_ast::node_type::LABEL, p);
		new_ast.get_data<core_ast::label>(*new_ast.get_node(after_label).data_index).id = after_lbl;

		return lowering_result(location_type::stack, size);
	}

	lowering_result lower_match(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::MATCH);
		auto& children = ast.children_of(n);

		assert(!"nyi match");
		return lowering_result();
	}

	lowering_result lower_id(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		// Assumes variable in current stack frame

		assert(n.kind == node_type::IDENTIFIER);
		assert(!has_children(n, ast));

		auto& id = ast.get_data<identifier>(n.data_index);
		auto size = (*ast
			.get_type_scope(n.type_scope_id)
			.resolve_variable(id, ast.type_scope_cb()))
			.type
			.calculate_size();
		auto& id_data = ast.get_data<identifier>(ast.get_node((*ast
			.get_name_scope(n.name_scope_id)
			.resolve_variable(id.root_identifier(), ast.name_scope_cb())).declaration_node)
			.data_index);

		auto read = new_ast.create_node(core_ast::node_type::MOVE, p);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(read).data_index).val = size;

		// First child resolves source address
		auto param_ref = new_ast.create_node(id_data.is_parameter ? core_ast::node_type::PARAM : core_ast::node_type::VARIABLE, read);
		new_ast.get_node_data<core_ast::var_data>(param_ref) = { context.get_offset(id_data.index_in_function), context.get_size(id_data.index_in_function) };

		// Second child resolves target address
		auto alloc = new_ast.create_node(core_ast::node_type::STACK_ALLOC, read);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(alloc).data_index).val = size;

		return lowering_result(location_type::stack, size);
	}

	lowering_result lower_string(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::STRING);
		assert(!has_children(n, ast));
		auto& str_data = ast.get_data<string>(n.data_index);

		// #todo do something?
		auto str = new_ast.create_node(core_ast::node_type::STRING, p);
		auto& str_node = new_ast.get_node(str);
		new_ast.get_data<string>(*str_node.data_index) = str_data;

		assert(!"nyi");
		return lowering_result();
	}

	lowering_result lower_boolean(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(!has_children(n, ast));
		auto& bool_data = ast.get_data<boolean>(n.data_index);

		auto bool_id = new_ast.create_node(core_ast::node_type::BOOLEAN, p);
		auto& bool_node = new_ast.get_node(bool_id);
		new_ast.get_data<boolean>(*bool_node.data_index) = bool_data;

		return lowering_result(location_type::stack, 1);
	}

	lowering_result lower_number(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::NUMBER);
		assert(!has_children(n, ast));
		auto& num_data = ast.get_data<number>(n.data_index);

		auto num_id = new_ast.create_node(core_ast::node_type::NUMBER, p);
		auto& num_node = new_ast.get_node(num_id);
		new_ast.get_data<number>(*num_node.data_index) = num_data;

		if (num_data.type == number_type::I8 || num_data.type == number_type::UI8)
			return lowering_result(location_type::stack, 1);
		else if (num_data.type == number_type::I16 || num_data.type == number_type::UI16)
			return lowering_result(location_type::stack, 2);
		else if (num_data.type == number_type::I32 || num_data.type == number_type::UI32)
			return lowering_result(location_type::stack, 4);
		else if (num_data.type == number_type::I64 || num_data.type == number_type::UI64)
			return lowering_result(location_type::stack, 8);
	}

	lowering_result lower_function_call(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto fun_id = new_ast.create_node(core_ast::node_type::FUNCTION_CALL, p);

		auto& name = ast.get_data<identifier>(ast.get_node(children[0]).data_index);
		assert(name.offsets.size() == 0);
		new_ast.get_data<core_ast::function_call_data>(*new_ast.get_node(fun_id).data_index).name = name.mangle();

		auto param = lower(fun_id, ast.get_node(children[1]), ast, new_ast, context);

		// #robustness? check if size < int64_t::max
		auto size = (*ast
			.get_type_scope(ast.get_node(children[0]).type_scope_id)
			.resolve_variable(ast.get_data<identifier>(ast.get_node(children[0]).data_index), ast.type_scope_cb()))
			.type.calculate_size();

		new_ast.get_node(fun_id).size = size;

		return lowering_result(location_type::stack, size);
	}

	lowering_result lower_array_access(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::ARRAY_ACCESS);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		// Literal of element size
		auto& id_node = ast.get_node(children[0]);
		auto type = ast.get_type_scope(id_node.type_scope_id).resolve_variable(ast.get_data<identifier>(id_node.data_index), ast.type_scope_cb());
		auto as_array = dynamic_cast<types::array_type*>(&type->type);
		size_t element_size = as_array->element_type->calculate_size();

		assert(id_node.kind == ext_ast::node_type::IDENTIFIER);
		auto& id_data = ast.get_data<ext_ast::identifier>(ast.get_node((*ast
			.get_name_scope(id_node.name_scope_id)
			.resolve_variable(ast.get_data<identifier>(id_node.data_index).segments[0], ast.name_scope_cb()))
			.declaration_node).data_index);

		auto var_id = id_data.index_in_function;
		auto is_param = id_data.is_parameter;

		if (is_param)
		{
			assert(!"check if param");
		}
		else
		{
			// Multiply index with element size to get actual location
			auto mul_node = new_ast.create_node(core_ast::node_type::MUL, p);
			// Index
			auto idx_node = lower(mul_node, ast.get_node(children[1]), ast, new_ast, context);
			auto num_node = new_ast.create_node(core_ast::node_type::NUMBER, mul_node);
			new_ast.get_node_data<number>(num_node) = { static_cast<int64_t>(element_size), number_type::UI64 };

			auto move = new_ast.create_node(core_ast::node_type::MOVE, p);
			auto from = new_ast.create_node(core_ast::node_type::DYNAMIC_VARIABLE, move);
			new_ast.get_node_data<core_ast::var_data>(from) = { context.get_offset(var_id), static_cast<uint32_t>(element_size) };

			auto to = new_ast.create_node(core_ast::node_type::STACK_ALLOC, move);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(to).data_index).val = context.get_size(var_id);

		}

		return lowering_result();
	}

	lowering_result lower_module_declaration(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::MODULE_DECLARATION);
		assert(ast.children_of(n).size() == 1);
		return lowering_result();
	}

	lowering_result lower_import_declaration(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::IMPORT_DECLARATION);
		return lowering_result();
	}

	lowering_result lower_export_stmt(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::EXPORT_STMT);
		return lowering_result();
	}

	lowering_result lower_declaration(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::DECLARATION);
		auto& children = ast.children_of(n);
		assert(children.size() == 3);

		auto& lhs_node = ast.get_node(children[0]);

		auto& rhs_node = ast.get_node(children[2]);
		auto rhs = lower(p, rhs_node, ast, new_ast, context);

		// Function declaration
		if (lhs_node.kind == node_type::IDENTIFIER && rhs_node.kind == node_type::FUNCTION)
		{
			// Put location in register
			auto& identifier_data = ast.get_data<ext_ast::identifier>(lhs_node.data_index);
			auto& function_data = new_ast.get_data<core_ast::function_data>(*new_ast.get_node(new_ast.get_node(p).children.back()).data_index);
			function_data.name = identifier_data.full;
			types::function_type* func_type = dynamic_cast<types::function_type*>(&(*ast
				.get_type_scope(n.type_scope_id)
				.resolve_variable(identifier_data, ast.type_scope_cb()))
				.type);
			function_data.in_size = func_type->from->calculate_size();
			function_data.out_size = func_type->to->calculate_size();

			return lowering_result();
		}
		// Variable declaration
		else if (lhs_node.kind == node_type::IDENTIFIER)
		{
			auto& identifier_data = ast.get_data<ext_ast::identifier>(lhs_node.data_index);
			auto var_id = context.alloc_variable(rhs.allocated_stack_space);
			identifier_data.index_in_function = var_id;

			// Pop value into result variable
			auto pop = new_ast.create_node(core_ast::node_type::POP, p);
			new_ast.get_node_data<core_ast::size>(pop).val = rhs.allocated_stack_space;
			auto r = new_ast.create_node(core_ast::node_type::VARIABLE, pop);
			new_ast.get_node_data<core_ast::var_data>(r) = { context.get_offset(var_id), context.get_size(var_id) };

			return lowering_result();
		}
		// Tuple declaration
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			// Tuple declarations are implemented by declaring n seperate variables and popping each individually off the stack.
			// Popping happens from right-to-left since the value of the rightmost id/variable is at the top of the stack.

			auto& ids = ast.children_of(lhs_node);
			std::vector<variable_index> var_idxs;
			for (auto it = ids.begin(); it != ids.end(); it++)
			{
				auto& node = ast.get_node(*it);
				auto& id = ast.get_data<identifier>(node.data_index);
				assert(node.kind == node_type::IDENTIFIER);
				auto& identifier_data = ast.get_data<ext_ast::identifier>(ast.get_node(*it).data_index);

				auto size = ast
					.get_type_scope(node.type_scope_id)
					.resolve_variable(id, ast.type_scope_cb())->type.calculate_size();

				auto var_idx = context.alloc_variable(size);
				identifier_data.index_in_function = var_idx;
				var_idxs.push_back(var_idx);
			}

			for (auto it = var_idxs.rbegin(); it != var_idxs.rend(); it++)
			{
				// Pop value into result variable
				auto pop = new_ast.create_node(core_ast::node_type::POP, p);
				new_ast.get_node_data<core_ast::size>(pop).val = context.get_size(*it);
				auto r = new_ast.create_node(core_ast::node_type::VARIABLE, pop);
				new_ast.get_node_data<core_ast::var_data>(r) = { context.get_offset(*it), context.get_size(*it) };
			}

			return lowering_result();
		}

		assert(!"error");
	}

	lowering_result lower_record(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::RECORD);
		return lowering_result();
	}

	lowering_result lower_type_definition(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto& name_id = ast.get_node(children[0]);
		auto& name_data = ast.get_data<identifier>(name_id.data_index);
		auto size = (*ast
			.get_type_scope(name_id.type_scope_id)
			.resolve_variable(name_data, ast.type_scope_cb()))
			.type.calculate_size();

		auto fn = new_ast.create_node(core_ast::node_type::FUNCTION, p);
		auto& fn_data = new_ast.get_data<core_ast::function_data>(*new_ast.get_node(fn).data_index);
		fn_data.in_size = size;
		fn_data.out_size = size;
		fn_data.name = name_data.segments[0];
		assert(size <= 8);

		auto input_var = context.alloc_variable(size);

		auto ret = new_ast.create_node(core_ast::node_type::RET, fn);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(ret).data_index).val = size;
		new_ast.get_node(ret).size = size;

		{
			auto move = new_ast.create_node(core_ast::node_type::MOVE, ret);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(move).data_index).val = size;

			// Resolve source (param at offset 0)
			auto from = new_ast.create_node(core_ast::node_type::VARIABLE, move);
			new_ast.get_node_data<core_ast::var_data>(from) = { context.get_offset(input_var), context.get_size(input_var) };

			// Resolve target (stack)
			auto to = new_ast.create_node(core_ast::node_type::STACK_ALLOC, move);
			new_ast.get_node_data<core_ast::size>(to).val = size;
		}

		return lowering_result();
	}

	lowering_result lower_type_atom(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TYPE_ATOM);
		return lowering_result();
	}

	lowering_result lower_reference(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::REFERENCE);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);

		assert(!"nyi");
		return lowering_result();
	}

	lowering_result lower_array_value(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(n.data_index == no_data);

		auto arr = new_ast.create_node(core_ast::node_type::TUPLE, p);
		auto& children = ast.children_of(n);
		int64_t size_sum = 0;
		for (auto child : children)
		{
			auto new_child = lower(arr, ast.get_node(child), ast, new_ast, context);
			size_sum += new_child.allocated_stack_space;
		}

		return lowering_result(location_type::stack, size_sum);
	}

	lowering_result lower_binary_op(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(ext_ast::is_binary_op(n.kind));
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		assert(n.data_index != no_data);


		core_ast::node_type new_node_type;
		// By default the number of stack bytes is the largest of the two operands
		// Boolean operators however reduce this to a single byte
		int32_t stack_bytes = 0;
		switch (n.kind)
		{
		case node_type::ADDITION:       new_node_type = core_ast::node_type::ADD; break;
		case node_type::SUBTRACTION:    new_node_type = core_ast::node_type::SUB; break;
		case node_type::MULTIPLICATION: new_node_type = core_ast::node_type::MUL; break;
		case node_type::DIVISION:       new_node_type = core_ast::node_type::DIV; break;
		case node_type::MODULO:         new_node_type = core_ast::node_type::MOD; break;
		case node_type::EQUALITY:       new_node_type = core_ast::node_type::EQ;  stack_bytes = 1; break;
		case node_type::GREATER_OR_EQ:  new_node_type = core_ast::node_type::GEQ; stack_bytes = 1; break;
		case node_type::GREATER_THAN:   new_node_type = core_ast::node_type::GT;  stack_bytes = 1; break;
		case node_type::LESS_OR_EQ:     new_node_type = core_ast::node_type::LEQ; stack_bytes = 1; break;
		case node_type::LESS_THAN:      new_node_type = core_ast::node_type::LT;  stack_bytes = 1; break;
		case node_type::AND:            new_node_type = core_ast::node_type::AND; break;
		case node_type::OR:             new_node_type = core_ast::node_type::OR;  break;
		default: throw lower_error{ "Unknown binary op" };
		}

		auto new_node = new_ast.create_node(new_node_type, p);

		// #todo and or short circuiting

		auto lhs = lower(new_node, ast.get_node(children[0]), ast, new_ast, context);
		auto rhs = lower(new_node, ast.get_node(children[1]), ast, new_ast, context);

		// If we hadnt set stack_bytes to 1, then we have a number operator
		if (stack_bytes == 0)
		{
			stack_bytes = lhs.allocated_stack_space > rhs.allocated_stack_space
				? lhs.allocated_stack_space
				: rhs.allocated_stack_space;
		}

		return lowering_result(location_type::stack, stack_bytes);
	}

	lowering_result lower(node_id p, node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT:         return lower_assignment(p, n, ast, new_ast, context);           break;
		case node_type::TUPLE:              return lower_tuple(p, n, ast, new_ast, context);                break;
		case node_type::BLOCK:              return lower_block(p, n, ast, new_ast, context);                break;
		case node_type::BLOCK_RESULT:       return lower_block_result(p, n, ast, new_ast, context);         break;
		case node_type::FUNCTION:           return lower_function(p, n, ast, new_ast, context);             break;
		case node_type::WHILE_LOOP:         return lower_while_loop(p, n, ast, new_ast, context);           break;
		case node_type::IF_STATEMENT:       return lower_if_statement(p, n, ast, new_ast, context);         break;
		case node_type::MATCH:              return lower_match(p, n, ast, new_ast, context);                break;
		case node_type::IDENTIFIER:         return lower_id(p, n, ast, new_ast, context);                   break;
		case node_type::STRING:             return lower_string(p, n, ast, new_ast, context);               break;
		case node_type::BOOLEAN:            return lower_boolean(p, n, ast, new_ast, context);              break;
		case node_type::NUMBER:             return lower_number(p, n, ast, new_ast, context);               break;
		case node_type::FUNCTION_CALL:      return lower_function_call(p, n, ast, new_ast, context);        break;
		case node_type::ARRAY_ACCESS:       return lower_array_access(p, n, ast, new_ast, context);         break;
		case node_type::MODULE_DECLARATION: return lower_module_declaration(p, n, ast, new_ast, context);   break;
		case node_type::IMPORT_DECLARATION: return lower_import_declaration(p, n, ast, new_ast, context);   break;
		case node_type::EXPORT_STMT:        return lower_export_stmt(p, n, ast, new_ast, context);          break;
		case node_type::DECLARATION:        return lower_declaration(p, n, ast, new_ast, context);          break;
		case node_type::RECORD:             return lower_record(p, n, ast, new_ast, context);               break;
		case node_type::TYPE_DEFINITION:    return lower_type_definition(p, n, ast, new_ast, context);      break;
		case node_type::TYPE_ATOM:          return lower_type_atom(p, n, ast, new_ast, context);            break;
		case node_type::REFERENCE:          return lower_reference(p, n, ast, new_ast, context);            break;
		case node_type::ARRAY_VALUE:        return lower_array_value(p, n, ast, new_ast, context);          break;
		default:
			if (ext_ast::is_binary_op(n.kind)) return lower_binary_op(p, n, ast, new_ast, context);

			assert(!"Node type not lowerable");
			throw std::runtime_error("Fatal Error - Node type not lowerable");
		}
	}


	core_ast::ast lower(ast& ast)
	{
		auto& root = ast.get_node(ast.root_id());
		assert(root.kind == node_type::BLOCK);
		auto& children = ast.children_of(root);

		lowering_context context;
		core_ast::ast new_ast(core_ast::node_type::BLOCK);
		auto root_block = new_ast.root_id();

		auto bootstrap = new_ast.create_node(core_ast::node_type::FUNCTION_CALL, root_block);
		new_ast.get_node_data<core_ast::function_call_data>(bootstrap).name = "main";
		new_ast.create_node(core_ast::node_type::TUPLE, bootstrap);
		new_ast.get_node(bootstrap).size = 0;

		auto main = new_ast.create_node(core_ast::node_type::FUNCTION, root_block);
		new_ast.get_node_data<core_ast::function_data>(main) = core_ast::function_data{
			"main", 0, 0, 0 /*these are all temporary*/
		};
		auto block = new_ast.create_node(core_ast::node_type::BLOCK, main);

		for (auto child : children)
		{
			auto new_child = lower(block, ast.get_node(child), ast, new_ast, context);
		}

		new_ast.get_node_data<core_ast::function_data>(main).locals_size = context.curr_fn_context.total_var_size;

		auto ret = new_ast.create_node(core_ast::node_type::RET, block);
		new_ast.get_node_data<core_ast::return_data>(ret) = { 0 };
		auto num = new_ast.create_node(core_ast::node_type::TUPLE, ret);


		return new_ast;
	}
}