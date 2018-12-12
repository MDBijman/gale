#include "fe/data/ext_ast.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/lowering_stage.h"
#include "fe/data/types.h"

namespace fe::ext_ast
{
	// Helper

	enum class location_type
	{
		stack, reg, none
	};

	struct lowering_result
	{
		lowering_result(node_id id, location_type l, int64_t p) : id(id), location(l) 
		{
			if (l == location_type::stack) allocated_stack_space = p;
			else if (l == location_type::reg) result_register = static_cast<uint8_t>(p);
			else assert(!"Invalid location type");
		}
		lowering_result(node_id id) : id(id), location(location_type::none) {}

		node_id id;
		location_type location;
		uint8_t result_register;
		int64_t allocated_stack_space;
	};

	struct lowering_context
	{
		using variable_index = uint32_t;
		variable_index next_variable = 0;
		using label_index = uint32_t;
		label_index next_label = 0;

		variable_index alloc_variable() { return next_variable++; }
		label_index new_label() { return next_label++; }
	};

	static bool has_children(node& n, ast& ast)
	{
		if (n.children_id == no_children) return false;
		return ast.get_children(n.children_id).size() > 0;
	}

	static void link_child_parent(node_id child, node_id parent, core_ast::ast& new_ast)
	{
		new_ast.get_node(child).parent_id = parent;
		new_ast.get_node(parent).children.push_back(child);
	}

	// Lowerers

	lowering_result lower_assignment(node& n, ast& ext_ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto& children = ext_ast.children_of(n);
		assert(children.size() == 2);

		// a eval rhs -> result on stack
		// b get location of var
		// c move result on stack to var location
		// d dealloc from stack

		auto block = new_ast.create_node(core_ast::node_type::BLOCK);

		// a
		auto& value_node = ext_ast.get_node(children[1]);
		auto rhs = lower(value_node, ext_ast, new_ast, context);
		link_child_parent(rhs.id, block, new_ast);
		assert(rhs.location == location_type::stack);

		// b
		auto& id_node = ext_ast.get_node(children[0]);
		assert(id_node.kind == ext_ast::node_type::IDENTIFIER);
		auto variable_id = ext_ast.get_data<ext_ast::identifier>(ext_ast.get_node((*ext_ast
			.get_name_scope(id_node.name_scope_id)
			.resolve_variable(ext_ast.get_data<identifier>(id_node.data_index).segments[0], ext_ast.name_scope_cb()))
			.declaration_node).data_index).index_in_function;

		// c
		if (rhs.allocated_stack_space > 8)
		{
			assert(!"Not working since rhs.result_register is undefined");
			auto move = new_ast.create_node(core_ast::node_type::MOVE, block);
			auto& mv_data = new_ast.get_data<core_ast::size>(*new_ast.get_node(move).data_index);
			mv_data.val = rhs.allocated_stack_space;

			auto from = new_ast.create_node(core_ast::node_type::LOCAL_ADDRESS, move);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(from).data_index).val = rhs.result_register;

			auto to = new_ast.create_node(core_ast::node_type::LOCAL_ADDRESS, move);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(to).data_index).val = variable_id;

