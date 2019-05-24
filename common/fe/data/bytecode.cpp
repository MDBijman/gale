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
		case op_kind::ADD_R64_R64_R64: return "add_r64_r64_r64";
		case op_kind::ADD_R64_R64_UI8: return "add_r64_r64_ui8";
		case op_kind::SUB_R64_R64_R64: return "sub_r64_r64_r64";
		case op_kind::SUB_R64_R64_UI8: return "sub_r64_r64_ui8";
		case op_kind::MUL_R64_R64_R64: return "mul_r64_r64_r64";
		case op_kind::DIV_R64_R64_R64: return "div_r64_r64_r64";
		case op_kind::MOD_R64_R64_R64: return "mod_r64_r64_r64";
		case op_kind::GT_R8_R64_R64: return "gt_r8_r64_r64";
		case op_kind::GTE_R8_R64_R64: return "gte_r8_r64_r64";
		case op_kind::LT_R8_R64_R64: return "lt_r8_r64_r64";
		case op_kind::LTE_R8_R64_R64: return "lte_r8_r64_r64";
		case op_kind::EQ_R8_R64_R64: return "eq_r8_r64_r64";
		case op_kind::EQ_R8_R8_R8: return "eq_r8_r8_r8";
		case op_kind::NEQ_R8_R64_R64: return "neq_r8_r64_r64";
		case op_kind::AND_R64_R64_R64: return "and_r64_r64_r64";
		case op_kind::AND_R8_R8_UI8: return "and_r8_r8_ui8";
		case op_kind::AND_R8_R8_R8: return "and_r8_r8_r8";
		case op_kind::OR_R64_R64_R64: return "or_r64_r64_r64";
		case op_kind::OR_R8_R8_R8: return "or_r8_r8_r8";
		case op_kind::XOR_R8_R8_UI8: return "xor_r8_r8_ui8";
		case op_kind::MV_REG_UI8: return "mv_r8_ui8";
		case op_kind::MV_REG_UI16: return "mv_r16_ui16";
		case op_kind::MV_REG_UI32: return "mv_r32_ui32";
		case op_kind::MV_REG_UI64: return "mv_r64_ui64";
		case op_kind::MV_REG_I8: return "mv_r8_i8";
		case op_kind::MV_REG_I16: return "mv_r16_i16";
		case op_kind::MV_REG_I32: return "mv_r32_i32";
		case op_kind::MV_REG_I64: return "mv_r64_i64";
		case op_kind::MV_RN_RN: return "mv_rn_rn";
		case op_kind::MV_RN_LN: return "mv_rn_ln";
		case op_kind::LBL_UI32: return "lbl_ui32";
		case op_kind::JMPR_I32: return "jmpr_i32";
		case op_kind::JRNZ_REG_I32: return "jrnz_reg_i32";
		case op_kind::JRZ_REG_I32: return "jrz_reg_i32";
		case op_kind::CALL_UI64_UI8_UI8_UI8: return "call_ui64_ui8_ui8_ui8";
		case op_kind::CALL_NATIVE_UI64_UI8_UI8: return "call_native_ui64_ui8_ui8";
		case op_kind::ALLOC_UI8: return "alloc_ui8";
		case op_kind::RET_UI8_UI8_UI8_UI8: return "ret_ui8_ui8_ui8_ui8";
		case op_kind::EXIT: return "exit";
		}
		assert(!"Unknown instruction");
	}

	op_kind string_to_op(const std::string &o)
	{
		if (o == "nop") return op_kind::NOP;
		if (o == "add_r64_r64_r64") return op_kind::ADD_R64_R64_R64;
		if (o == "add_r64_r64_ui8") return op_kind::ADD_R64_R64_UI8;
		if (o == "sub_r64_r64_r64") return op_kind::SUB_R64_R64_R64;
		if (o == "sub_r64_r64_ui8") return op_kind::SUB_R64_R64_UI8;
		if (o == "mul_r64_r64_r64") return op_kind::MUL_R64_R64_R64;
		if (o == "div_r64_r64_r64") return op_kind::DIV_R64_R64_R64;
		if (o == "mod_r64_r64_r64") return op_kind::MOD_R64_R64_R64;
		if (o == "gt_r8_r64_r64") return op_kind::GT_R8_R64_R64;
		if (o == "gte_r8_r64_r64") return op_kind::GTE_R8_R64_R64;
		if (o == "lt_r8_r64_r64") return op_kind::LT_R8_R64_R64;
		if (o == "lte_r8_r64_r64") return op_kind::LTE_R8_R64_R64;
		if (o == "eq_r8_r64_r64") return op_kind::EQ_R8_R64_R64;
		if (o == "eq_r8_r8_r8") return op_kind::EQ_R8_R8_R8;
		if (o == "neq_r8_r64_r64") return op_kind::NEQ_R8_R64_R64;
		if (o == "and_r64_r64_r64") return op_kind::AND_R64_R64_R64;
		if (o == "and_r8_r8_ui8") return op_kind::AND_R8_R8_UI8;
		if (o == "and_r8_r8_r8") return op_kind::AND_R8_R8_R8;
		if (o == "or_r64_r64_r64") return op_kind::OR_R64_R64_R64;
		if (o == "or_r8_r8_r8") return op_kind::OR_R8_R8_R8;
		if (o == "xor_r8_r8_ui8") return op_kind::XOR_R8_R8_UI8;
		if (o == "mv_r8_ui8") return op_kind::MV_REG_UI8;
		if (o == "mv_r16_ui16") return op_kind::MV_REG_UI16;
		if (o == "mv_r32_ui32") return op_kind::MV_REG_UI32;
		if (o == "mv_r64_ui64") return op_kind::MV_REG_UI64;
		if (o == "mv_r8_i8") return op_kind::MV_REG_I8;
		if (o == "mv_r16_i16") return op_kind::MV_REG_I16;
		if (o == "mv_r32_i32") return op_kind::MV_REG_I32;
		if (o == "mv_r64_i64") return op_kind::MV_REG_I64;
		if (o == "mv_rn_rn") return op_kind::MV_RN_RN;
		if (o == "mv_rn_ln") return op_kind::MV_RN_LN;
		if (o == "lbl_ui32") return op_kind::LBL_UI32;
		if (o == "jmpr_i32") return op_kind::JMPR_I32;
		if (o == "jrnz_reg_i32") return op_kind::JRNZ_REG_I32;
		if (o == "jrz_reg_i32") return op_kind::JRZ_REG_I32;
		if (o == "call_native_ui64_ui8_ui8") return op_kind::CALL_NATIVE_UI64_UI8_UI8;
		if (o == "call_ui64_ui8_ui8_ui8") return op_kind::CALL_UI64_UI8_UI8_UI8;
		if (o == "alloc_ui8") return op_kind::ALLOC_UI8;
		if (o == "ret_ui8_ui8_ui8_ui8") return op_kind::RET_UI8_UI8_UI8_UI8;
		if (o == "exit") return op_kind::EXIT;
		throw std::runtime_error("Unknown instruction");
	}

	bool writes_to(const byte *op, reg r)
	{
		switch (byte_to_op(op->val))
		{
		case op_kind::ADD_R64_R64_R64:
		case op_kind::ADD_R64_R64_UI8:
		case op_kind::SUB_R64_R64_R64:
		case op_kind::SUB_R64_R64_UI8:
		case op_kind::MUL_R64_R64_R64:
		case op_kind::DIV_R64_R64_R64:
		case op_kind::MOD_R64_R64_R64:
		case op_kind::GT_R8_R64_R64:
		case op_kind::GTE_R8_R64_R64:
		case op_kind::LT_R8_R64_R64:
		case op_kind::LTE_R8_R64_R64:
		case op_kind::EQ_R8_R64_R64:
		case op_kind::NEQ_R8_R64_R64:
		case op_kind::AND_R64_R64_R64:
		case op_kind::AND_R8_R8_UI8:
		case op_kind::OR_R64_R64_R64:
		case op_kind::MV_REG_UI8:
		case op_kind::MV_REG_UI16:
		case op_kind::MV_REG_UI32:
		case op_kind::MV_REG_UI64:
		case op_kind::MV_REG_I8:
		case op_kind::MV_REG_I16:
		case op_kind::MV_REG_I32:
		case op_kind::MV_REG_I64: return (op + 1)->val == r.val;
		default: return false;
		}
	}

	bool reads_from(const byte *op, reg r)
	{
		switch (byte_to_op(op->val))
		{
		case op_kind::ADD_R64_R64_R64:
		case op_kind::SUB_R64_R64_R64:
		case op_kind::MUL_R64_R64_R64:
		case op_kind::DIV_R64_R64_R64:
		case op_kind::MOD_R64_R64_R64:
		case op_kind::GT_R8_R64_R64:
		case op_kind::GTE_R8_R64_R64:
		case op_kind::LT_R8_R64_R64:
		case op_kind::LTE_R8_R64_R64:
		case op_kind::EQ_R8_R64_R64:
		case op_kind::NEQ_R8_R64_R64:
		case op_kind::AND_R64_R64_R64:
		case op_kind::AND_R8_R8_UI8:
		case op_kind::OR_R64_R64_R64:
			return (op + 2)->val == r.val || (op + 3)->val == r.val;

		case op_kind::ADD_R64_R64_UI8:
		case op_kind::SUB_R64_R64_UI8: return (op + 2)->val == r.val;

		case op_kind::JRNZ_REG_I32:
		case op_kind::JRZ_REG_I32:

		default: return false;
		}
	}

	/*
	 * Op data helpers
	 */

	bytes<8> make_i64(int64_t a)
	{
		return *reinterpret_cast<bytes<8> *>(&a);
	}
	int64_t read_i64(const uint8_t *b)
	{
		return *reinterpret_cast<const int64_t *>(b);
	}
	int64_t read_i64(bytes<8> in)
	{
		return read_i64(&in[0].val);
	}

	bytes<8> make_ui64(uint64_t a)
	{
		return *reinterpret_cast<bytes<8> *>(&a);
	}
	uint64_t read_ui64(const uint8_t *in)
	{
		return *reinterpret_cast<const uint64_t *>(in);
	}
	uint64_t read_ui64(bytes<8> in)
	{
		return static_cast<uint64_t>(read_i64(in));
	}

	bytes<4> make_i32(int32_t a)
	{
		return *reinterpret_cast<bytes<4> *>(&a);
	}
	int32_t read_i32(const uint8_t *in)
	{
		return *reinterpret_cast<const int32_t *>(in);
	}
	int32_t read_i32(bytes<4> in)
	{
		return read_i32(&in[0].val);
	}

	bytes<4> make_ui32(uint32_t a)
	{
		return make_i32(static_cast<int32_t>(a));
	}
	uint32_t read_ui32(const uint8_t *in)
	{
		return static_cast<uint32_t>(read_i32(in));
	}
	uint32_t read_ui32(bytes<4> in)
	{
		return static_cast<uint32_t>(read_i32(in));
	}

	bytes<2> make_i16(int16_t a)
	{
		return *reinterpret_cast<bytes<2> *>(&a);
	}
	int16_t read_i16(const uint8_t *in)
	{
		return *reinterpret_cast<const int16_t *>(in);
	}
	int16_t read_i16(bytes<2> in)
	{
		return read_i16(&in[0].val);
	}

	bytes<2> make_ui16(uint16_t a)
	{
		return make_i16(static_cast<int16_t>(a));
	}
	uint16_t read_ui16(const uint8_t *in)
	{
		return static_cast<uint16_t>(read_i16(in));
	}
	uint16_t read_ui16(bytes<2> in)
	{
		return static_cast<uint16_t>(read_i16(in));
	}

	bytes<1> make_i8(int8_t a)
	{
		return *reinterpret_cast<bytes<1> *>(&a);
	}
	int8_t read_i8(const uint8_t *in)
	{
		return *reinterpret_cast<const int8_t *>(in);
	}
	int8_t read_i8(bytes<1> in)
	{
		return read_i8(&in[0].val);
	}

	bytes<1> make_ui8(uint8_t a)
	{
		return make_i8(static_cast<int8_t>(a));
	}
	uint8_t read_ui8(const uint8_t *in)
	{
		return static_cast<uint8_t>(read_i8(in));
	}
	uint8_t read_ui8(bytes<1> in)
	{
		return static_cast<uint8_t>(read_i8(in));
	}

	/*
	 * Op helpers
	 */

	bytes<1> make_nop()
	{
		return bytes<1>{ op_to_byte(op_kind::NOP) };
	}
	bytes<4> make_add_r64_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::ADD_R64_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_add_r64_r64_ui8(reg dest, reg a, byte b)
	{
		return bytes<4>{ op_to_byte(op_kind::ADD_R64_R64_UI8), dest.val, a.val, b.val };
	}
	bytes<4> make_sub_r64_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::SUB_R64_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_sub_r64_r64_ui8(reg dest, reg a, byte b)
	{
		return bytes<4>{ op_to_byte(op_kind::SUB_R64_R64_UI8), dest.val, a.val, b.val };
	}
	bytes<4> make_mul_r64_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::MUL_R64_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_div_r64_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::DIV_R64_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_mod_r64_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::MOD_R64_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_gt_r8_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::GT_R8_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_gt_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::GT_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_gte_r8_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::GTE_R8_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_gte_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::GTE_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_lt_r8_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::LT_R8_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_lt_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::LT_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_lte_r8_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::LTE_R8_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_lte_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::LTE_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_eq_r8_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::EQ_R8_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_eq_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::EQ_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_neq_r8_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::NEQ_R8_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_neq_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::NEQ_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_and_r64_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::AND_R64_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_and_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::AND_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_and_r8_r8_ui8(reg dest, reg a, byte b)
	{
		return bytes<4>{ op_to_byte(op_kind::AND_R8_R8_UI8), dest.val, a.val, b.val };
	}
	bytes<4> make_or_r64_r64_r64(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::OR_R64_R64_R64), dest.val, a.val, b.val };
	}
	bytes<4> make_or_r8_r8_r8(reg dest, reg a, reg b)
	{
		return bytes<4>{ op_to_byte(op_kind::OR_R8_R8_R8), dest.val, a.val, b.val };
	}
	bytes<4> make_xor_r8_r8_ui8(reg dest, reg a, int8_t b)
	{
		return bytes<4>{ op_to_byte(op_kind::XOR_R8_R8_UI8), dest.val, a.val, byte(b) };
	}
	bytes<3> make_mv_reg_ui8(reg dest, uint8_t a)
	{
		return bytes<3>{ op_to_byte(op_kind::MV_REG_UI8), dest.val, a };
	}
	bytes<4> make_mv_reg_ui16(reg dest, uint16_t a)
	{
		auto lit = make_ui16(a);
		return bytes<4>{ op_to_byte(op_kind::MV_REG_UI16), dest.val, lit[0], lit[1] };
	}
	bytes<6> make_mv_reg_ui32(reg dest, uint32_t a)
	{
		auto lit = make_ui32(a);
		return bytes<6>{
			op_to_byte(op_kind::MV_REG_UI32), dest.val, lit[0], lit[1], lit[2], lit[3]
		};
	}
	bytes<10> make_mv_reg_ui64(reg dest, uint64_t a)
	{
		auto lit = make_ui64(a);
		return bytes<10>{ op_to_byte(op_kind::MV_REG_UI64),
				  dest.val,
				  lit[0],
				  lit[1],
				  lit[2],
				  lit[3],
				  lit[4],
				  lit[5],
				  lit[6],
				  lit[7] };
	}
	bytes<3> make_mv_reg_i8(reg dest, int8_t a)
	{
		return bytes<3>{ op_to_byte(op_kind::MV_REG_I8), dest.val,
				 static_cast<uint8_t>(a) };
	}
	bytes<4> make_mv_reg_i16(reg dest, int16_t a)
	{
		auto lit = make_i16(a);
		return bytes<4>{ op_to_byte(op_kind::MV_REG_I16), dest.val, lit[0], lit[1] };
	}
	bytes<6> make_mv_reg_i32(reg dest, int32_t a)
	{
		auto lit = make_i32(a);
		return bytes<6>{
			op_to_byte(op_kind::MV_REG_I32), dest.val, lit[0], lit[1], lit[2], lit[3]
		};
	}
	bytes<10> make_mv_reg_i64(reg dest, int64_t a)
	{
		auto lit = make_i64(a);
		return bytes<10>{ op_to_byte(op_kind::MV_REG_I64),
				  dest.val,
				  lit[0],
				  lit[1],
				  lit[2],
				  lit[3],
				  lit[4],
				  lit[5],
				  lit[6],
				  lit[7] };
	}
	bytes<4> make_mv_rn_rn(byte count, reg dest, reg src)
	{
		return bytes<4>{ op_to_byte(op_kind::MV_RN_RN), count, dest.val, src.val };
	}
	bytes<4> make_mv_rn_ln(byte count, reg dest, reg src)
	{
		return bytes<4>{ op_to_byte(op_kind::MV_RN_LN), count.val, dest.val, src.val };
	}
	bytes<12> make_call_ui64_ui8_ui8_ui8(uint64_t ip, uint8_t reg, uint8_t regc,
					     uint8_t ret_reg)
	{
		auto to = make_ui64(ip);
		return bytes<12>{ op_to_byte(op_kind::CALL_UI64_UI8_UI8_UI8),
				  to[0],
				  to[1],
				  to[2],
				  to[3],
				  to[4],
				  to[5],
				  to[6],
				  to[7],
				  reg,
				  regc,
				  ret_reg };
	}
	bytes<11> make_call_native_ui64_ui8_ui8(uint64_t ip, uint8_t in_reg, uint8_t out_reg)
	{
		auto to = make_ui64(ip);
		return bytes<11>{ op_to_byte(op_kind::CALL_NATIVE_UI64_UI8_UI8),
				  to[0],
				  to[1],
				  to[2],
				  to[3],
				  to[4],
				  to[5],
				  to[6],
				  to[7],
				  in_reg,
				  out_reg };
	}
	bytes<2> make_alloc_ui8(uint8_t size)
	{
		return bytes<2>{ op_to_byte(op_kind::ALLOC_UI8), size };
	}
	bytes<5> make_ret(byte paramc, byte fs, byte first_reg, byte returnc)
	{
		return bytes<5>{ op_to_byte(op_kind::RET_UI8_UI8_UI8_UI8), paramc.val, fs.val,
				 first_reg.val, returnc.val };
	}
	bytes<5> make_jmpr_i32(int32_t dest)
	{
		auto to = make_i32(dest);
		return bytes<5>{ op_to_byte(op_kind::JMPR_I32), to[0], to[1], to[2], to[3] };
	}
	bytes<6> make_jrnz_i32(reg a, int32_t dest)
	{
		auto to = make_i32(dest);
		return bytes<6>{
			op_to_byte(op_kind::JRNZ_REG_I32), a.val, to[0], to[1], to[2], to[3]
		};
	}
	bytes<6> make_jrz_i32(reg a, int32_t dest)
	{
		auto to = make_i32(dest);
		return bytes<6>{
			op_to_byte(op_kind::JRZ_REG_I32), a.val, to[0], to[1], to[2], to[3]
		};
	}
	bytes<5> make_lbl(uint32_t id)
	{
		auto lit = make_ui32(id);
		return bytes<5>{ op_to_byte(op_kind::LBL_UI32), lit[0], lit[1], lit[2], lit[3] };
	}
	bytes<1> make_exit()
	{
		return bytes<1>{ op_to_byte(op_kind::EXIT) };
	}

	/*
	 * Bytecode
	 */

	bytecode::bytecode()
	{
	}
	bytecode::bytecode(std::vector<byte> bs) : instructions(bs)
	{
	}

	const byte *bytecode::get_instruction(near_lbl l) const
	{
		return &(instructions[l.ip]);
	}

	byte *bytecode::operator[](uint64_t index)
	{
		return &instructions[index];
	}

	void bytecode::append(const bytecode &other)
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
			auto in = get_instruction<12>(ip);
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

	std::vector<byte> &bytecode::data()
	{
		return this->instructions;
	}

	const std::vector<byte> &bytecode::data() const
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

	bytecode::iterator::iterator(std::vector<byte> &c) : i(0), data(c)
	{
	}
	bytecode::iterator::iterator(std::vector<byte> &c, uint64_t i) : i(i), data(c)
	{
	}

	bytecode::iterator::iterator(const iterator &o) : i(o.i), data(o.data)
	{
	}
	bytecode::iterator &bytecode::iterator::operator=(const iterator &o)
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
	bytecode::iterator &bytecode::iterator::operator++()
	{
		i += op_size(byte_to_op(data[i].val));
		return *this;
	}
	bool bytecode::iterator::operator==(const iterator &o)
	{
		return (i == o.i) && (&data != &o.data);
	}
	bool bytecode::iterator::operator!=(const iterator &o)
	{
		return (i != o.i) || (&data != &o.data);
	}
	byte *bytecode::iterator::operator*()
	{
		return &data[i];
	}
	byte *bytecode::iterator::operator->()
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

	function::function(name n, bytecode c, symbols s) : signature(n), code(c), externals(s)
	{
	}
	function::function(name n, bytecode c) : signature(n), code(c)
	{
	}
	function::function(name n, native_function_id c, symbols s)
	    : signature(n), code(c), externals(s)
	{
	}
	function::function(name n, native_function_id c) : signature(n), code(c)
	{
	}
	function::function()
	{
	}

	const name &function::get_name() const
	{
		return signature;
	}
	symbols &function::get_symbols()
	{
		return externals;
	}
	bool function::is_bytecode() const
	{
		return std::holds_alternative<bytecode>(code);
	}
	bool function::is_native() const
	{
		return std::holds_alternative<native_function_id>(code);
	}
	bytecode &function::get_bytecode()
	{
		return std::get<bytecode>(code);
	}
	const bytecode &function::get_bytecode() const
	{
		return std::get<bytecode>(code);
	}
	native_function_id function::get_native_function_id() const
	{
		return std::get<native_function_id>(code);
	}

	/*
	 * Module
	 */

	function_id module::add_function(function fn)
	{
		code.push_back(fn);
		return code.size() - 1;
	}

	function &module::get_function(function_id i)
	{
		assert(i < code.size());
		return code.at(i);
	}

	function &module::get_function(name n)
	{
		auto loc = std::find_if(code.begin(), code.end(),
					[&n](auto &fn) { return fn.get_name() == n; });
		assert(loc != code.end());
		return *loc;
	}

	size_t module::function_count() const
	{
		return code.size();
	}

	void module::insert_padding(far_lbl loc, uint8_t size)
	{
		if (size == 0) return;
		auto &bc = code.at(loc.chunk_id).get_bytecode().data();
		bc.insert(bc.begin() + loc.ip, size, byte(0));
	}

	std::vector<function> &module::get_code()
	{
		return code;
	}

	const std::vector<function> &module::get_code() const
	{
		return code;
	}

	std::string module::to_string()
	{
		std::string r;
		for (auto &chunk : this->code)
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

	executable::executable(bytecode code) : code(code)
	{
	}

	std::string executable::to_string()
	{
		return code.operator std::string();
	}

	bytecode::iterator executable::begin()
	{
		return code.begin();
	}
	bytecode::iterator executable::end()
	{
		return code.end();
	}
	byte *executable::operator[](uint64_t i)
	{
		return &code.data()[i];
	}
	size_t executable::byte_length() const
	{
		return this->code.size();
	}

	/*
	 * Direct Threaded Executable
	 */

	direct_threaded_executable::direct_threaded_executable(bytecode code) : code(code)
	{
	}
} // namespace fe::vm