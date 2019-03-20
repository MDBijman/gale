#include "fe/pipeline/bytecode_optimization_stage.h"
#include "fe/data/bytecode.h"
#include <assert.h>
#include <iostream>
#include <optional>
#include <stack>
#include <string>

namespace fe::vm
{
	void optimize_program(program &e, optimization_settings &s)
	{
		program_dependency_graph dg = build_dependency_graph(e);
		while ( // optimize_dependencies(e, dg, s)
		  /*||*/ optimize_single_ops(e, dg, s) || remove_dependantless_instructions(e, dg))
			;
	}

	void function_dependency_graph::add_offset(uint64_t loc, uint32_t size)
	{
		for (auto &dep : dependencies)
		{
			if (dep.depends_on >= loc) dep.depends_on += size;
			if (dep.instruction_id >= loc) dep.instruction_id += size;
		}
	}

	program_dependency_graph build_dependency_graph(program &e)
	{
		program_dependency_graph graph;

		auto &functions = e.get_code();

		for (auto it = functions.begin(); it != functions.end(); it++)
		{
			function_id id = std::distance(functions.begin(), it);
			function &func = *it;
			if (!func.is_bytecode()) continue;

			bytecode &bc = func.get_bytecode();

			// Map of register ids to instruction ids
			// where latest_writes[register id] = instruction id
			uint64_t latest_writes[64] = { 0 };

			// Map of register ids to instruction ids
			std::unordered_map<uint64_t, uint64_t> index_to_op;

			// Vector of generated instruction dependencies
			function_dependency_graph local_graph;

			std::stack<uint64_t> latest_push_ops;

			for (uint64_t i = 0, op_i = 0; i < bc.size(); op_i++)
			{
				auto current_instruction = bc.get_instruction<10>(i);
				op_kind kind = byte_to_op(current_instruction[0].val);
				size_t size = op_size(kind);

				switch (kind)
				{
				case op_kind::ADD_REG_REG_REG:
				case op_kind::SUB_REG_REG_REG:
				case op_kind::MUL_REG_REG_REG:
				case op_kind::DIV_REG_REG_REG:
				case op_kind::MOD_REG_REG_REG:
				case op_kind::GT_REG_REG_REG:
				case op_kind::GTE_REG_REG_REG:
				case op_kind::LT_REG_REG_REG:
				case op_kind::LTE_REG_REG_REG:
				case op_kind::EQ_REG_REG_REG:
				case op_kind::NEQ_REG_REG_REG:
				case op_kind::AND_REG_REG_REG:
				case op_kind::OR_REG_REG_REG:
				{
					reg dest = current_instruction[1].val;
					reg src1 = current_instruction[2].val;
					reg src2 = current_instruction[3].val;

					local_graph.dependencies.push_back(
					  dependency{ i, latest_writes[src1.val] });
					local_graph.dependencies.push_back(
					  dependency{ i, latest_writes[src2.val] });

					latest_writes[dest.val] = i;
					break;
				}
				case op_kind::MV_REG_UI8:
				case op_kind::MV_REG_UI16:
				case op_kind::MV_REG_UI32:
				case op_kind::MV_REG_UI64:
				case op_kind::MV_REG_I8:
				case op_kind::MV_REG_I16:
				case op_kind::MV_REG_I32:
				case op_kind::MV_REG_I64:
				{
					reg dest = current_instruction[1].val;

					latest_writes[dest.val] = i;
					break;
				}
				case op_kind::MV8_REG_REG:
				case op_kind::MV16_REG_REG:
				case op_kind::MV32_REG_REG:
				case op_kind::MV64_REG_REG:
				{
					reg dest = current_instruction[1].val;
					reg src = current_instruction[2].val;

					local_graph.dependencies.push_back(
					  dependency{ i, latest_writes[src.val] });

					latest_writes[dest.val] = i;
					break;
				}
				case op_kind::JRNZ_REG_I32:
				case op_kind::JRZ_REG_I32:
				{
					reg src = current_instruction[1].val;

					latest_push_ops = {};
					local_graph.dependencies.push_back(
					  dependency{ i, latest_writes[src.val] });
					break;
				}
				case op_kind::LBL_UI32:
				{
					latest_push_ops = {};
					break;
				}
				case op_kind::CALL_UI64_UI8_UI8_UI8:
				{
					for (int i = 0; i < 32; i++) latest_writes[i] = 0;
					latest_push_ops = {};
					latest_writes[60] = i;
					break;
				}
				}

				index_to_op.insert({ i, op_i });
				i += size;
			}

			graph[id].dependencies = std::move(local_graph.dependencies);
		}

		return graph;
	}

