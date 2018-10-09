#include "fe/pipeline/bytecode_optimization_stage.h"
#include "fe/data/bytecode.h"
#include <iostream>
#include <string>
#include <optional>
#include <assert.h>
#include <stack>

namespace fe::vm
{
	void optimize_program(program& e, optimization_settings& s)
	{
		auto dg = build_dependency_graph(e);
		while(remove_redundant_dependencies(e, dg, s));
		auto& funs = e.get_code();
		for (int i = 0; i < funs.size(); i++)
		{
			auto& fun = funs[i];
			if (!fun.is_bytecode()) continue;
			auto& bc = fun.get_bytecode();

			std::cout << "\n" << fun.get_name() << "\n";
			std::string out;
			size_t ip = 0;
			while (bc.has_instruction(ip))
			{
				auto in = bc.get_instruction<10>(ip);
				if (byte_to_op(in[0].val) == op_kind::NOP)
				{
					ip++; continue;
				}

				out += std::to_string(ip) + ": ";
				out += op_to_string(byte_to_op(in[0].val)) + " ";
				for (int i = 1; i < op_size(byte_to_op(in[0].val)); i++)
					out += std::to_string(in[i].val) + " ";

				auto dep = std::find_if(dg.dependencies.at(i).begin(), dg.dependencies.at(i).end(), [ip](dependency& dep) {
					return (ip == dep.instruction_id);
				});

				while (dep != dg.dependencies.at(i).end())
				{
					out += " dep ";
					out += std::to_string(dep->depends_on);

					dep = std::find_if(dep + 1, dg.dependencies.at(i).end(), [ip](dependency& dep) {
						return (ip == dep.instruction_id);
					});
				}

				out += "\n";
				ip += op_size(byte_to_op(in[0].val));
			}

			std::cout << out;
		}

	}

	void dependency_graph::add_offset(function_id fun, uint64_t loc, uint32_t size)
	{
		auto& deps = dependencies[fun];
		for (auto& dep : deps)
		{
			if (dep.depends_on >= loc)
				dep.depends_on += size;
			if (dep.instruction_id >= loc)
				dep.instruction_id += size;
		}
	}

	dependency_graph build_dependency_graph(program& e)
	{
		dependency_graph graph;

		auto& functions = e.get_code();
		for (auto it = functions.begin(); it != functions.end(); it++)
		{
			function_id id = std::distance(functions.begin(), it);
			function& func = *it;
			if (!func.is_bytecode()) continue;

			bytecode& bc = func.get_bytecode();

			// Map of register ids to instruction ids
			// where latest_writes[register id] = instruction id
			uint64_t latest_writes[64] = { 0 };

			// Map of register ids to instruction ids
			std::unordered_map<uint64_t, uint64_t> index_to_op;

			// Vector of generated instruction dependencies
			std::vector<dependency> dependencies;

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
				case op_kind::OR_REG_REG_REG: {
					reg dest = current_instruction[1].val;
					reg src1 = current_instruction[2].val;
					reg src2 = current_instruction[3].val;

					dependencies.push_back(dependency{ i, latest_writes[src1.val] });
					dependencies.push_back(dependency{ i, latest_writes[src2.val] });

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
				case op_kind::MV_REG_I64: {
					reg dest = current_instruction[1].val;

					latest_writes[dest.val] = i;
					break;
				}
				case op_kind::MV8_REG_REG:
				case op_kind::MV16_REG_REG:
				case op_kind::MV32_REG_REG:
				case op_kind::MV64_REG_REG: {
					reg dest = current_instruction[1].val;
					reg src = current_instruction[2].val;

					dependencies.push_back(dependency{ i, latest_writes[src.val] });

					latest_writes[dest.val] = i;
					break;
				}
				case op_kind::MV8_LOC_REG:
				case op_kind::MV16_LOC_REG:
				case op_kind::MV32_LOC_REG:
				case op_kind::MV64_LOC_REG: {
					reg src = current_instruction[2].val;

					dependencies.push_back(dependency{ i, latest_writes[src.val] });
					break;
				}
				case op_kind::MV8_REG_LOC:
				case op_kind::MV16_REG_LOC:
				case op_kind::MV32_REG_LOC:
				case op_kind::MV64_REG_LOC: {
					reg src = current_instruction[1].val;

					latest_writes[src.val] = i;
					break;
				}
				case op_kind::PUSH8_REG:
				case op_kind::PUSH16_REG:
				case op_kind::PUSH32_REG:
				case op_kind::PUSH64_REG: {
					reg src = current_instruction[1].val;

					latest_push_ops.push(i);
					dependencies.push_back(dependency{ i, latest_writes[src.val] });
					break;
				}
				case op_kind::POP8_REG:
				case op_kind::POP16_REG:
				case op_kind::POP32_REG:
				case op_kind::POP64_REG: {
					reg dst = current_instruction[1].val;

					if (!latest_push_ops.empty())
					{
						dependencies.push_back(dependency{ i, latest_push_ops.top() });
						latest_push_ops.pop();
					}
					latest_writes[dst.val] = i;
					break;
				}
				case op_kind::JRNZ_REG_I32:
				case op_kind::JRZ_REG_I32: {
					reg src = current_instruction[1].val;

					latest_push_ops = {};
					dependencies.push_back(dependency{ i, latest_writes[src.val] });
					break;
				}
				case op_kind::CALL_UI64: {
					for (int i = 0; i < 32; i++) latest_writes[i] = 0;

					latest_push_ops = {};
					latest_writes[60] = i;
					dependencies.push_back(dependency{ i, latest_writes[0] });
					break;
				}
				}

				index_to_op.insert({ i, op_i });
				i += size;
			}

			graph.dependencies.insert({ id, dependencies });
		}

