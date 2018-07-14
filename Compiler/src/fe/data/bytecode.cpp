#include "fe/data/bytecode.h"
#include <assert.h>
#include <iostream>
#include <string>

namespace fe::vm
{
	uint8_t op_to_byte(op_kind o)
	{
		return static_cast<uint8_t>(o);
	}

	op_kind byte_to_op(uint8_t b)
	{
		return static_cast<op_kind>(b);
	}

	single_byte make_nop()
	{
		return single_byte{ op_to_byte(op_kind::NOP) };
	}
	quad_byte make_add(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::ADD_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_sub(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::SUB_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_mul(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::MUL_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_div(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::DIV_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_mod(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::MOD_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_and(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::AND_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_or(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::OR_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_gt(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::GT_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_gte(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::GTE_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_eq(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::EQ_REG_REG_REG), dest.val, a.val, b.val };
	}
	quad_byte make_neq(reg dest, reg a, reg b)
	{
		return quad_byte{ op_to_byte(op_kind::NEQ_REG_REG_REG), dest.val, a.val, b.val };
	}
	triple_byte make_jz(reg a, reg ip)
	{
		return triple_byte{ op_to_byte(op_kind::JZ_REG_REG), a.val, ip.val };
	}
	triple_byte make_load64(reg dest, reg src)
	{
		return triple_byte{ op_to_byte(op_kind::LOAD64_REG_REG), dest.val, src.val };
	}
	double_byte make_load_sp(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::LOAD_SP_REG), dest.val };
	}
	triple_byte make_mv(reg dest, byte a)
	{
		return triple_byte{ op_to_byte(op_kind::MV_REG_UI8), dest.val, a };
	}
	double_byte make_push(uint8_t bits, reg src)
	{
		switch (bits)
		{
		case 1: return make_push8(src);
		case 2: return make_push16(src);
		case 4: return make_push32(src);
		case 8: return make_push64(src);
		default: assert(!"Invalid push bit count");
		}
	}
	double_byte make_push8(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::PUSH8_REG), dest.val };
	}
	double_byte make_push16(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::PUSH16_REG), dest.val };
	}
	double_byte make_push32(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::PUSH32_REG), dest.val };
	}
	double_byte make_push64(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::PUSH64_REG), dest.val };
	}
	double_byte make_pop(uint8_t bits, reg src)
	{
		switch (bits)
		{
		case 1: return make_pop8(src);
		case 2: return make_pop16(src);
		case 4: return make_pop32(src);
		case 8: return make_pop64(src);
		default: assert(!"Invalid push bit count");
		}	
	}
	double_byte make_pop8(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::POP8_REG), dest.val };
	}
	double_byte make_pop16(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::POP16_REG), dest.val };
	}
	double_byte make_pop32(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::POP32_REG), dest.val };
	}
	double_byte make_pop64(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::POP64_REG), dest.val };
	}
	triple_byte make_call(reg chunk, reg ip)
	{
		return triple_byte{ op_to_byte(op_kind::CALL_REG_REG), chunk.val, ip.val };
	}
	triple_byte make_ret(byte a, byte b)
	{
		return triple_byte{ op_to_byte(op_kind::RET_UI8_UI8), a.val, b.val };
	}
	double_byte make_jmp(reg dest)
	{
		return double_byte{ op_to_byte(op_kind::JMP_REG), dest.val };
	}


	/*
	* Bytecode
	*/

	near_lbl bytecode::add_instruction(any_byte b)
	{
		near_lbl res = static_cast<uint8_t>(instructions.size());
		if (std::holds_alternative<single_byte>(b))
		{
			auto sb = std::get<single_byte>(b);
			instructions.push_back(sb[0]);
		}
		else if (std::holds_alternative<double_byte>(b))
		{
			auto db = std::get<double_byte>(b);
			instructions.push_back(db[0]);
			instructions.push_back(db[1]);
		}
		else if (std::holds_alternative<triple_byte>(b))
		{
			auto tb = std::get<triple_byte>(b);
			instructions.push_back(tb[0]);
			instructions.push_back(tb[1]);
			instructions.push_back(tb[2]);
		}
		else if (std::holds_alternative<quad_byte>(b))
		{
			auto qb = std::get<quad_byte>(b);
			instructions.push_back(qb[0]);
			instructions.push_back(qb[1]);
			instructions.push_back(qb[2]);
			instructions.push_back(qb[3]);
		}
		return res;
	}

	near_lbl bytecode::add_instructions(std::vector<any_byte> in)
	{
		assert(in.size() > 0);
		auto res = add_instruction(in[0]);
		for (auto it = in.begin() + 1; it != in.end(); it++) add_instruction(*it);
		return res;
	}

	quad_byte bytecode::get_instruction(near_lbl i) const
	{
		assert(has_instruction(i));

		if (has_instruction(i + 3))
			return { instructions[i.ip], instructions[i.ip + 1], instructions[i.ip + 2], instructions[i.ip + 3] };
		else if (has_instruction(i + 2))
			return { instructions[i.ip], instructions[i.ip + 1], instructions[i.ip + 2], 0 };
		else if (has_instruction(i + 1))
			return { instructions[i.ip], instructions[i.ip + 1], 0, 0 };
		else
			return { instructions[i.ip], 0, 0, 0 };
	}

	void bytecode::set_instruction(near_lbl i, any_byte b)
	{
		assert(has_instruction(i));
		if (std::holds_alternative<single_byte>(b))
		{
			auto sb = std::get<single_byte>(b);
			instructions.at(i.ip) = sb[0];
		}
		else if (std::holds_alternative<double_byte>(b))
		{
			auto db = std::get<double_byte>(b);
			instructions.at(i.ip) = db[0];
			instructions.at(i.ip + 1) = db[1];
		}
		else if (std::holds_alternative<triple_byte>(b))
		{
			auto tb = std::get<triple_byte>(b);
			instructions.at(i.ip) = tb[0];
			instructions.at(i.ip + 1) = tb[1];
			instructions.at(i.ip + 2) = tb[2];
		}
		else if (std::holds_alternative<quad_byte>(b))
		{
			auto qb = std::get<quad_byte>(b);
			instructions.at(i.ip) = qb[0];
			instructions.at(i.ip + 1) = qb[1];
			instructions.at(i.ip + 2) = qb[2];
			instructions.at(i.ip + 3) = qb[3];
		}
	}

	bool bytecode::has_instruction(near_lbl i) const
	{
		return i.ip < instructions.size();
	}

	bytecode::operator std::string() const
	{
		std::string out;
		size_t ip = 0;
		while (has_instruction(ip))
		{
			auto qb = get_instruction(ip);
			out += std::to_string(ip) + ": ";
			switch (byte_to_op(qb[0].val))
			{
			case op_kind::ADD_REG_REG_REG: ip += 4; out += "add\n"; break;
			case op_kind::SUB_REG_REG_REG: ip += 4; out += "sub\n"; break;
			case op_kind::MUL_REG_REG_REG: ip += 4; out += "mul\n"; break;
			case op_kind::MOD_REG_REG_REG: ip += 4; out += "mod\n"; break;
			case op_kind::GT_REG_REG_REG: ip += 4; out += "gt\n"; break;
			case op_kind::GTE_REG_REG_REG: ip += 4; out += "gte\n"; break;
			case op_kind::EQ_REG_REG_REG: ip += 4; out += "eq\n"; break;
			case op_kind::NEQ_REG_REG_REG: ip += 4; out += "neq\n"; break;
			case op_kind::AND_REG_REG_REG: ip += 4; out += "and\n"; break;
			case op_kind::OR_REG_REG_REG: ip += 4; out += "or\n"; break;
			case op_kind::LOAD64_REG_REG: ip += 3; out += "load64\n"; break;
			case op_kind::LOAD_SP_REG: ip += 2; out += "loadsp\n"; break;
			case op_kind::MV_REG_UI8: ip += 3; out += "mv\n"; break;
			case op_kind::PUSH8_REG: ip += 2; out += "push8\n"; break;
			case op_kind::PUSH16_REG: ip += 2; out += "push16\n"; break;
			case op_kind::PUSH32_REG: ip += 2; out += "push32\n"; break;
			case op_kind::PUSH64_REG: ip += 2; out += "push64\n"; break;
			case op_kind::POP8_REG: ip += 2; out += "pop8\n"; break;
			case op_kind::POP16_REG: ip += 2; out += "pop16\n"; break;
			case op_kind::POP32_REG: ip += 2; out += "pop32\n"; break;
			case op_kind::POP64_REG: ip += 2; out += "pop64\n"; break;
			case op_kind::JMP_REG: ip += 2; out += "jmp\n"; break;
			case op_kind::JNZ_REG_REG: ip += 3; out += "jnz\n"; break;
			case op_kind::JZ_REG_REG: ip += 3; out += "jz\n"; break;
			case op_kind::CALL_REG_REG: ip += 3; out += "call\n"; break;
			case op_kind::RET_UI8_UI8: ip += 3; out += "ret\n"; break;
			case op_kind::NOP: ip += 1; out += "nop\n"; break;
			case op_kind::LBL: ip += 1; out += "lbl\n"; break;
			default:
				throw std::runtime_error("Unknown instruction");
			}
		}
		return out;
	}

	/*
	* Program
	*/

	uint8_t program::add_chunk(bytecode bc)
	{
		chunks.push_back(std::move(bc));
		return chunks.size() - 1;
	}

	bytecode& program::get_chunk(uint8_t i)
	{
		assert(i < chunks.size());
		return chunks.at(i);
	}

	size_t program::nr_of_chunks()
	{
		return chunks.size();
	}

	quad_byte program::get_instruction(far_lbl l)
	{
		return get_chunk(l.chunk_id).get_instruction(l.ip);
	}

	void program::print()
	{
		for (auto& chunk : this->chunks)
		{
			std::cout << "chunk" << std::endl;
			auto s = chunk.operator std::string();
			std::cout << s << std::endl;
		}
	}
}