	/*
	 * Reduces pairs of operations into a single operation, when dependencies allow.
	 * E.g. mov r1 1, mov r2 r1 becomes mov r2 1.
	 * Dependencies are updated so that the new instruction depends on all old dependencies, and
	 * all old dependants depend on the new instruction. Returns true if any changes to the
	 * program were made
	 */
	// bool optimize_dependencies(program& e, program_dependency_graph& g,
	// optimization_settings& s)
	//{
	//	bool change_made = false;

	//	for (auto&[fun_id, fun_dg] : g)
	//	{
	//		auto& function = e.get_function(fun_id);
	//		assert(function.is_bytecode());
	//		auto& bytecode = function.get_bytecode();

	//		while (true)
	//		{
	//			// first: dependency, second: dependant, third: replacement
	//			std::optional<std::tuple<uint64_t, uint64_t, uint64_t>> replaced_op;

	//			for (auto it = fun_dg.dependencies.begin(); it !=
	//fun_dg.dependencies.end(); it++)
	//			{
	//				auto& dep = *it;

	//				auto replacement =
	//peephole_optimizations::try_remove_dependency(bytecode, fun_dg, dep);

	//				if (replacement)
	//				{
	//					replaced_op = { dep.depends_on, dep.instruction_id,
	//*replacement }; 					break;
	//				}
	//			}

	//			if (!replaced_op)
	//				break;

	//			std::vector<dependency> replacement_deps;
	//			for (auto& dep : fun_dg.dependencies)
	//			{
	//				auto dependency = std::get<0>(*replaced_op);
	//				auto dependant = std::get<1>(*replaced_op);
	//				auto replacement = std::get<2>(*replaced_op);

	//				if (dep.instruction_id == dependant && dep.depends_on ==
	//dependency) 					continue;

	//				assert(dep.depends_on != dependency);
	//				if (dep.depends_on == dependant)
	//					replacement_deps.push_back(vm::dependency{
	//dep.instruction_id, replacement }); 				else if (dep.instruction_id == dependency)
	//					replacement_deps.push_back(vm::dependency{ replacement,
	//dep.depends_on }); 				else if (dep.instruction_id == dependant && dep.depends_on !=
	//dependency) 					replacement_deps.push_back(vm::dependency{ replacement, dep.depends_on });
	//			}

	//			fun_dg.dependencies.erase(std::remove_if(fun_dg.dependencies.begin(),
	//fun_dg.dependencies.end(), [replaced_op](auto& dep) { 				return (std::get<0>(*replaced_op) ==
	//dep.instruction_id
	//					|| std::get<1>(*replaced_op) == dep.depends_on
	//					|| std::get<1>(*replaced_op) == dep.instruction_id
	//					|| std::get<1>(*replaced_op) == dep.depends_on);
	//			}), fun_dg.dependencies.end());

	//			std::move(replacement_deps.begin(), replacement_deps.end(),
	//std::back_inserter(fun_dg.dependencies));
	//		}

	//	}

	//	return change_made;
	//}

	namespace peephole_optimizations
	{
		template <typename To, typename From> bool fits(From val)
		{
			return (val <= std::numeric_limits<To>::max() &&
				val >= std::numeric_limits<To>::min());
		}

