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

	std::string op_to_string(op_kind o)
	{
		switch (o)
		{
		case op_kind::NOP: return "nop";
		case op_kind::ADD_REG_REG_REG: return "add_reg_reg_reg";
		case op_kind::ADD_REG_REG_UI8: return "add_reg_reg_ui8";
		case op_kind::SUB_REG_REG_REG: return "sub_reg_reg_reg";
		case op_kind::SUB_REG_REG_UI8: return "sub_reg_reg_ui8";
		case op_kind::MUL_REG_REG_REG: return "mul_reg_reg_reg";
		case op_kind::DIV_REG_REG_REG: return "div_reg_reg_reg";
		case op_kind::MOD_REG_REG_REG: return "mod_reg_reg_reg";
		case op_kind::GT_REG_REG_REG: return "gt_reg_reg_reg";
		case op_kind::GTE_REG_REG_REG: return "gte_reg_reg_reg";
		case op_kind::EQ_REG_REG_REG: return "eq_reg_reg_reg";
		case op_kind::NEQ_REG_REG_REG: return "neq_reg_reg_reg";
		case op_kind::AND_REG_REG_REG: return "and_reg_reg_reg";
		case op_kind::AND_REG_REG_UI8: return "and_reg_reg_ui8";
		case op_kind::OR_REG_REG_REG: return "or_reg_reg_reg";
		case op_kind::MV_REG_SP: return "mv_reg_sp";
		case op_kind::MV_REG_IP: return "mv_reg_ip";
		case op_kind::MV_REG_UI8: return "mv_reg_ui8";
		case op_kind::MV_REG_UI16: return "mv_reg_ui16";
		case op_kind::MV_REG_UI32: return "mv_reg_ui32";
		case op_kind::MV_REG_UI64: return "mv_reg_ui64";
		case op_kind::MV_REG_I8: return "mv_reg_i8";
		case op_kind::MV_REG_I16: return "mv_reg_i16";
		case op_kind::MV_REG_I32: return "mv_reg_i32";
		case op_kind::MV_REG_I64: return "mv_reg_i64";
		case op_kind::MV8_REG_REG: return "mv8_reg_reg";
		case op_kind::MV16_REG_REG: return "mv16_reg_reg";
		case op_kind::MV32_REG_REG: return "mv32_reg_reg";
		case op_kind::MV64_REG_REG: return "mv64_reg_reg";
		case op_kind::MV8_LOC_REG: return "mv8_loc_reg";
		case op_kind::MV16_LOC_REG: return "mv16_loc_reg";
		case op_kind::MV32_LOC_REG: return "mv32_loc_reg";
		case op_kind::MV64_LOC_REG: return "mv64_loc_reg";
		case op_kind::MV8_REG_LOC: return "mv8_reg_loc";
		case op_kind::MV16_REG_LOC: return "mv16_reg_loc";
		case op_kind::MV32_REG_LOC: return "mv32_reg_loc";
		case op_kind::MV64_REG_LOC: return "mv64_reg_loc";
		case op_kind::PUSH8_REG: return "push8_reg";
		case op_kind::PUSH16_REG: return "push16_reg";
		case op_kind::PUSH32_REG: return "push32_reg";
		case op_kind::PUSH64_REG: return "push64_reg";
		case op_kind::POP8_REG: return "pop8_reg";
		case op_kind::POP16_REG: return "pop16_reg";
		case op_kind::POP32_REG: return "pop32_reg";
		case op_kind::POP64_REG: return "pop64_reg";
		case op_kind::LBL_UI32: return "lbl_ui32";
		case op_kind::JMPR_I32: return "jmpr_i32";
		case op_kind::JRNZ_REG_I32: return "jrnz_reg_i32";
		case op_kind::JRZ_REG_I32: return "jrz_reg_i32";
		case op_kind::CALL_UI64: return "call_ui64";
		case op_kind::RET_UI8: return "ret_ui8";
		}
		assert(!"Unknown instruction");
	}

	uint8_t op_size(op_kind o)
	{
		switch (o)
		{
		case op_kind::NOP: return 1;
		case op_kind::ADD_REG_REG_REG: return 4;
		case op_kind::ADD_REG_REG_UI8: return 4;
		case op_kind::SUB_REG_REG_REG: return 4;
		case op_kind::SUB_REG_REG_UI8: return 4;
		case op_kind::MUL_REG_REG_REG: return 4;
		case op_kind::DIV_REG_REG_REG: return 4;
		case op_kind::MOD_REG_REG_REG: return 4;
		case op_kind::GT_REG_REG_REG: return 4;
		case op_kind::GTE_REG_REG_REG: return 4;
		case op_kind::EQ_REG_REG_REG: return 4;
		case op_kind::NEQ_REG_REG_REG: return 4;
		case op_kind::AND_REG_REG_REG: return 4;
		case op_kind::AND_REG_REG_UI8: return 4;
		case op_kind::OR_REG_REG_REG: return 4;
		case op_kind::MV_REG_SP: return 2;
		case op_kind::MV_REG_IP: return 2;
		case op_kind::MV_REG_UI8: return 3;
		case op_kind::MV_REG_UI16: return 4;
		case op_kind::MV_REG_UI32: return 6;
		case op_kind::MV_REG_UI64: return 10;
		case op_kind::MV_REG_I8: return 3;
		case op_kind::MV_REG_I16: return 4;
		case op_kind::MV_REG_I32: return 6;
		case op_kind::MV_REG_I64: return 10;
		case op_kind::MV8_REG_REG: return 3;
		case op_kind::MV16_REG_REG: return 3;
		case op_kind::MV32_REG_REG: return 3;
		case op_kind::MV64_REG_REG: return 3;
		case op_kind::MV8_LOC_REG: return 3;
		case op_kind::MV16_LOC_REG: return 3;
		case op_kind::MV32_LOC_REG: return 3;
		case op_kind::MV64_LOC_REG: return 3;
		case op_kind::MV8_REG_LOC: return 3;
		case op_kind::MV16_REG_LOC: return 3;
		case op_kind::MV32_REG_LOC: return 3;
		case op_kind::MV64_REG_LOC: return 3;
		case op_kind::PUSH8_REG: return 2;
		case op_kind::PUSH16_REG: return 2;
		case op_kind::PUSH32_REG: return 2;
		case op_kind::PUSH64_REG: return 2;
		case op_kind::POP8_REG: return 2;
		case op_kind::POP16_REG: return 2;
		case op_kind::POP32_REG: return 2;
		case op_kind::POP64_REG: return 2;
		case op_kind::LBL_UI32: return 5;
		case op_kind::JMPR_I32: return 5;
		case op_kind::JRNZ_REG_I32: return 6;
		case op_kind::JRZ_REG_I32: return 6;
		case op_kind::CALL_UI64: return 9;
		case op_kind::RET_UI8: return 2;
		}
		assert(!"Unknown instruction");
	}

	bytes<8> make_i64(int64_t a)
	{
		return bytes<8> {
			static_cast<uint8_t>((a & 0xFF00000000000000) >> 56),
				static_cast<uint8_t>((a & 0xFF000000000000) >> 48),
				static_cast<uint8_t>((a & 0xFF0000000000) >> 40),
				static_cast<uint8_t>((a & 0xFF00000000) >> 32),
				static_cast<uint8_t>((a & 0xFF000000) >> 24),
				static_cast<uint8_t>((a & 0xFF0000) >> 16),
				static_cast<uint8_t>((a & 0xFF00) >> 8),
				static_cast<uint8_t>(a & 0xFF)
		};
	}
	int64_t read_i64(bytes<8> in)
	{
		return (static_cast<int64_t>(in[0].val) << 56) |
			(static_cast<int64_t>(in[1].val) << 48) |
			(static_cast<int64_t>(in[2].val) << 40) |
			(static_cast<int64_t>(in[3].val) << 32) |
			(static_cast<int64_t>(in[4].val) << 24) |
			(static_cast<int64_t>(in[5].val) << 16) |
			(static_cast<int64_t>(in[6].val) << 8) |
			in[7].val;
	}
	bytes<8> make_ui64(uint64_t a) { return make_i64(static_cast<int64_t>(a)); }
	uint64_t read_ui64(bytes<8> in) { return static_cast<uint64_t>(read_i64(in)); }
	bytes<4> make_i32(int32_t a)
	{
		return bytes<4> {
			static_cast<uint8_t>((a & 0xFF000000) >> 24),
				static_cast<uint8_t>((a & 0xFF0000) >> 16),
				static_cast<uint8_t>((a & 0xFF00) >> 8),
				static_cast<uint8_t>((a & 0xFF))
		};
	}
	int32_t read_i32(bytes<4> in)
	{
		return (static_cast<int32_t>(in[0].val) << 24) |
			(static_cast<int32_t>(in[1].val) << 16) |
			(static_cast<int32_t>(in[2].val) << 8) |
			in[3].val;
	}
	bytes<4> make_ui32(uint32_t a) { return make_i32(static_cast<int32_t>(a)); }
	uint32_t read_ui32(bytes<4> in) { return static_cast<uint32_t>(read_i32(in)); }
	bytes<2> make_i16(int16_t a)
	{ 
		return bytes<2> {
			static_cast<uint8_t>((a & 0xFF00) >> 8),
			static_cast<uint8_t>((a & 0xFF))
		};
	}
	int16_t read_i16(bytes<2> in)
	{
		return (static_cast<int16_t>(in[0].val) << 8) |
			in[1].val;
	}
	bytes<2> make_ui16(uint16_t a) { return make_i16(static_cast<int16_t>(a)); }
	uint16_t read_ui16(bytes<2> in) { return static_cast<int16_t>(read_i16(in)); }
	bytes<1> make_ui8(uint8_t a) 
	{
		return bytes<1>{a};
	}
	uint8_t read_ui8(bytes<1> in)
	{
		return in[0].val;
	}
	bytes<1> make_i8(int8_t a) { return make_ui8(static_cast<uint8_t>(a)); }
	int8_t read_i8(bytes<1> in) { return static_cast<int8_t>(read_ui8(in)); }



	bytes<1> make_nop()
	{
		return bytes<1>{ op_to_byte(op_kind::NOP) };
	}
	bytes<4> make_add(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::ADD_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_add(reg dest, reg a, byte b)
	{
		return bytes<4>{ op_to_byte(op_kind::ADD_REG_REG_UI8), dest.val, a.val, b.val };
	}
	bytes<4> make_sub(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::SUB_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_sub(reg dest, reg a, byte b)
	{
		return bytes<4>{op_to_byte(op_kind::SUB_REG_REG_UI8), dest.val, a.val, b.val};
	}
	bytes<4> make_mul(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::MUL_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_div(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::DIV_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_mod(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::MOD_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_and(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::AND_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_and(reg dest, reg a, byte b)
	{
		return bytes<4>{ op_to_byte(op_kind::AND_REG_REG_UI8), dest.val, a.val, b.val };
	}
	bytes<4> make_or(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::OR_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_gt(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::GT_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_gte(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::GTE_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_eq(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::EQ_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_neq(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::NEQ_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<3> make_mv64(reg dest, reg src)
	{
		return bytes<3>{ op_to_byte(op_kind::MV64_REG_REG), dest.val, src.val };
	}
	bytes<2> make_mv_sp(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::MV_REG_SP), dest.val };
	}
	bytes<3> make_mv_reg_ui8(reg dest, uint8_t a)
	{
		return bytes<3>{ op_to_byte(op_kind::MV_REG_UI8), dest.val, a };
	}
	bytes<4> make_mv_reg_ui16(reg dest, uint16_t a)
	{
		return bytes<4>{
			op_to_byte(op_kind::MV_REG_UI16),
				dest.val,
				// #todo replace with make_ui16
				static_cast<uint8_t>((a & 0xFF00) >> 8),
				static_cast<uint8_t>(a & 0xFF),
		};
	}
	bytes<6> make_mv_reg_ui32(reg dest, uint32_t a)
	{
		return bytes<6>{
			op_to_byte(op_kind::MV_REG_UI32),
				dest.val,
				// #todo replace with make_ui32
				static_cast<uint8_t>((a & 0xFF000000) >> 24),
				static_cast<uint8_t>((a & 0xFF0000) >> 16),
				static_cast<uint8_t>((a & 0xFF00) >> 8),
				static_cast<uint8_t>(a & 0xFF),
		};
	}
	bytes<10> make_mv_reg_ui64(reg dest, uint64_t a)
	{
		return bytes<10>{
			op_to_byte(op_kind::MV_REG_UI64),
				dest.val,
				// #todo replace with make_ui64
				static_cast<uint8_t>((a & 0xFF00000000000000) >> 56),
				static_cast<uint8_t>((a & 0xFF000000000000) >> 48),
				static_cast<uint8_t>((a & 0xFF0000000000) >> 40),
				static_cast<uint8_t>((a & 0xFF00000000) >> 32),
				static_cast<uint8_t>((a & 0xFF000000) >> 24),
				static_cast<uint8_t>((a & 0xFF0000) >> 16),
				static_cast<uint8_t>((a & 0xFF00) >> 8),
				static_cast<uint8_t>(a & 0xFF),
		};
	}
	bytes<3> make_mv_reg_i8(reg dest, int8_t a)
	{
		return bytes<3>{ op_to_byte(op_kind::MV_REG_I8), dest.val, static_cast<uint8_t>(a) };
	}
	bytes<4> make_mv_reg_i16(reg dest, int16_t a)
	{
		return bytes<4>{ op_to_byte(op_kind::MV_REG_I16), dest.val, static_cast<uint8_t>((a & 0xFF00) >> 8), static_cast<uint8_t>(a & 0xFF) };
	}
	bytes<6> make_mv_reg_i32(reg dest, int32_t a)
	{
		assert(!"nyi");
		return bytes<6>();
	}
	bytes<10> make_mv_reg_i64(reg dest, int64_t a)
	{
		auto lit = make_i64(a);
		return bytes<10>{op_to_byte(op_kind::MV_REG_I64), dest.val, lit[0], lit[1], lit[2], lit[3], lit[4], lit[5], lit[6], lit[7] };
	}
	bytes<3> make_mv_reg_reg(uint8_t bytes, reg dest, reg a)
	{
		switch (bytes)
		{
		case 1: return make_mv8_reg_reg(dest, a);
		case 2: return make_mv16_reg_reg(dest, a);
		case 4: return make_mv32_reg_reg(dest, a);
		case 8: return make_mv64_reg_reg(dest, a);
		default: assert(!"Invalid push bit count");
		}
	}
	bytes<3> make_mv8_reg_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV8_REG_REG), dest.val, src.val };
	}
	bytes<3> make_mv16_reg_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV16_REG_REG), dest.val, src.val };
	}
	bytes<3> make_mv32_reg_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV32_REG_REG), dest.val, src.val };
	}
	bytes<3> make_mv64_reg_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV64_REG_REG), dest.val, src.val };
	}
	bytes<3> make_mv_reg_loc(uint8_t bytes, reg dest, reg src)
	{
		switch (bytes)
		{
		case 1: return make_mv8_reg_loc(dest, src);
		case 2: return make_mv16_reg_loc(dest, src);
		case 4: return make_mv32_reg_loc(dest, src);
		case 8: return make_mv64_reg_loc(dest, src);
		default: assert(!"Invalid push bit count");
		}
	}
	bytes<3> make_mv8_reg_loc(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV8_REG_LOC), dest.val, src.val};
	}
	bytes<3> make_mv16_reg_loc(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV16_REG_LOC), dest.val, src.val};
	}
	bytes<3> make_mv32_reg_loc(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV32_REG_LOC), dest.val, src.val};
	}
	bytes<3> make_mv64_reg_loc(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV64_REG_LOC), dest.val, src.val};
	}
	bytes<3> make_mv_loc_reg(uint8_t bytes, reg dest, reg src)
	{
		switch (bytes)
		{
		case 1: return make_mv8_loc_reg(dest, src);
		case 2: return make_mv16_loc_reg(dest, src);
		case 4: return make_mv32_loc_reg(dest, src);
		case 8: return make_mv64_loc_reg(dest, src);
		default: assert(!"Invalid push bit count");
		}
	}
	bytes<3> make_mv8_loc_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV8_LOC_REG), dest.val, src.val};
	}
	bytes<3> make_mv16_loc_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV16_LOC_REG), dest.val, src.val};
	}
	bytes<3> make_mv32_loc_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV32_LOC_REG), dest.val, src.val};
	}
	bytes<3> make_mv64_loc_reg(reg dest, reg src)
	{
		return bytes<3>{op_to_byte(op_kind::MV64_LOC_REG), dest.val, src.val};
	}
	bytes<2> make_push(uint8_t bytes, reg src)
	{
		switch (bytes)
		{
		case 1: return make_push8(src);
		case 2: return make_push16(src);
		case 4: return make_push32(src);
		case 8: return make_push64(src);
		default: assert(!"Invalid push bit count");
		}
	}
	bytes<2> make_push8(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::PUSH8_REG), dest.val };
	}
	bytes<2> make_push16(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::PUSH16_REG), dest.val };
	}
	bytes<2> make_push32(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::PUSH32_REG), dest.val };
	}
	bytes<2> make_push64(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::PUSH64_REG), dest.val };
	}
	bytes<2> make_pop(uint8_t bits, reg src)
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
	bytes<2> make_pop8(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::POP8_REG), dest.val };
	}
	bytes<2> make_pop16(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::POP16_REG), dest.val };
	}
	bytes<2> make_pop32(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::POP32_REG), dest.val };
	}
	bytes<2> make_pop64(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::POP64_REG), dest.val };
	}
	bytes<9> make_call_ui64(uint64_t ip)
	{
		auto to = make_ui64(ip);
		return bytes<9>{ op_to_byte(op_kind::CALL_UI64), to[0], to[1], to[2], to[3], to[4], to[5], to[6], to[7] };
	}
	bytes<2> make_ret(byte a)
	{
		return bytes<2>{ op_to_byte(op_kind::RET_UI8), a.val };
	}
	bytes<5> make_jmpr_i32(int32_t dest)
	{
		auto to = make_i32(dest);
		return bytes<5>{ op_to_byte(op_kind::JMPR_I32), to[0], to[1], to[2], to[3] };
	}
	bytes<6> make_jrnz_i32(reg a, int32_t dest)
	{
		auto to = make_i32(dest);
		return bytes<6>{ op_to_byte(op_kind::JRNZ_REG_I32), a.val, to[0], to[1], to[2], to[3] };
	}
	bytes<6> make_jrz_i32(reg a, int32_t dest)
	{
		auto to = make_i32(dest);
		return bytes<6>{ op_to_byte(op_kind::JRZ_REG_I32), a.val, to[0], to[1], to[2], to[3] };
	}

	bytes<5> make_lbl(uint32_t id)
	{
		auto lit = make_ui32(id);
		return bytes<5>{op_to_byte(op_kind::LBL_UI32), lit[0], lit[1], lit[2], lit[3] };
	}

	/*
	* Bytecode
	*/

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
			auto in = get_instruction<10>(ip);
			out += std::to_string(ip) + ": ";
			out += op_to_string(byte_to_op(in[0].val)) + " ";
			for (int i = 1; i < op_size(byte_to_op(in[0].val)); i++)
				out += std::to_string(in[i].val) + " ";
			out += "\n";
			ip += op_size(byte_to_op(in[0].val));
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

	size_t program::chunk_count()
	{
		return chunks.size();
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