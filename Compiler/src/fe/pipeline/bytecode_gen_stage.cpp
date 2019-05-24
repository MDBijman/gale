#include "fe/pipeline/bytecode_gen_stage.h"
#include "fe/data/bytecode.h"
#include "fe/data/core_ast.h"
#include "fe/pipeline/core_stack_analysis.h"
#include <optional>

namespace fe::vm
{
	code_gen_scope::code_gen_scope() {}
	code_gen_scope::code_gen_scope(core_ast::function_data fd) : current_function(fd) {}

	code_gen_state::code_gen_state(core_ast::label first_label) : next_label(first_label) {}

	code_gen_scope code_gen_state::set_scope(code_gen_scope new_scope)
	{
		auto old_scope = this->scope;
		this->scope = new_scope;
		return old_scope;
	}

	std::optional<reg> code_gen_state::last_alloced_register(uint32_t fid, uint32_t nid)
	{
		auto stack_size = this->analyzed_functions.at(fid).pre_node_stack_sizes.at(nid);
		if (stack_size == 0)
			return std::nullopt;
		else
			return reg(stack_size - 1);
	}

	std::optional<reg> code_gen_state::last_alloced_register_after(uint32_t fid, uint32_t nid)
	{
		auto stack_size = this->analyzed_functions.at(fid).node_stack_sizes.at(nid);
		if (stack_size == 0)
			return std::nullopt;
		else
			return reg(stack_size - 1);
	}

	reg code_gen_state::next_register(uint32_t fid, uint32_t nid)
	{
		return reg(this->analyzed_functions.at(fid).pre_node_stack_sizes.at(nid));
	}

	void code_gen_state::link_node_chunk(node_id n, uint8_t c)
	{
		node_to_chunk.insert_or_assign(n, c);
	}

	uint8_t code_gen_state::chunk_of(node_id n)
	{
		assert(node_to_chunk.find(n) != node_to_chunk.end());
		return node_to_chunk.at(n);
	}

	code_gen_result::code_gen_result() {}

	core_ast::label code_gen_state::get_function_label(const std::string &name)
	{
		auto p = this->functions.find(name);
		if (p == this->functions.end())
		{
			auto new_label = next_label;
			next_label.id++;
			this->functions.insert({ name, new_label });
			return new_label;
		}
		return p->second;
	}
	uint32_t code_gen_state::node_pre_stack_size(uint32_t function_id, uint32_t node_id)
	{
		return analyzed_functions.at(function_id).pre_node_stack_sizes.at(node_id);
	}
	uint32_t code_gen_state::node_post_stack_size(uint32_t function_id, uint32_t node_id)
	{
		return analyzed_functions.at(function_id).node_stack_sizes.at(node_id);
	}
	uint32_t code_gen_state::node_diff_stack_size(uint32_t function_id, uint32_t node_id)
	{
		return node_post_stack_size(function_id, node_id) -
		       node_pre_stack_size(function_id, node_id);
	}
	void code_gen_state::set_stack_label_size(uint32_t stack_label, uint32_t size)
	{
		stack_label_sizes.insert({ stack_label, size });
	}
	uint32_t code_gen_state::get_stack_label_size(uint32_t stack_label)
	{
		return stack_label_sizes.at(stack_label);
	}
} // namespace fe::vm

namespace fe::vm
{
	constexpr size_t RETURN_ADDRESS_SIZE = 8;
}

namespace fe::vm
{
	void generate_bytecode(node_id n, core_ast::ast &ast, module &p, code_gen_state &i);

	void link_to_parent_chunk(node_id n, core_ast::ast &ast, code_gen_state &i)
	{
		i.link_node_chunk(n, i.chunk_of(ast.parent_of(n).id));
	}