		std::optional<std::pair<uint64_t, uint64_t>>
		try_simplify_literal(bytecode &bc, function_dependency_graph &g, uint64_t mv_lit)
		{
			byte *first = bc[mv_lit];

			auto dst_reg = first[1].val;
			byte *lit = &first[2];

			// And add new single instruction
			uint64_t new_op;
			switch (byte_to_op(first->val))
			{
			case op_kind::MV_REG_I64:
			{
				auto val = read_i64(*reinterpret_cast<bytes<8> *>(lit));

				if (fits<uint8_t>(val))
				{
					new_op =
					  bc.add_instruction(mv_lit + op_size(op_kind::MV_REG_I64),
							     make_mv_reg_ui8(dst_reg, val))
					    .ip;
					// Remove the old instruction
					bc.set_instruction(
					  mv_lit,
					  make_nops<ct_op_size<op_kind::MV_REG_I64>::value>());
					g.add_offset(mv_lit + op_size(op_kind::MV_REG_I64),
						     ct_op_size<op_kind::MV_REG_UI8>::value);
				}

				break;
			}
			default: return std::nullopt;
			}

			return std::pair(mv_lit, new_op);
		}

		bool try_remove_mv(bytecode &bc, uint64_t mv)
		{
			byte *b = bc[mv];

			reg dst = (b + 1)->val;
			reg src = (b + 2)->val;

			if (!(dst == src)) return false;

			bc.set_instruction(mv,
					   make_nops<ct_op_size<op_kind::MV64_REG_REG>::value>());

			return true;
		}
	} // namespace peephole_optimizations

	bool optimize_single_ops(program &e, program_dependency_graph &g, optimization_settings &s)
	{
		bool change_made = false;

		for (auto &[fun_id, fun_dg] : g)
		{
			auto &function = e.get_function(fun_id);
			assert(function.is_bytecode());
			auto &bytecode = function.get_bytecode();

			while (true)
			{
				std::optional<uint64_t> old_op;
				// Only has value if the old op is replaced (instead of deleted)
				std::optional<uint64_t> new_op;

				for (int i = 0; i < bytecode.size();)
				{

					auto replacement =
					  peephole_optimizations::try_simplify_literal(
					    bytecode, g[fun_id], i);

					if (replacement)
					{
						old_op = replacement->first;
						new_op = replacement->second;
						break;
					}

					if (byte_to_op(bytecode[i]->val) == op_kind::MV64_REG_REG)
					{
						auto removed =
						  peephole_optimizations::try_remove_mv(bytecode,
											i);

						if (removed)
						{
							old_op = i;
							break;
						}
					}

					i += op_size(byte_to_op(bytecode[i]->val));
				}

				if (!old_op) break;

				change_made = true;

				std::vector<dependency> replacement_deps;

				if (old_op && new_op)
				{
					for (auto &dep : fun_dg.dependencies)
					{
						if (!(dep.instruction_id == *old_op) &&
						    !(dep.depends_on == *old_op))
							continue;

						if (dep.instruction_id == *old_op)
							replacement_deps.push_back(
							  dependency{ *new_op, dep.depends_on });
						else if (dep.depends_on == *old_op)
							replacement_deps.push_back(dependency{
							  dep.instruction_id, *new_op });
					}
				}
				else if (old_op && !new_op)
				{
					std::vector<uint64_t> op_dependencies;
					for (auto &dep : fun_dg.dependencies)
						if (dep.instruction_id == *old_op)
							op_dependencies.push_back(dep.depends_on);

					std::vector<uint64_t> op_dependants;
					for (auto &dep : fun_dg.dependencies)
						if (dep.depends_on == *old_op)
							op_dependants.push_back(dep.instruction_id);

					for (auto dependency : op_dependencies)
						for (auto dependant : op_dependants)
							replacement_deps.push_back(
							  vm::dependency{ dependant, dependency });
				}

				fun_dg.dependencies.erase(
				  std::remove_if(fun_dg.dependencies.begin(),
						 fun_dg.dependencies.end(),
						 [old_op, new_op](auto &dep) {
							 return (*old_op == dep.instruction_id) ||
								(*new_op == dep.depends_on);
						 }),
				  fun_dg.dependencies.end());

				std::move(replacement_deps.begin(), replacement_deps.end(),
					  std::back_inserter(fun_dg.dependencies));
			}
		}

		return change_made;
	}

