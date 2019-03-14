#include "bytecode.h"
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
		case op_kind::LT_REG_REG_REG: return "lt_reg_reg_reg";
		case op_kind::LTE_REG_REG_REG: return "lte_reg_reg_reg";
		case op_kind::LTE_REG_REG_I8: return "lte_reg_reg_i8";
		case op_kind::EQ_REG_REG_REG: return "eq_reg_reg_reg";
		case op_kind::NEQ_REG_REG_REG: return "neq_reg_reg_reg";
		case op_kind::AND_REG_REG_REG: return "and_reg_reg_reg";
		case op_kind::AND_REG_REG_UI8: return "and_reg_reg_ui8";
		case op_kind::OR_REG_REG_REG: return "or_reg_reg_reg";
		case op_kind::XOR_REG_REG_UI8: return "xor_reg_reg_ui8";
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
		case op_kind::CALL_NATIVE_UI64: return "call_native_ui64";
		case op_kind::CALL_REG: return "call_reg";
		case op_kind::RET_UI8: return "ret_ui8";
		case op_kind::SALLOC_REG_UI8: return "salloc_reg_ui8";
		case op_kind::SDEALLOC_UI8: return "sdealloc_ui8";
		case op_kind::EXIT: return "exit";
		}
		assert(!"Unknown instruction");
	}

	op_kind string_to_op(const std::string& o)
	{
		if(o == "nop") return op_kind::NOP;
		if(o == "add_reg_reg_reg") return op_kind::ADD_REG_REG_REG;
		if(o == "add_reg_reg_ui8") return op_kind::ADD_REG_REG_UI8;
		if(o == "sub_reg_reg_reg") return op_kind::SUB_REG_REG_REG;
		if(o == "sub_reg_reg_ui8") return op_kind::SUB_REG_REG_UI8;
		if(o == "mul_reg_reg_reg") return op_kind::MUL_REG_REG_REG;
		if(o == "div_reg_reg_reg") return op_kind::DIV_REG_REG_REG;
		if(o == "mod_reg_reg_reg") return op_kind::MOD_REG_REG_REG;
		if(o == "gt_reg_reg_reg") return op_kind::GT_REG_REG_REG;
		if(o == "gte_reg_reg_reg") return op_kind::GTE_REG_REG_REG;
		if(o == "lt_reg_reg_reg") return op_kind::LT_REG_REG_REG;
		if(o == "lte_reg_reg_reg") return op_kind::LTE_REG_REG_REG;
		if(o == "lte_reg_reg_i8") return op_kind::LTE_REG_REG_I8;
		if(o == "eq_reg_reg_reg") return op_kind::EQ_REG_REG_REG;
		if(o == "neq_reg_reg_reg") return op_kind::NEQ_REG_REG_REG;
		if(o == "and_reg_reg_reg") return op_kind::AND_REG_REG_REG;
		if(o == "and_reg_reg_ui8") return op_kind::AND_REG_REG_UI8;
		if(o == "or_reg_reg_reg") return op_kind::OR_REG_REG_REG;
		if(o == "xor_reg_reg_ui8") return op_kind::XOR_REG_REG_UI8;
		if(o == "mv_reg_sp") return op_kind::MV_REG_SP;
		if(o == "mv_reg_ip") return op_kind::MV_REG_IP;
		if(o == "mv_reg_ui8") return op_kind::MV_REG_UI8;
		if(o == "mv_reg_ui16") return op_kind::MV_REG_UI16;
		if(o == "mv_reg_ui32") return op_kind::MV_REG_UI32;
		if(o == "mv_reg_ui64") return op_kind::MV_REG_UI64;
		if(o == "mv_reg_i8") return op_kind::MV_REG_I8;
		if(o == "mv_reg_i16") return op_kind::MV_REG_I16;
		if(o == "mv_reg_i32") return op_kind::MV_REG_I32;
		if(o == "mv_reg_i64") return op_kind::MV_REG_I64;
		if(o == "mv8_reg_reg") return op_kind::MV8_REG_REG;
		if(o == "mv16_reg_reg") return op_kind::MV16_REG_REG;
		if(o == "mv32_reg_reg") return op_kind::MV32_REG_REG;
		if(o == "mv64_reg_reg") return op_kind::MV_REG_UI8;
		if(o == "mv8_loc_reg") return op_kind::MV_REG_UI8;
		if(o == "mv16_loc_reg") return op_kind::MV16_LOC_REG;
		if(o == "mv32_loc_reg") return op_kind::MV32_LOC_REG;
		if(o == "mv64_loc_reg") return op_kind::MV64_LOC_REG;
		if(o == "mv8_reg_loc") return op_kind::MV8_REG_LOC;
		if(o == "mv16_reg_loc") return op_kind::MV16_REG_LOC;
		if(o == "mv32_reg_loc") return op_kind::MV32_REG_LOC;
		if(o == "mv64_reg_loc") return op_kind::MV64_REG_LOC;
		if(o == "push8_reg") return op_kind::PUSH8_REG;
		if(o == "push16_reg") return op_kind::PUSH16_REG;
		if(o == "push32_reg") return op_kind::PUSH32_REG;
		if(o == "push64_reg") return op_kind::PUSH64_REG;
		if(o == "pop8_reg") return op_kind::POP8_REG;
		if(o == "pop16_reg") return op_kind::POP16_REG;
		if(o == "pop32_reg") return op_kind::POP32_REG;
		if(o == "pop64_reg") return op_kind::POP64_REG;
		if(o == "lbl_ui32") return op_kind::LBL_UI32;
		if(o == "jmpr_i32") return op_kind::JMPR_I32;
		if(o == "jrnz_reg_i32") return op_kind::JRNZ_REG_I32;
		if(o == "jrz_reg_i32") return op_kind::JRZ_REG_I32;
		if(o == "call_ui64") return op_kind::CALL_UI64;
		if(o == "call_native_ui64") return op_kind::CALL_NATIVE_UI64;
		if (o == "call_reg") return op_kind::CALL_REG;
		if(o == "ret_ui8") return op_kind::RET_UI8;
		if(o == "salloc_reg_ui8") return op_kind::SALLOC_REG_UI8;
		if(o == "sdealloc_ui8") return op_kind::SDEALLOC_UI8;
		if(o == "exit") return op_kind::EXIT;
		assert(!"Unknown instruction");
	}

	bool writes_to(const byte* op, reg r)
	{
		switch (byte_to_op(op->val))
		{
		case op_kind::ADD_REG_REG_REG:
		case op_kind::ADD_REG_REG_UI8:
		case op_kind::SUB_REG_REG_REG:
		case op_kind::SUB_REG_REG_UI8:
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
		case op_kind::AND_REG_REG_UI8: 
		case op_kind::OR_REG_REG_REG:
		case op_kind::MV_REG_SP:
		case op_kind::MV_REG_IP:
		case op_kind::MV_REG_UI8:
		case op_kind::MV_REG_UI16:
		case op_kind::MV_REG_UI32:
		case op_kind::MV_REG_UI64:
		case op_kind::MV_REG_I8:
		case op_kind::MV_REG_I16:
		case op_kind::MV_REG_I32:
		case op_kind::MV_REG_I64:
		case op_kind::MV8_REG_REG:
		case op_kind::MV16_REG_REG:
		case op_kind::MV32_REG_REG:
		case op_kind::MV64_REG_REG: 
		case op_kind::MV8_REG_LOC:
		case op_kind::MV16_REG_LOC:
		case op_kind::MV32_REG_LOC:
		case op_kind::MV64_REG_LOC:
		case op_kind::POP8_REG:
		case op_kind::POP16_REG:
		case op_kind::POP32_REG:
		case op_kind::POP64_REG:
		case op_kind::SALLOC_REG_UI8: 
			return (op + 1)->val == r.val;
		default: return false;
		}
	}

	bool reads_from(const byte* op, reg r)
	{
		switch (byte_to_op(op->val))
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
		case op_kind::AND_REG_REG_UI8:
		case op_kind::OR_REG_REG_REG: 
			return (op + 2)->val == r.val || (op + 3)->val == r.val;

		case op_kind::ADD_REG_REG_UI8:
		case op_kind::SUB_REG_REG_UI8:
		case op_kind::MV8_REG_REG:
		case op_kind::MV16_REG_REG: 
		case op_kind::MV32_REG_REG:
		case op_kind::MV64_REG_REG: 
		case op_kind::MV8_LOC_REG:
		case op_kind::MV16_LOC_REG:
		case op_kind::MV32_LOC_REG:
		case op_kind::MV64_LOC_REG:
			return (op + 2)->val == r.val;

		case op_kind::PUSH8_REG:
		case op_kind::PUSH16_REG:
		case op_kind::PUSH32_REG:
		case op_kind::PUSH64_REG:
		case op_kind::JRNZ_REG_I32: 
		case op_kind::JRZ_REG_I32:
		case op_kind::CALL_REG:
			return (op + 1)->val == r.val;

		default: return false;
		}
	}

	/*
	* Op data helpers
	*/

	bytes<8> make_i64(int64_t a) { return *reinterpret_cast<bytes<8>*>(&a); }
	int64_t read_i64(const uint8_t* b) { return *reinterpret_cast<const int64_t*>(b); }
	int64_t read_i64(bytes<8> in) { return read_i64(&in[0].val); }

	bytes<8> make_ui64(uint64_t a) { return *reinterpret_cast<bytes<8>*>(&a); }
	uint64_t read_ui64(const uint8_t* in) { return *reinterpret_cast<const uint64_t*>(in); }
	uint64_t read_ui64(bytes<8> in) { return static_cast<uint64_t>(read_i64(in)); }

	bytes<4> make_i32(int32_t a) { return *reinterpret_cast<bytes<4>*>(&a); }
	int32_t read_i32(const uint8_t* in) { return *reinterpret_cast<const int32_t*>(in); }
	int32_t read_i32(bytes<4> in) { return read_i32(&in[0].val); }

	bytes<4> make_ui32(uint32_t a) { return make_i32(static_cast<int32_t>(a)); }
	uint32_t read_ui32(const uint8_t* in) { return static_cast<uint32_t>(read_i32(in)); }
	uint32_t read_ui32(bytes<4> in) { return static_cast<uint32_t>(read_i32(in)); }

	bytes<2> make_i16(int16_t a) { return *reinterpret_cast<bytes<2>*>(&a); }
	int16_t read_i16(const uint8_t* in) { return *reinterpret_cast<const int16_t*>(in); }
	int16_t read_i16(bytes<2> in) { return read_i16(&in[0].val); }

	bytes<2> make_ui16(uint16_t a) { return make_i16(static_cast<int16_t>(a)); }
	uint16_t read_ui16(const uint8_t* in) { return static_cast<uint16_t>(read_i16(in)); }
	uint16_t read_ui16(bytes<2> in) { return static_cast<uint16_t>(read_i16(in)); }

	bytes<1> make_i8(int8_t a) { return *reinterpret_cast<bytes<1>*>(&a); }
	int8_t read_i8(const uint8_t* in) { return *reinterpret_cast<const int8_t*>(in); }
	int8_t read_i8(bytes<1> in) { return read_i8(&in[0].val); }

	bytes<1> make_ui8(uint8_t a) { return make_i8(static_cast<int8_t>(a)); }
	uint8_t read_ui8(const uint8_t* in) { return static_cast<uint8_t>(read_i8(in)); }
	uint8_t read_ui8(bytes<1> in) { return static_cast<uint8_t>(read_i8(in)); }

	/*
	* Op helpers
	*/

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
	bytes<4> make_lt(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::LT_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_lte(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::LTE_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_lte(reg dest, reg a, byte b)
	{
		return bytes<4>{ op_to_byte(op_kind::LTE_REG_REG_I8), dest.val, a.val, b.val };
	}
	bytes<4> make_eq(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::EQ_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_neq(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::NEQ_REG_REG_REG), dest.val, a.val, b.val };
	}
	bytes<4> make_xor(reg dest, reg a, int8_t b)
	{
		return bytes<4>{ op_to_byte(op_kind::XOR_REG_REG_UI8), dest.val, a.val, byte(b) };
	}
	bytes<2> make_mv_reg_sp(reg dest)
	{
		return bytes<2>{ op_to_byte(op_kind::MV_REG_SP), dest.val };
	}
	bytes<3> make_mv64(reg dest, reg src)
	{
		return bytes<3>{ op_to_byte(op_kind::MV64_REG_REG), dest.val, src.val };
	}
	bytes<3> make_mv_reg_ui8(reg dest, uint8_t a)
	{
		return bytes<3>{ op_to_byte(op_kind::MV_REG_UI8), dest.val, a };
	}
	bytes<4> make_mv_reg_ui16(reg dest, uint16_t a)
	{
		auto lit = make_ui16(a);
		return bytes<4>{op_to_byte(op_kind::MV_REG_UI16), dest.val, lit[0], lit[1] };
	}
	bytes<6> make_mv_reg_ui32(reg dest, uint32_t a)
	{
		auto lit = make_ui32(a);
		return bytes<6>{op_to_byte(op_kind::MV_REG_UI32), dest.val, lit[0], lit[1], lit[2], lit[3] };
	}
	bytes<10> make_mv_reg_ui64(reg dest, uint64_t a)
	{
		auto lit = make_ui64(a);
		return bytes<10>{op_to_byte(op_kind::MV_REG_UI64), dest.val, lit[0], lit[1], lit[2], lit[3], lit[4], lit[5], lit[6], lit[7] };
	}
	bytes<3> make_mv_reg_i8(reg dest, int8_t a)
	{
		return bytes<3>{ op_to_byte(op_kind::MV_REG_I8), dest.val, static_cast<uint8_t>(a) };
	}
	bytes<4> make_mv_reg_i16(reg dest, int16_t a)
	{
		auto lit = make_i16(a);
		return bytes<4>{op_to_byte(op_kind::MV_REG_I16), dest.val, lit[0], lit[1] };
	}
	bytes<6> make_mv_reg_i32(reg dest, int32_t a)
	{
		auto lit = make_i32(a);
		return bytes<6>{op_to_byte(op_kind::MV_REG_I32), dest.val, lit[0], lit[1], lit[2], lit[3] };
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
		default: assert(!"Invalid mv bit count");
		}
		throw std::runtime_error("Bytecode Generation Error: Invalid mv bit count");
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
		default: assert(!"Invalid mv bit count");
		}
		throw std::runtime_error("Bytecode Generation Error: Invalid mv bit count");
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
		default: assert(!"Invalid mv bit count");
		}
		throw std::runtime_error("Bytecode Generation Error: Invalid mv bit count");
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
		throw std::runtime_error("Bytecode Generation Error: Invalid push bit count");
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
	bytes<2> make_push64(reg src)
	{
		return bytes<2>{ op_to_byte(op_kind::PUSH64_REG), src.val };
	}
	bytes<2> make_pop(uint8_t bits, reg dst)
	{
		switch (bits)
		{
		case 1: return make_pop8(dst);
		case 2: return make_pop16(dst);
		case 4: return make_pop32(dst);
		case 8: return make_pop64(dst);
		default: assert(!"Invalid pop bit count");
		}
		throw std::runtime_error("Bytecode Generation Error: Invalid pop bit count");
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
	bytes<9> make_call_native_ui64(uint64_t ip)
	{
		auto to = make_ui64(ip);
		return bytes<9>{ op_to_byte(op_kind::CALL_NATIVE_UI64), to[0], to[1], to[2], to[3], to[4], to[5], to[6], to[7] };
	}
	bytes<2> make_call_reg(reg r)
	{
		return bytes<2>{ op_to_byte(op_kind::CALL_REG), r.val };
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
	bytes<3> make_salloc_reg_ui8(reg r, uint8_t size)
	{
		return bytes<3>{ op_to_byte(op_kind::SALLOC_REG_UI8), r.val, size };
	}
	bytes<2> make_sdealloc_ui8(uint8_t size)
	{
		return bytes<2>{ op_to_byte(op_kind::SDEALLOC_UI8), size };
	}
	bytes<1> make_exit()
	{
		return bytes<1>{op_to_byte(op_kind::EXIT)};
	}

	/*
	* Bytecode
	*/

	bytecode::bytecode() {}
	bytecode::bytecode(std::vector<byte> bs) : instructions(bs) {}


	const byte* bytecode::get_instruction(near_lbl l) const
	{
		return &(instructions[l.ip]);
	}

	byte* bytecode::operator[](uint64_t index)
	{
		return &instructions[index];
	}

	void bytecode::append(bytecode& other)
	{
		instructions.insert(instructions.end(), other.data().begin(), other.data().end());
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
			auto in = get_instruction<10>(ip);
			out += op_to_string(byte_to_op(in[0].val)) + " ";
			for (int i = 1; i < op_size(byte_to_op(in[0].val)); i++)
				out += std::to_string(in[i].val) + " ";
			out += "\n";
			ip += op_size(byte_to_op(in[0].val));
		}
		return out;
	}

	size_t bytecode::size() const
	{
		return instructions.size();
	}

	std::vector<byte>& bytecode::data()
	{
		return this->instructions;
	}

	bytecode::iterator bytecode::begin()
	{
		return iterator(instructions);
	}

	bytecode::iterator bytecode::end()
	{
		return iterator(instructions, instructions.size());
	}

	/*
	* Bytecode iterator
	*/

	bytecode::iterator::iterator(std::vector<byte>& c) : i(0), data(c) {}
	bytecode::iterator::iterator(std::vector<byte>& c, uint64_t i) : i(i), data(c) {}

	bytecode::iterator::iterator(const iterator& o) : i(o.i), data(o.data) {}
	bytecode::iterator& bytecode::iterator::operator=(const iterator& o)
	{
		data = o.data;
		i = o.i;
		return *this;
	}
	bytecode::iterator bytecode::iterator::add_unsafe(uint64_t offset)
	{
		i += offset;
		return *this;
	}
	// postfix
	bytecode::iterator bytecode::iterator::operator++(int)
	{
		i += op_size(byte_to_op(data[i].val));
		return *this;
	}
	// prefix
	bytecode::iterator& bytecode::iterator::operator++()
	{
		i += op_size(byte_to_op(data[i].val));
		return *this;
	}
	bool bytecode::iterator::operator==(const iterator& o)
	{
		return (i == o.i) && (&data != &o.data);
	}
	bool bytecode::iterator::operator!=(const iterator& o)
	{
		return (i != o.i) || (&data != &o.data);
	}
	byte* bytecode::iterator::operator*()
	{
		return &data[i];
	}
	byte* bytecode::iterator::operator->()
	{
		return this->operator*();
	}

	/*
	* Bytecode Builder
	*/

	bytecode bytecode_builder::build()
	{
		return bc;
	}

	/*
	* Function
	*/

	function::function(name n, bytecode c, symbols s) : signature(n), code(c), externals(s) {}
	function::function(name n, bytecode c) : signature(n), code(c) {}
	function::function(name n, native_function_id c, symbols s) : signature(n), code(c), externals(s) {}
	function::function(name n, native_function_id c) : signature(n), code(c) {}
	function::function() {}

	name& function::get_name() { return signature; }
	symbols& function::get_symbols() { return externals; }
	bool function::is_bytecode() { return std::holds_alternative<bytecode>(code); }
	bool function::is_native() { return std::holds_alternative<native_function_id>(code); }
	bytecode& function::get_bytecode() { return std::get<bytecode>(code); }
	native_function_id function::get_native_function_id() { return std::get<native_function_id>(code); }

	/*
	* Program
	*/

	function_id program::add_function(function fn)
	{
		code.push_back(fn);
		return code.size() - 1;
	}

	function& program::get_function(function_id i)
	{
		assert(i < code.size());
		return code.at(i);
	}

	function& program::get_function(name n)
	{
		auto loc = std::find_if(code.begin(), code.end(), [&n](auto& fn) { return fn.get_name() == n; });
		assert(loc != code.end());
		return *loc;
	}

	size_t program::function_count()
	{
		return code.size();
	}

	void program::insert_padding(far_lbl loc, uint8_t size)
	{
		if (size == 0) return;
		auto& bc = code.at(loc.chunk_id).get_bytecode().data();
		bc.insert(bc.begin() + loc.ip, size, byte(0));
	}

	std::vector<function>& program::get_code() { return code; }

	std::string program::to_string()
	{
		std::string r;
		for (auto& chunk : this->code)
		{
			if (!chunk.is_bytecode()) continue;

			r += chunk.get_name() + "\n";
			r += chunk.get_bytecode().operator std::string();
			r += "\n";
		}
		return r;
	}

	/*
	* Executable
	*/

	executable::executable(bytecode code) : code(code) {}

	std::string executable::to_string()
	{
		return code.operator std::string();
	}

	bytecode::iterator executable::begin() { return code.begin(); }
	bytecode::iterator executable::end() { return code.end(); }
	byte* executable::operator[](uint64_t i)
	{
		return &code.data()[i];
	}

	/*
	* Direct Threaded Executable
	*/

	direct_threaded_executable::direct_threaded_executable(bytecode code) : code(code) {}
}