			// d
			auto dealloc = new_ast.create_node(core_ast::node_type::STACK_DEALLOC);
			link_child_parent(dealloc, block, new_ast);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(dealloc).data_index).val = rhs.allocated_stack_space;
		}
		else
		{
			// d
			auto pop = new_ast.create_node(core_ast::node_type::POP, block);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(pop).data_index).val = rhs.allocated_stack_space;

			auto to = new_ast.create_node(core_ast::node_type::REGISTER, pop);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(to).data_index).val = variable_id;
		}

		return lowering_result(block);
	}

	lowering_result lower_tuple(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TUPLE);
		auto& children = ast.children_of(n);
		auto tuple = new_ast.create_node(core_ast::node_type::TUPLE);

		int64_t stack_size = 0;
		for (auto child : children)
		{
			auto res = lower(ast.get_node(child), ast, new_ast, context);
			link_child_parent(res.id, tuple, new_ast);
			stack_size = res.allocated_stack_space;
		}

		return lowering_result(tuple, location_type::stack, stack_size);
	}

	lowering_result lower_block(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BLOCK);
		auto block = new_ast.create_node(core_ast::node_type::BLOCK);
		auto& block_n = new_ast.get_node(block);
		block_n.size = 0;
		auto& children = ast.children_of(n);

		lowering_context nested_context = context;

		int64_t declaration_sum = 0;
		for (auto it = children.begin(); it != children.end(); it++)
		{
			auto& n = ast.get_node(*it);
			auto res = lower(n, ast, new_ast, context);
			link_child_parent(res.id, block, new_ast);

			if (n.kind == node_type::BLOCK_RESULT)
			{
				assert(res.allocated_stack_space <= 8);
				assert(it == children.end() - 1);
				auto pop = new_ast.create_node(core_ast::node_type::POP, block);
				new_ast.get_node_data<core_ast::size>(pop).val = res.allocated_stack_space;
				new_ast.create_node(core_ast::node_type::RESULT_REGISTER, pop);

				block_n.size = res.allocated_stack_space;
			}
			else if (n.kind == node_type::DECLARATION)
			{
				declaration_sum += res.allocated_stack_space;
			}
			else if (res.location == location_type::stack)
			{
				auto dealloc = new_ast.create_node(core_ast::node_type::STACK_DEALLOC, block);
				new_ast.get_data<core_ast::size>(*new_ast.get_node(dealloc).data_index).val = res.allocated_stack_space;
			}
		}

		if (declaration_sum > 0)
		{
			auto dealloc = new_ast.create_node(core_ast::node_type::STACK_DEALLOC);
			link_child_parent(dealloc, block, new_ast);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(dealloc).data_index).val = declaration_sum;
		}

		context.next_label = nested_context.next_label;

		return lowering_result(block, location_type::stack, *block_n.size);
	}

	lowering_result lower_block_result(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);
		auto& child = ast.get_node(children[0]);
		return lower(child, ast, new_ast, context);
	}

	lowering_result lower_function(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::FUNCTION);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto variables_before = context.next_variable;
		context.next_variable = 0;

		auto function_id = new_ast.create_node(core_ast::node_type::FUNCTION);
		// Assignment of function_id name data is done in lower_declaration

		// Parameters
		auto& param_node = ast.get_node(children[0]);
		size_t in_size = 0;
		if (param_node.kind == node_type::IDENTIFIER)
		{
			in_size = (*ast
				.get_type_scope(param_node.type_scope_id)
				.resolve_variable(ast.get_data<identifier>(param_node.data_index), ast.type_scope_cb()))
				.type.calculate_size();
			ast.get_data<ext_ast::identifier>(param_node.data_index).index_in_function = context.alloc_variable();
			// #todo put stack offset in register
		}
		else if (param_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"nyi");
			// #todo generate set each identifier of the tuple to value of the rhs
		}
		else throw std::runtime_error("Error: parameter node type invalid");

		auto ret = new_ast.create_node(core_ast::node_type::RET, function_id);
		new_ast.get_data<core_ast::return_data>(*new_ast.get_node(ret).data_index) = core_ast::return_data{ in_size, };

		auto block = new_ast.create_node(core_ast::node_type::BLOCK, ret);
		// Body
		auto& body_node = ast.get_node(children[1]);
		auto new_body = lower(body_node, ast, new_ast, context);
		link_child_parent(new_body.id, block, new_ast);

		new_ast.get_node(ret).size = new_ast.get_node(new_body.id).size;

		context.next_variable = variables_before;

		// #todo functions should be root-scope only
		return lowering_result(function_id);
	}

	lowering_result lower_while_loop(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		assert(!"nyi while");
		return lowering_result(0);
	}

	lowering_result lower_if_statement(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		auto& children = ast.children_of(n);
		assert(children.size() >= 2);

		auto block = new_ast.create_node(core_ast::node_type::BLOCK);

		bool has_else = children.size() % 2 == 1;

		auto after_lbl = context.new_label();

		int64_t size = 0;
		for (int i = 0; i < children.size() - 1; i += 2)
		{
			auto lbl = context.new_label();
			auto test_res = lower(ast.get_node(children[i]), ast, new_ast, context);
			link_child_parent(test_res.id, block, new_ast);

			// Does not count towards allocation size since JZ consumes the byte
			assert(test_res.allocated_stack_space == 1);

			auto jump = new_ast.create_node(core_ast::node_type::JZ, block);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(jump).data_index).id = lbl;

			auto body_res = lower(ast.get_node(children[i + 1]), ast, new_ast, context);
			link_child_parent(body_res.id, block, new_ast);

			// Each branch must allocate the same amount of space
			if (i == 0) size = body_res.allocated_stack_space;
			else assert(body_res.allocated_stack_space == size);

			auto jump_end = new_ast.create_node(core_ast::node_type::JMP, block);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(jump_end).data_index).id = after_lbl;

			auto label = new_ast.create_node(core_ast::node_type::LABEL, block);
			new_ast.get_data<core_ast::label>(*new_ast.get_node(label).data_index).id = lbl;
		}

		// This size will be used in bytecode generation
		new_ast.get_node(block).size = size;

		if (has_else)
		{
			auto body_res = lower(ast.get_node(children.back()), ast, new_ast, context);
			link_child_parent(body_res.id, block, new_ast);
			assert(body_res.allocated_stack_space == size);
		}

		auto after_label = new_ast.create_node(core_ast::node_type::LABEL, block);
		new_ast.get_data<core_ast::label>(*new_ast.get_node(after_label).data_index).id = after_lbl;

		return lowering_result(block, location_type::stack, size);
	}

	lowering_result lower_match(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::MATCH);
		auto& children = ast.children_of(n);

		assert(!"nyi match");
		return lowering_result(0);
	}

	lowering_result lower_id(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
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
		auto offset = (*ast
			.get_type_scope(n.type_scope_id)
			.resolve_variable(id.root_identifier(), ast.type_scope_cb()))
			.type
			.calculate_offset(id.offsets);
		auto location_register = ast.get_data<identifier>(ast.get_node((*ast
			.get_name_scope(n.name_scope_id)
			.resolve_variable(id.root_identifier(), ast.name_scope_cb())).declaration_node)
			.data_index).index_in_function;

		auto read = new_ast.create_node(core_ast::node_type::MOVE);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(read).data_index).val = size;

		// First child resolves source address
		auto param_ref = new_ast.create_node(size > 8
			? core_ast::node_type::LOCAL_ADDRESS
			: core_ast::node_type::REGISTER, read);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(param_ref).data_index).val = location_register;

		// Second child resolves target address
		auto alloc = new_ast.create_node(core_ast::node_type::STACK_ALLOC, read);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(alloc).data_index).val = size;

		return lowering_result(read, location_type::stack, size);
	}

	lowering_result lower_string(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::STRING);
		assert(!has_children(n, ast));
		auto& str_data = ast.get_data<string>(n.data_index);

		// #todo do something?
		auto str = new_ast.create_node(core_ast::node_type::STRING);
		auto& str_node = new_ast.get_node(str);
		new_ast.get_data<string>(*str_node.data_index) = str_data;

		assert(!"nyi");
		return lowering_result(0);
	}

	lowering_result lower_boolean(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(!has_children(n, ast));
		auto& bool_data = ast.get_data<boolean>(n.data_index);

		auto bool_id = new_ast.create_node(core_ast::node_type::BOOLEAN);
		auto& bool_node = new_ast.get_node(bool_id);
		new_ast.get_data<boolean>(*bool_node.data_index) = bool_data;

		return lowering_result(bool_id, location_type::stack, 1);
	}

	lowering_result lower_number(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::NUMBER);
		assert(!has_children(n, ast));
		auto& num_data = ast.get_data<number>(n.data_index);

		auto num_id = new_ast.create_node(core_ast::node_type::NUMBER);
		auto& num_node = new_ast.get_node(num_id);
		new_ast.get_data<number>(*num_node.data_index) = num_data;

		if (num_data.type == number_type::I8 || num_data.type == number_type::UI8)
			return lowering_result(num_id, location_type::stack, 1);
		else if (num_data.type == number_type::I16 || num_data.type == number_type::UI16)
			return lowering_result(num_id, location_type::stack, 2);
		else if (num_data.type == number_type::I32 || num_data.type == number_type::UI32)
			return lowering_result(num_id, location_type::stack, 4);
		else if (num_data.type == number_type::I64 || num_data.type == number_type::UI64)
			return lowering_result(num_id, location_type::stack, 8);
	}

	lowering_result lower_function_call(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto& children = ast.children_of(n);
		assert(children.size() == 2);

		auto fun_id = new_ast.create_node(core_ast::node_type::FUNCTION_CALL);

		auto& name = ast.get_data<identifier>(ast.get_node(children[0]).data_index);
		assert(name.offsets.size() == 0);
		new_ast.get_data<core_ast::function_call_data>(*new_ast.get_node(fun_id).data_index).name = name.mangle();

		auto param = lower(ast.get_node(children[1]), ast, new_ast, context);
		auto& param_node = new_ast.get_node(param.id);
		link_child_parent(param.id, fun_id, new_ast);

		// #robustness? check if size < int64_t::max
		auto size = (*ast
			.get_type_scope(ast.get_node(children[0]).type_scope_id)
			.resolve_variable(ast.get_data<identifier>(ast.get_node(children[0]).data_index), ast.type_scope_cb()))
			.type.calculate_size();

		new_ast.get_node(fun_id).size = size;

		return lowering_result(fun_id, location_type::stack, size);
	}

	lowering_result lower_module_declaration(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::MODULE_DECLARATION);
		assert(ast.children_of(n).size() == 1);
		return lowering_result(new_ast.create_node(core_ast::node_type::NOP));
	}

	lowering_result lower_import_declaration(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::IMPORT_DECLARATION);
		return lowering_result(new_ast.create_node(core_ast::node_type::NOP));
	}

	lowering_result lower_export_stmt(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::EXPORT_STMT);
		return lowering_result(new_ast.create_node(core_ast::node_type::NOP));
	}

	lowering_result lower_declaration(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::DECLARATION);
		auto& children = ast.children_of(n);
		assert(children.size() == 3);

		auto block = new_ast.create_node(core_ast::node_type::BLOCK);

		auto& lhs_node = ast.get_node(children[0]);
		if (lhs_node.kind == node_type::IDENTIFIER)
		{
			// Put location in register
			auto& identifier_data = ast.get_data<ext_ast::identifier>(lhs_node.data_index);
			identifier_data.index_in_function = context.alloc_variable();

			auto& rhs_node = ast.get_node(children[2]);
			auto rhs = lower(rhs_node, ast, new_ast, context);
			link_child_parent(rhs.id, block, new_ast);

			if (rhs_node.kind == node_type::FUNCTION)
			{
				auto& function_data = new_ast.get_data<core_ast::function_data>(*new_ast.get_node(rhs.id).data_index);
				function_data.name = identifier_data.full;
				function_data.label = context.new_label();
				types::function_type* func_type = dynamic_cast<types::function_type*>(&(*ast
					.get_type_scope(n.type_scope_id)
					.resolve_variable(identifier_data, ast.type_scope_cb()))
					.type);
				function_data.in_size = func_type->from->calculate_size();
				function_data.out_size = func_type->to->calculate_size();
			}
			else
			{
				// Keep on stack, put location in register
				if (rhs.allocated_stack_space > 8)
				{
					auto mv = new_ast.create_node(core_ast::node_type::MOVE, block);
					new_ast.create_node(core_ast::node_type::SP_REGISTER, mv);
					auto r = new_ast.create_node(core_ast::node_type::REGISTER, mv);
					new_ast.get_node_data<core_ast::size>(r).val = identifier_data.index_in_function;
				}
				// Put value in register directly
				else
				{
					// Pop value into result register
					auto pop = new_ast.create_node(core_ast::node_type::POP, block);
					new_ast.get_node_data<core_ast::size>(pop).val = rhs.allocated_stack_space;
					auto r = new_ast.create_node(core_ast::node_type::REGISTER, pop);
					new_ast.get_node_data<core_ast::size>(r).val = identifier_data.index_in_function;
				}
			}

			return lowering_result(block, location_type::stack, rhs.allocated_stack_space);
		}
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			assert(!"identifier tuple lowering nyi");
		}

		assert(!"error");
	}

	lowering_result lower_record(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::RECORD);
		return lowering_result(new_ast.create_node(core_ast::node_type::NOP));
	}

	lowering_result lower_type_definition(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
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

		auto fn = new_ast.create_node(core_ast::node_type::FUNCTION);
		auto& fn_data = new_ast.get_data<core_ast::function_data>(*new_ast.get_node(fn).data_index);
		fn_data.in_size = size;
		fn_data.out_size = size;
		fn_data.name = name_data.segments[0];
		fn_data.label = context.new_label();
		assert(size <= 8);

		auto location_register = context.alloc_variable();

		auto ret = new_ast.create_node(core_ast::node_type::RET, fn);
		new_ast.get_data<core_ast::size>(*new_ast.get_node(ret).data_index).val = size;
		new_ast.get_node(ret).size = size;

		{
			auto move = new_ast.create_node(core_ast::node_type::MOVE, ret);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(move).data_index).val = size;

			// Resolve source (param at offset 0)
			auto from = new_ast.create_node(core_ast::node_type::LOCAL_ADDRESS, move);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(from).data_index).val = location_register;

			// Resolve target (stack)
			auto to = new_ast.create_node(core_ast::node_type::STACK_ALLOC, move);
			new_ast.get_data<core_ast::size>(*new_ast.get_node(to).data_index).val = size;
		}

		return lowering_result(fn);
	}

	lowering_result lower_type_atom(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::TYPE_ATOM);
		return lowering_result(new_ast.create_node(core_ast::node_type::NOP));
	}

	lowering_result lower_reference(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::REFERENCE);
		auto& children = ast.children_of(n);
		assert(children.size() == 1);

		assert(!"nyi");
		return lowering_result(0);
	}

	lowering_result lower_array_value(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		assert(n.data_index == no_data);

		auto arr = new_ast.create_node(core_ast::node_type::TUPLE);
		auto& children = ast.children_of(n);
		int64_t size_sum = 0;
		for (auto child : children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast, context);
			link_child_parent(new_child.id, arr, new_ast);
			size_sum += new_child.allocated_stack_space;
		}

		return lowering_result(arr, location_type::stack, size_sum);
	}

	lowering_result lower_binary_op(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		assert(ext_ast::is_binary_op(n.kind));
		auto& children = ast.children_of(n);
		assert(children.size() == 2);
		assert(n.data_index != no_data);

		// #todo and or short circuiting
		auto lhs = lower(ast.get_node(children[0]), ast, new_ast, context);
		auto rhs = lower(ast.get_node(children[1]), ast, new_ast, context);

		core_ast::node_type new_node_type;
		// By default the number of stack bytes is the largest of the two operands
		// Boolean operators however reduce this to a single byte
		int32_t stack_bytes = lhs.allocated_stack_space > rhs.allocated_stack_space ? lhs.allocated_stack_space : rhs.allocated_stack_space;
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

		auto new_node = new_ast.create_node(new_node_type);
		new_ast.link_child_parent(lhs.id, new_node);
		new_ast.link_child_parent(rhs.id, new_node);

		return lowering_result(new_node, location_type::stack, stack_bytes);
	}

	lowering_result lower(node& n, ast& ast, core_ast::ast& new_ast, lowering_context& context)
	{
		switch (n.kind)
		{
		case node_type::ASSIGNMENT:         return lower_assignment(n, ast, new_ast, context);           break;
		case node_type::TUPLE:              return lower_tuple(n, ast, new_ast, context);                break;
		case node_type::BLOCK:              return lower_block(n, ast, new_ast, context);                break;
		case node_type::BLOCK_RESULT:       return lower_block_result(n, ast, new_ast, context);         break;
		case node_type::FUNCTION:           return lower_function(n, ast, new_ast, context);             break;
		case node_type::WHILE_LOOP:         return lower_while_loop(n, ast, new_ast, context);           break;
		case node_type::IF_STATEMENT:       return lower_if_statement(n, ast, new_ast, context);         break;
		case node_type::MATCH:              return lower_match(n, ast, new_ast, context);                break;
		case node_type::IDENTIFIER:         return lower_id(n, ast, new_ast, context);                   break;
		case node_type::STRING:             return lower_string(n, ast, new_ast, context);               break;
		case node_type::BOOLEAN:            return lower_boolean(n, ast, new_ast, context);              break;
		case node_type::NUMBER:             return lower_number(n, ast, new_ast, context);               break;
		case node_type::FUNCTION_CALL:      return lower_function_call(n, ast, new_ast, context);        break;
		case node_type::MODULE_DECLARATION: return lower_module_declaration(n, ast, new_ast, context);   break;
		case node_type::IMPORT_DECLARATION: return lower_import_declaration(n, ast, new_ast, context);   break;
		case node_type::EXPORT_STMT:        return lower_export_stmt(n, ast, new_ast, context);          break;
		case node_type::DECLARATION:        return lower_declaration(n, ast, new_ast, context);          break;
		case node_type::RECORD:             return lower_record(n, ast, new_ast, context);               break;
		case node_type::TYPE_DEFINITION:    return lower_type_definition(n, ast, new_ast, context);      break;
		case node_type::TYPE_ATOM:          return lower_type_atom(n, ast, new_ast, context);            break;
		case node_type::REFERENCE:          return lower_reference(n, ast, new_ast, context);            break;
		case node_type::ARRAY_VALUE:        return lower_array_value(n, ast, new_ast, context);          break;
		default:
			if (ext_ast::is_binary_op(n.kind)) return lower_binary_op(n, ast, new_ast, context);

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
		auto block = new_ast.root_id();

		for (auto child : children)
		{
			auto new_child = lower(ast.get_node(child), ast, new_ast, context);
			link_child_parent(new_child.id, block, new_ast);
		}

		return new_ast;
	}
}