	/*
	 * Removes operations that have no dependants, and thus no observed side-effects.
	 * E.g. mov r1 r2, with no operation reading the result.
	 * Dependencies of the instruction are removed.
	 * Returns true if any changes to the program were made.
	 */
	bool remove_dependantless_instructions(program &e, program_dependency_graph &g)
	{
		bool change_made = false;
		for (auto i = 0; i < e.get_code().size(); i++)
		{
			auto &fun = e.get_function(i);

			if (!fun.is_bytecode()) continue;
			auto &bytecode = fun.get_bytecode();
			auto &fun_dg = g[i];
			std::vector<dependency> &current_deps = fun_dg.dependencies;

			std::vector<uint64_t> removed_ops;

			for (auto it = bytecode.begin(); it != bytecode.end(); it++)
			{
				auto op = *it;
				auto op_id = op - *bytecode.begin();
				if (byte_to_op(op->val) == op_kind::MV8_REG_REG)
				{
					bool has_dependants = std::any_of(
					  current_deps.begin(), current_deps.end(),
					  [op_id](auto &dep) { return dep.depends_on == op_id; });

					if (has_dependants) continue;

					change_made = true;
					*reinterpret_cast<bytes<3> *>(op) = make_nops<3>();
					removed_ops.push_back(op_id);
				}
			}

			current_deps.erase(
			  std::remove_if(current_deps.begin(), current_deps.end(),
					 [&removed_ops](auto &dep) {
						 return std::find(
							  removed_ops.begin(), removed_ops.end(),
							  dep.instruction_id) != removed_ops.end();
					 }),
			  current_deps.end());
		}

		return change_made;
	}

	void optimize_executable(executable &e, optimization_settings &s) { remove_nops(e, s); }

	void remove_nops(executable &e, optimization_settings &s)
	{
		auto nops_between = [](byte *a, byte *b) {
			byte *min = a > b ? b : a;
			byte *max = b > a ? b : a;
			int count = 0;
			for (auto i = min; i < max;)
			{
				op_kind kind = byte_to_op(i->val);
				if (kind == op_kind::NOP) count++;
				i += op_size(kind);
			}
			return count;
		};

		// Fix jumps
		for (auto *op : e)
		{
			switch (byte_to_op(op->val))
			{
			case op_kind::JMPR_I32:
			{
				byte *jump_bytes = op + 1;
				int32_t jump_offset = read_i32(&jump_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<4> *>(jump_bytes) = make_i32(
					  jump_offset + nops_between(op, op + jump_offset));
				else
					*reinterpret_cast<bytes<4> *>(jump_bytes) = make_i32(
					  jump_offset - nops_between(op, op + jump_offset));
				continue;
			}
			case op_kind::JRNZ_REG_I32:
			case op_kind::JRZ_REG_I32:
			{
				byte *jump_bytes = op + 2;
				int32_t jump_offset = read_i32(&jump_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<4> *>(jump_bytes) = make_i32(
					  jump_offset + nops_between(op, op + jump_offset));
				else
					*reinterpret_cast<bytes<4> *>(jump_bytes) = make_i32(
					  jump_offset - nops_between(op, op + jump_offset));
				continue;
			}
			case op_kind::CALL_UI64_UI8_UI8_UI8:
			{
				byte *address_bytes = op + 1;
				int64_t jump_offset = read_i64(&address_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<8> *>(address_bytes) = make_ui64(
					  jump_offset + nops_between(op, op + jump_offset));
				else
					*reinterpret_cast<bytes<8> *>(address_bytes) = make_ui64(
					  jump_offset - nops_between(op, op + jump_offset));
				continue;
			}
			default: continue;
			}
		}

		// Remove all nops
		int nops_passed = 0;
		for (auto i = 0; i < e.code.data().size();)
		{
			op_kind kind = byte_to_op(e[i]->val);
			size_t size = op_size(kind);

			switch (kind)
			{
			case op_kind::NOP: nops_passed++; break;
			default:
				if (nops_passed > 0)
				{
					// Move current op back as many places as ops we passed
					for (int j = 0; j < size; j++)
					{
						e.code.data()[(i + j) - nops_passed] =
						  e.code.data()[(i + j)];
						e.code.data()[i + j] = 0;
					}
				}
				break;
			}

			i += size;
		}

		e.code.data().resize(e.code.data().size() - nops_passed);
		std::cout << "executable size: " << e.code.data().size() << std::endl;
	}
} // namespace fe::vm