	void generate_number(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto fid = i.chunk_of(n);
		auto &bc = p.get_function(fid).get_bytecode();

		auto &num = ast.get_data<fe::number>(*ast.get_node(n).data_index);
		size_t size = 0;
		auto last = i.last_alloced_register(fid, n);
		reg r_result = (last ? last->val : -1) + number_size(num.type);
		switch (num.type)
		{
		case number_type::UI8:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_ui8(r_result, static_cast<uint8_t>(num.value)));
			break;
		}
		case number_type::I8:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_i8(r_result, static_cast<int8_t>(num.value)));
			break;
		}
		case number_type::UI16:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_ui16(r_result, static_cast<uint16_t>(num.value)));
			break;
		}
		case number_type::I16:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_i16(r_result, static_cast<int16_t>(num.value)));
			break;
		}
		case number_type::UI32:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_ui32(r_result, static_cast<uint32_t>(num.value)));
			break;
		}
		case number_type::I32:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_i32(r_result, static_cast<int32_t>(num.value)));
			break;
		}
		case number_type::UI64:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_ui64(r_result, static_cast<uint64_t>(num.value)));
			break;
		}
		case number_type::I64:
		{
			auto [location, size] = bc.add_instructions(
			  make_mv_reg_i64(r_result, static_cast<int64_t>(num.value)));
			break;
		}
		default: assert(!"Number type not supported");
		}
	}

	void generate_string(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		throw std::runtime_error("NYI string");
	}

	void generate_boolean(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto fid = i.chunk_of(n);
		auto &bc = p.get_function(fid).get_bytecode();

		auto value = ast.get_data<fe::boolean>(*ast.get_node(n).data_index).value;

		auto last_reg = i.last_alloced_register(fid, n);
		reg r_res = (last_reg ? last_reg->val + 1 : 0);
		auto [location, size] = bc.add_instructions(make_mv_reg_ui8(r_res, value ? 1 : 0));
	}

	void generate_function(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		auto &node = ast.get_node(n);
		assert(node.children.size() == 1);
		auto &func_data = ast.get_data<core_ast::function_data>(*node.data_index);

		auto stack_analysis = core_ast::analyze_stack(node.id, ast);

		auto id = p.add_function(function(func_data.name, bytecode()));
		i.link_node_chunk(n, id);
		i.analyzed_functions[id] = stack_analysis;

		auto prev_scope = i.set_scope(code_gen_scope(func_data));

		// Generate body
		auto [loc, _] = p.get_function(id).get_bytecode().add_instruction(
		  make_lbl(i.get_function_label(func_data.name).id));

		// Generate frame allocation
		auto stack_sizes = i.analyzed_functions[id].node_stack_sizes;
		auto max_size = 0;
		for (auto size : stack_sizes)
			if (size.second > max_size) max_size = size.second;

		p.get_function(id).get_bytecode().add_instruction(
		  make_alloc_ui8(max_size - func_data.in_size));

		generate_bytecode(node.children[0], ast, p, i);

		i.set_scope(prev_scope);
	}

	void generate_tuple(node_id n, core_ast::ast &ast, module &p, code_gen_state &info)
	{
		link_to_parent_chunk(n, ast, info);

		auto &node = ast.get_node(n);

		if (node.children.size() == 0) return;

		for (auto i = 0; i < node.children.size(); i++)
			generate_bytecode(node.children[i], ast, p, info);
	}

	void generate_block(node_id n, core_ast::ast &ast, module &p, code_gen_state &info)
	{
		auto chunk_id = 0;
		bool is_root = ast.root_id() == n;

		if (is_root) // Create new chunk
		{
			chunk_id = p.add_function(function{ "_main", bytecode(), {} });
			info.link_node_chunk(n, chunk_id);
		}
		else // Link to parent chunk
		{
			link_to_parent_chunk(n, ast, info);
			chunk_id = info.chunk_of(n);
		}

		auto &children = ast.children_of(n);
		assert(children.size() > 0);

		far_lbl location(chunk_id, p.get_function(chunk_id).get_bytecode().size());

		for (auto i = 0; i < children.size(); i++)
			generate_bytecode(children[i], ast, p, info);

		if (is_root)
		{ p.get_function(info.chunk_of(n)).get_bytecode().add_instruction(make_exit()); }

		} // weird clang #format

	void generate_function_call(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		auto &node = ast.get_node(n);
		auto fid = i.chunk_of(ast.parent_of(n).id);
		i.link_node_chunk(n, fid);
		auto &bc = p.get_function(i.chunk_of(n)).get_bytecode();

		auto &call_data = ast.get_data<core_ast::function_call_data>(*node.data_index);
		core_ast::label func_label = i.get_function_label(call_data.name);
		p.get_function(i.chunk_of(n))
		  .get_symbols()
		  .insert({ func_label.id, call_data.name });

		for (auto &child : node.children) { generate_bytecode(child, ast, p, i); }

		// The number of parameter registers
		auto after = i.last_alloced_register_after(fid, node.children.back());
		auto before = i.last_alloced_register(fid, n);
		uint8_t reg_count = after ? (after->val - (before ? before->val : -1)) : 0;

		reg res_reg = 0;
		if (call_data.out_size > 0)
			res_reg = (before ? before->val : -1) + call_data.out_size;

		// Perform call
		bc.add_instruction(
		  make_call_ui64_ui8_ui8_ui8(func_label.id, after->val, reg_count, res_reg.val));
	}

	void generate_reference(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		throw std::runtime_error("NYI ref");
	}

	void generate_return(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto &node = ast.get_node(n);
		assert(node.children.size() == 1);
		auto f_id = i.chunk_of(n);
		auto &bc = p.get_function(f_id).get_bytecode();

		generate_bytecode(node.children[0], ast, p, i);

		auto ret_data = ast.get_node_data<core_ast::return_data>(n);

		auto stack_sizes = i.analyzed_functions[f_id].node_stack_sizes;
		auto max_size = 0;
		for (auto size : stack_sizes)
			if (size.second > max_size) max_size = size.second;
		auto locals_size = i.scope.current_function.locals_size;

		bc.add_instruction(make_ret(
		  static_cast<uint8_t>(ret_data.in_size), max_size - ret_data.in_size,
		  i.last_alloced_register(f_id, n)->val + ret_data.out_size, ret_data.out_size));
	}

	void generate_stack_alloc(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
	}

	void generate_stack_dealloc(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto &size = ast.get_node_data<core_ast::size>(n);
	}

	void generate_jump_not_zero(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto fid = i.chunk_of(n);
		auto &bc = p.get_function(fid).get_bytecode();

		auto &node = ast.get_node(n);
		auto &lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto last_reg = i.last_alloced_register_after(fid, n);
		auto test_reg = (last_reg ? last_reg->val + 1 : 0);

		// the label is a placeholder for the actual location
		// We must first register all labels before we substitute the locations in the jumps
		// As labels can occur after the jumps to them and locations can still change
		auto [loc, size] = bc.add_instruction(make_jrnz_i32(test_reg, lbl.id));
	}

	void generate_jump_zero(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto fid = i.chunk_of(n);
		auto &bc = p.get_function(fid).get_bytecode();

		auto &node = ast.get_node(n);
		auto &lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto last_reg = i.last_alloced_register_after(fid, n);
		auto test_reg = (last_reg ? last_reg->val + 1 : 0);

		// the label is a placeholder for the actual location
		// We must first register all labels before we substitute the locations in the jumps
		// As labels can occur after the jumps to them and locations can still change
		auto [loc, size] = bc.add_instruction(make_jrz_i32(test_reg, lbl.id));
	}

	void generate_jump(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto &bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto &node = ast.get_node(n);
		auto &lbl = ast.get_data<core_ast::label>(*node.data_index);

		// the label is a placeholder for the actual location
		// We must first register all labels before we substitute the locations in the jumps
		// As labels can occur after the jumps to them and locations can still change
		auto [loc, size] = bc.add_instruction(make_jmpr_i32(lbl.id));
	}

	void generate_label(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto &bc = p.get_function(i.chunk_of(n)).get_bytecode();
		auto &node = ast.get_node(n);
		auto &lbl = ast.get_data<core_ast::label>(*node.data_index);

		auto [loc, size] = bc.add_instruction(make_lbl(lbl.id));
	}

	void generate_stack_label(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto f_id = i.chunk_of(n);
		auto &bc = p.get_function(f_id).get_bytecode();
		auto &node = ast.get_node(n);
		auto &lbl = ast.get_data<core_ast::stack_label>(*node.data_index);
		i.set_stack_label_size(lbl.id, i.node_post_stack_size(f_id, n));
	}

	void generate_binary_op(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto f_id = i.chunk_of(n);
		auto &bc = p.get_function(f_id).get_bytecode();
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		// children bytecode should leave 2 values on stack
		generate_bytecode(children[0], ast, p, i);
		auto first_size = i.node_diff_stack_size(f_id, children[0]);
		assert(first_size > 0 && first_size <= 8);

		generate_bytecode(children[1], ast, p, i);
		auto second_size = i.node_diff_stack_size(f_id, children[1]);
		assert(second_size > 0 && second_size <= 8);

		assert(second_size == first_size);
		auto in_size = first_size;

		auto r_res_rhs = reg(i.last_alloced_register_after(f_id, children[1])->val);
		auto r_res_lhs = reg(i.last_alloced_register_after(f_id, children[0])->val);
		auto r_res = i.last_alloced_register_after(f_id, n)->val;

		auto &node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::ADD:
			assert(in_size == 8);
			bc.add_instruction(make_add_r64_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::SUB:
			assert(in_size == 8);
			bc.add_instruction(make_sub_r64_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::MUL:
			assert(in_size == 8);
			bc.add_instruction(make_mul_r64_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::DIV:
			assert(in_size == 8);
			bc.add_instruction(make_div_r64_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::MOD:
			assert(in_size == 8);
			bc.add_instruction(make_mod_r64_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::AND:
		{
			switch (in_size)
			{
			case 8:
				bc.add_instructions(
				  make_and_r64_r64_r64(r_res + 7, r_res_lhs, r_res_rhs),
				  make_mv_rn_rn(1, r_res, r_res + 7));
				break;
			case 1:
				bc.add_instruction(make_and_r8_r8_r8(r_res, r_res_lhs, r_res_rhs));
				break;
			}
		}
		break;
		case core_ast::node_type::OR:
		{
			switch (in_size)
			{
			case 8:
				bc.add_instructions(
				  make_or_r64_r64_r64(r_res + 7, r_res_lhs, r_res_rhs),
				  make_mv_rn_rn(1, r_res, r_res + 7));
				break;
			case 1:
				bc.add_instruction(make_or_r8_r8_r8(r_res, r_res_lhs, r_res_rhs));
				break;
			}
		}
		break;
		case core_ast::node_type::LT:
			assert(in_size == 8);
			bc.add_instruction(make_lt_r8_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::LEQ:
			assert(in_size == 8);
			bc.add_instruction(make_lte_r8_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::GT:
			assert(in_size == 8);
			bc.add_instruction(make_gt_r8_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::GEQ:
			assert(in_size == 8);
			bc.add_instruction(make_gte_r8_r64_r64(r_res, r_res_lhs, r_res_rhs));
			break;
		case core_ast::node_type::EQ:
		{
			switch (in_size)
			{
			case 8:
				bc.add_instructions(
				  make_eq_r8_r64_r64(r_res, r_res_lhs, r_res_rhs));
				break;
			case 1:
				bc.add_instruction(make_eq_r8_r8_r8(r_res, r_res_lhs, r_res_rhs));
				break;
			}
		}
		break;
		case core_ast::node_type::NEQ:
		{
			switch (in_size)
			{
			case 8:
				bc.add_instructions(
				  make_neq_r8_r64_r64(r_res, r_res_lhs, r_res_rhs));
				break;
			case 1:
				bc.add_instruction(make_neq_r8_r8_r8(r_res, r_res_lhs, r_res_rhs));
				break;
			}
		}
		break;
		}
	}

	void generate_unary_op(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto f_id = i.chunk_of(n);
		auto &bc = p.get_function(f_id).get_bytecode();
		auto &children = ast.children_of(n);
		auto &node = ast.get_node(n);
		assert(children.size() == 1);

		assert(node.kind == core_ast::node_type::NOT);

		// children bytecode should leave 1 value on stack
		generate_bytecode(children[0], ast, p, i);
		auto child_size = i.node_diff_stack_size(f_id, children[0]);
		auto child_reg = *i.last_alloced_register(f_id, n);
		assert(child_size == 1);
		bc.add_instruction(make_xor_r8_r8_ui8(child_reg, child_reg, 1));
	}

	void generate_push(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto f_id = i.chunk_of(n);
		auto &bc = p.get_function(f_id).get_bytecode();
		auto &push_node = ast.get_node(n);
		assert(push_node.children.size() == 1); // sanity check
		auto &push_size = ast.get_node_data<core_ast::size>(push_node).val;

		auto &from = ast.get_node(push_node.children[0]);
		link_to_parent_chunk(from.id, ast, i);

		// These are the valid nodes describing push targets
		assert(from.kind == core_ast::node_type::VARIABLE ||
		       from.kind == core_ast::node_type::DYNAMIC_VARIABLE ||
		       from.kind == core_ast::node_type::PARAM ||
		       from.kind == core_ast::node_type::DYNAMIC_PARAM ||
		       from.kind == core_ast::node_type::STATIC_OFFSET ||
		       from.kind == core_ast::node_type::RELATIVE_OFFSET);

		// Currently only implemented these types
		assert(from.kind == core_ast::node_type::VARIABLE ||
		       from.kind == core_ast::node_type::PARAM ||
		       from.kind == core_ast::node_type::RELATIVE_OFFSET ||
		       from.kind == core_ast::node_type::DYNAMIC_PARAM ||
		       from.kind == core_ast::node_type::DYNAMIC_VARIABLE);

		auto dst_base = i.last_alloced_register_after(f_id, n)->val;
		auto src_base = 0;
		bool src_is_location = false;

		if (from.kind == core_ast::node_type::RELATIVE_OFFSET)
		{
			auto &data = ast.get_node_data<core_ast::relative_offset>(from);
			auto off = i.get_stack_label_size(data.label_id);
			src_base = off + data.offset - 1;
		}
		else if (from.kind == core_ast::node_type::DYNAMIC_PARAM ||
			 from.kind == core_ast::node_type::DYNAMIC_VARIABLE)
		{
			// #security #fixme what if push size overrides variable index
			src_is_location = true;
			auto idx_reg = i.last_alloced_register(f_id, n);
			auto &data = ast.get_node_data<core_ast::var_data>(from);
			src_base = idx_reg->val;
			assert(data.offset <= 255);
			bc.add_instruction(
			  make_add_r64_r64_ui8(src_base, src_base, data.offset + push_size - 1));
		}
		else
		{
			auto &data = ast.get_node_data<core_ast::var_data>(from);
			src_base = data.offset + data.size - 1;
		}

		if (!src_is_location)
			bc.add_instruction(make_mv_rn_rn(push_size, dst_base, src_base));
		else
			bc.add_instruction(make_mv_rn_ln(push_size, dst_base, src_base));
	}

	void generate_pop(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		link_to_parent_chunk(n, ast, i);
		auto fid = i.chunk_of(n);
		auto &bc = p.get_function(fid).get_bytecode();
		auto &node = ast.get_node(n);
		assert(node.children.size() == 1); // sanity check

		auto &to = ast.get_node(node.children[0]);
		auto &data_size = ast.get_node_data<core_ast::size>(node);

		// These are the valid nodes describing pop targets
		assert(to.kind == core_ast::node_type::VARIABLE ||
		       to.kind == core_ast::node_type::DYNAMIC_VARIABLE ||
		       to.kind == core_ast::node_type::PARAM ||
		       to.kind == core_ast::node_type::DYNAMIC_PARAM);

		// Currently only implemented variables and params
		assert(to.kind == core_ast::node_type::VARIABLE ||
		       to.kind == core_ast::node_type::PARAM);

		auto &to_var = ast.get_node_data<core_ast::var_data>(to);
		auto base = i.last_alloced_register(fid, n)->val;

		bc.add_instruction(
		  make_mv_rn_rn(to_var.size, reg(to_var.offset + to_var.size - 1), reg(base)));
	}

	void generate_bytecode(node_id n, core_ast::ast &ast, module &p, code_gen_state &i)
	{
		auto &node = ast.get_node(n);
		switch (node.kind)
		{
		case core_ast::node_type::NUMBER: return generate_number(n, ast, p, i);
		case core_ast::node_type::STRING: return generate_string(n, ast, p, i);
		case core_ast::node_type::BOOLEAN: return generate_boolean(n, ast, p, i);
		case core_ast::node_type::FUNCTION: return generate_function(n, ast, p, i);
		case core_ast::node_type::TUPLE: return generate_tuple(n, ast, p, i);
		case core_ast::node_type::BLOCK: return generate_block(n, ast, p, i);
		case core_ast::node_type::FUNCTION_CALL:
			return generate_function_call(n, ast, p, i);
		case core_ast::node_type::REFERENCE: return generate_reference(n, ast, p, i);
		case core_ast::node_type::RET: return generate_return(n, ast, p, i);
		case core_ast::node_type::STACK_ALLOC: return generate_stack_alloc(n, ast, p, i);
		case core_ast::node_type::STACK_DEALLOC:
			return generate_stack_dealloc(n, ast, p, i);
		case core_ast::node_type::JNZ: return generate_jump_not_zero(n, ast, p, i);
		case core_ast::node_type::JZ: return generate_jump_zero(n, ast, p, i);
		case core_ast::node_type::JMP: return generate_jump(n, ast, p, i);
		case core_ast::node_type::LABEL: return generate_label(n, ast, p, i);
		case core_ast::node_type::STACK_LABEL: return generate_stack_label(n, ast, p, i);
		case core_ast::node_type::PUSH: return generate_push(n, ast, p, i);
		case core_ast::node_type::POP: return generate_pop(n, ast, p, i);
		case core_ast::node_type::NOP: return;
		default:
			if (core_ast::is_binary_op(node.kind))
				return generate_binary_op(n, ast, p, i);
			if (core_ast::is_unary_op(node.kind))
				return generate_unary_op(n, ast, p, i);
			throw std::runtime_error("Error in bytecode gen. stage: unknown node type");
		}
	}

	module generate_bytecode(core_ast::ast &ast)
	{
		// module that will contain the chunks containing the bytecode
		module p;

		core_ast::ast_helper h(ast);

		// Find the highest label allocated so far in the ast, so function labels wont
		// overlap with jmp labels
		core_ast::label max_lbl{ 0 };
		h.for_all_t(core_ast::node_type::LABEL, [&max_lbl, &ast](core_ast::node &node) {
			auto &data = ast.get_node_data<core_ast::label>(node);
			if (data.id > max_lbl.id) max_lbl.id = data.id;
		});
		max_lbl.id++;

		// Meta information about intersection between core_ast and bytecode e.g. ast to
		// chunk mapping etc.
		code_gen_state i(max_lbl);

		// A bit hacky, for code in root
		auto stack_analysis_res = core_ast::stack_analysis_result();
		stack_analysis_res.pre_node_stack_sizes.insert({ 1, 0 });
		stack_analysis_res.node_stack_sizes.insert({ 1, 0 });
		stack_analysis_res.pre_node_stack_sizes.insert({ 2, 0 });
		stack_analysis_res.node_stack_sizes.insert({ 2, 0 });
		i.analyzed_functions.insert({ 0, stack_analysis_res });

		generate_bytecode(ast.root_id(), ast, p, i);

		return p;
	}
} // namespace fe::vm