		return graph;
	}

	/*
	* Codegen generates several situations where a register is pushed and popped immediately
	* This pass converts this into a mov instruction (or just nops if the src and dest are the same)
	* Returns true if any changes to the program were made
	*/
	bool remove_redundant_dependencies(program& e, dependency_graph& g, optimization_settings& s)
	{
		bool change_made = false;

		for (auto&[fun_id, dependencies] : g.dependencies)
		{
			auto& function = e.get_function(fun_id);
			assert(function.is_bytecode());
			auto& bytecode = function.get_bytecode();

			// first: dependency, second: dependant, second: replacement
			std::vector<std::tuple<uint64_t, uint64_t, uint64_t>> replaced_ops;

			for (auto it = dependencies.begin(); it != dependencies.end(); it++)
			{
				auto& dep = *it;
				auto dependant = dep.instruction_id;
				auto dependency = dep.depends_on;

				auto dependency_instruction = bytecode.get_instruction<10>(dependency);
				op_kind dependency_kind = byte_to_op(dependency_instruction[0].val);
				size_t dependency_op_size = op_size(dependency_kind);

				auto dependant_instruction = bytecode.get_instruction<10>(dependant);
				op_kind dependant_kind = byte_to_op(dependant_instruction[0].val);
				size_t dependant_op_size = op_size(dependant_kind);

				assert(dependency <= dependant);

				// If not compatible pair
				if ((dependency_kind == op_kind::PUSH64_REG && dependant_kind == op_kind::POP64_REG)
					|| (dependency_kind == op_kind::PUSH32_REG && dependant_kind == op_kind::POP32_REG)
					|| (dependency_kind == op_kind::PUSH16_REG && dependant_kind == op_kind::POP16_REG)
					|| (dependency_kind == op_kind::PUSH8_REG && dependant_kind == op_kind::POP8_REG))
				{
					auto first_reg = dependant_instruction[1].val;
					auto second_reg = dependency_instruction[1].val;

					// If there are conflicting instructions in between
					bool conflict = false;
					for (auto i = dependency + dependency_op_size; i < dependant;)
					{
						auto op = bytecode.get_instruction(i);

						if (writes_to(op, second_reg))
						{
							conflict = true;
							break;
						}

						i += op_size(byte_to_op(op->val));
					}
					if (conflict) continue;

					change_made = true;

					// Remove the old instructions
					bytecode.set_instruction(dependency, make_nops<ct_op_size<op_kind::PUSH64_REG>::value>());
					bytecode.set_instruction(dependant, make_nops<ct_op_size<op_kind::POP64_REG>::value>());

					// And add new single instruction
					uint64_t new_op;
					switch (dependency_kind)
					{
					case op_kind::PUSH64_REG: new_op = bytecode.add_instruction(dependant + 2, make_mv64_reg_reg(first_reg, second_reg)).ip; break;
					case op_kind::PUSH32_REG: new_op = bytecode.add_instruction(dependant + 2, make_mv32_reg_reg(first_reg, second_reg)).ip; break;
					case op_kind::PUSH16_REG: new_op = bytecode.add_instruction(dependant + 2, make_mv16_reg_reg(first_reg, second_reg)).ip; break;
					case op_kind::PUSH8_REG: new_op = bytecode.add_instruction(dependant + 2, make_mv8_reg_reg(first_reg, second_reg)).ip; break;
					}

					g.add_offset(fun_id, new_op, 3);

					replaced_ops.push_back({ dependency, dependant, new_op });
				}
				else if ((dependency_kind == op_kind::MV_REG_I64 && dependant_kind == op_kind::MV64_REG_REG
					&& dependency_instruction[1].val == dependant_instruction[2].val))
				{
					auto first_reg = dependant_instruction[2].val;
					auto second_reg = dependency_instruction[1].val;
					auto literal_int = read_i64(&dependency_instruction[2].val);

					// If there are conflicting instructions in between
					bool conflict = false;
					for (auto i = dependency + dependency_op_size; i < dependant;)
					{
						auto op = bytecode.get_instruction(i);

						if (reads_from(op, second_reg))
						{
							conflict = true;
							break;
						}

						i += op_size(byte_to_op(op->val));
					}
					if (conflict) continue;

					change_made = true;

					// Remove the old instructions
					bytecode.set_instruction(dependency, make_nops<ct_op_size<op_kind::MV_REG_I64>::value>());
					bytecode.set_instruction(dependant, make_nops<ct_op_size<op_kind::MV64_REG_REG>::value>());

					// And add new single instruction
					uint64_t new_op = bytecode.add_instruction(
						dependant + ct_op_size<op_kind::MV64_REG_REG>::value, 
						make_mv_reg_i64(dependant_instruction[1].val, literal_int)
					).ip;

					g.add_offset(fun_id, new_op, ct_op_size<op_kind::MV_REG_I64>::value);

					replaced_ops.push_back({ dependency, dependant, new_op });
				}
			}

			std::vector<dependency> added_deps;
			for (auto& dep : dependencies)
			{
				for (auto& replacement_tuple : replaced_ops)
				{
					auto dependency = std::get<0>(replacement_tuple);
					auto dependant = std::get<1>(replacement_tuple);
					auto replacement = std::get<2>(replacement_tuple);

					if (dep.instruction_id == dependant && dep.depends_on == dependency)
						continue;

					assert(dep.depends_on != dependency);
					if (dep.depends_on == dependant)
						added_deps.push_back(vm::dependency{ dep.instruction_id, replacement });
					else if (dep.instruction_id == dependency)
						added_deps.push_back(vm::dependency{ replacement, dep.depends_on });
				}
			}

			dependencies.erase(std::remove_if(dependencies.begin(), dependencies.end(), [replaced_ops](auto& dep) {
				for (auto& replacement : replaced_ops)
				{
					if (std::get<0>(replacement) == dep.instruction_id
						|| std::get<1>(replacement) == dep.depends_on
						|| std::get<1>(replacement) == dep.instruction_id
						|| std::get<1>(replacement) == dep.depends_on)
					{
						return true;
					}
				}

				return false;
			}), dependencies.end());


			std::move(added_deps.begin(), added_deps.end(), std::back_inserter(dependencies));
		}

		return change_made;
	}

	void optimize_executable(executable& e, optimization_settings& s)
	{
		remove_nops(e, s);
		std::cout << "Full code size: " << e.code.size() << " bytes\n";
	}

	void remove_nops(executable& e, optimization_settings& s)
	{
		auto nops_between = [](byte* a, byte* b) {
			byte* min = a > b ? b : a;
			byte* max = b > a ? b : a;
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
		for (auto* op : e)
		{
			switch (byte_to_op(op->val))
			{
			case op_kind::JMPR_I32: {
				byte* jump_bytes = op + 1;
				int32_t jump_offset = read_i32(&jump_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset + nops_between(op, op + jump_offset));
				else
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset - nops_between(op, op + jump_offset));
				continue;
			}
			case op_kind::JRNZ_REG_I32:
			case op_kind::JRZ_REG_I32: {
				byte* jump_bytes = op + 2;
				int32_t jump_offset = read_i32(&jump_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset + nops_between(op, op + jump_offset));
				else
					*reinterpret_cast<bytes<4>*>(jump_bytes) = make_i32(jump_offset - nops_between(op, op + jump_offset));
				continue;
			}
			case op_kind::CALL_UI64: {
				byte* address_bytes = op + 1;
				int64_t jump_offset = read_i64(&address_bytes->val);
				if (jump_offset < 0)
					*reinterpret_cast<bytes<8>*>(address_bytes) = make_ui64(jump_offset + nops_between(op, op + jump_offset));
				else
					*reinterpret_cast<bytes<8>*>(address_bytes) = make_ui64(jump_offset - nops_between(op, op + jump_offset));
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
						e.code.data()[(i + j) - nops_passed] = e.code.data()[(i + j)];
						e.code.data()[i + j] = 0;
					}
				}
				break;
			}

			i += size;
		}

		e.code.data().resize(e.code.data().size() - nops_passed);
	}
}