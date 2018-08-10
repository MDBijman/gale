#pragma once
#include <vector>
#include <array>
#include <variant>
#include <functional>

namespace fe::vm
{
	class machine_state;

	enum class op_kind : uint8_t
	{
		NOP = 0,

		// interrupt
		INT_UI8,

		/*
		* Arithmetic
		*/

		// reg[b0] <- reg[b1] + reg[b2]
		ADD_REG_REG_REG,
		ADD_REG_REG_UI8,
		// reg[b0] <- reg[b1] - reg[b2]
		SUB_REG_REG_REG,
		SUB_REG_REG_UI8,
		// reg[b0] <- reg[b1] * reg[b2]
		MUL_REG_REG_REG,
		// reg[b0] <- reg[b1] / reg[b2]
		DIV_REG_REG_REG,
		// reg[b0] <- reg[b1] % reg[b2]
		MOD_REG_REG_REG,
		
		/*
		* Logic
		*/

		// if reg[b1] > reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		GT_REG_REG_REG,
		// if reg[b1] >= reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		GTE_REG_REG_REG,
		// if reg[b1] == reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		EQ_REG_REG_REG,
		// if reg[b1] != reg[b2] { reg[b0] <- 1 } else { reg[b0] <- 0 }
		NEQ_REG_REG_REG,
		// if reg[b1] != 0 && reg[b2] != 0 { reg[b0] <- 1 } else { reg[b0] <- 0 }
		AND_REG_REG_REG,
		AND_REG_REG_UI8,
		// if reg[b1] != 0 || reg[b2] != 0 { reg[b0] <- 1 } else { reg[b0] <- 0 }
		OR_REG_REG_REG,

		/*
		* Control
		*/

		// reg[b0] <- sp
		MV_REG_SP,
		// reg[b0] <- ip
		MV_REG_IP,
		// reg[b0] <- b1
		MV_REG_UI8,
		MV_REG_UI16,
		MV_REG_UI32,
		MV_REG_UI64,
		MV_REG_I8,
		MV_REG_I16,
		MV_REG_I32,
		MV_REG_I64,
		// reg[b0] <- reg[b1]
		MV8_REG_REG,
		MV16_REG_REG,
		MV32_REG_REG,
		MV64_REG_REG,
		// stack[reg[b0]] <- reg[b1]
		MV8_LOC_REG,
		MV16_LOC_REG,
		MV32_LOC_REG,
		MV64_LOC_REG,
		// reg[b0] <- stack[reg[b1]]
		MV8_REG_LOC,
		MV16_REG_LOC,
		MV32_REG_LOC,
		MV64_REG_LOC,
		// stack[esp] <- reg[b0], esp++
		PUSH8_REG,
		PUSH16_REG,
		PUSH32_REG,
		PUSH64_REG,
		// reg[b0] <- stack[esp - 1], esp--
		POP8_REG,
		POP16_REG,
		POP32_REG,
		POP64_REG,
		// jump relative: ip += b0
		JMPR_I32,
		// jump relative not zero: if reg[b0] != 0 { ip += b1 } else { ip++ }
		JRNZ_REG_I32,
		// jump relative zero: if reg[b0] == 0 { ip += b1 } else { ip++ }
		JRZ_REG_I32,
		// push bp, push ip, ip <- reg[b1]
		CALL_UI64,
		// reg[x] <- pop, ip <- reg[x]
		RET_UI8,

		// temporary label with id
		LBL_UI32,
		ERR = 255
	};

	uint8_t op_to_byte(op_kind);
	op_kind byte_to_op(uint8_t);

	std::string op_to_string(op_kind);
	uint8_t op_size(op_kind);

	struct byte
	{
		byte() : val(0) {}
		byte(uint8_t v) : val(v) {}
		uint8_t val;
	};
	inline byte operator+(const byte& a, const byte& b) { return byte(a.val + b.val); }
	inline byte operator-(const byte& a, const byte& b) { return byte(a.val - b.val); }
	inline byte operator*(const byte& a, const byte& b) { return byte(a.val * b.val); }
	inline byte operator%(const byte& a, const byte& b) { return byte(a.val % b.val); }
	inline bool operator>(const byte& a, const byte& b) { return (a.val > b.val); }

	struct reg
	{
		reg(uint8_t v) : val(v) {}
		uint8_t val;
	};
	inline bool operator==(const reg& a, const reg& b) { return a.val == b.val; }

	template<int C>
	using bytes = std::array<byte, C>;

	bytes<8> make_i64(int64_t);
	int64_t read_i64(bytes<8>);
	bytes<8> make_ui64(uint64_t);
	uint64_t read_ui64(bytes<8>);

	bytes<4> make_i32(int32_t);
	int32_t read_i32(bytes<4>);
	bytes<4> make_ui32(uint32_t);
	uint32_t read_ui32(bytes<4>);

	bytes<2> make_ui16(uint16_t);
	uint16_t read_ui16(bytes<2>);
	bytes<2> make_i16(int16_t);
	int16_t read_i16(bytes<2>);

	bytes<1> make_ui8(uint8_t);
	uint8_t read_ui8(bytes<1>);
	bytes<1> make_i8(int8_t);
	int8_t read_i8(bytes<1>);

	// Operator construction methods
	bytes<1> make_nop();
	bytes<2> make_int(uint8_t id);
	bytes<4> make_add(reg dest, reg a, reg b);
	bytes<4> make_add(reg dest, reg a, byte b);
	bytes<4> make_sub(reg dest, reg a, reg b);
	bytes<4> make_sub(reg dest, reg a, byte b);
	bytes<4> make_mul(reg dest, reg a, reg b);
	bytes<4> make_div(reg dest, reg a, reg b);
	bytes<4> make_mod(reg dest, reg a, reg b);
	bytes<4> make_and(reg dest, reg a, reg b);
	bytes<4> make_and(reg dest, reg a, byte b);
	bytes<4> make_or(reg dest, reg a, reg b);
	bytes<4> make_gt(reg dest, reg a, reg b);
	bytes<4> make_gte(reg dest, reg a, reg b);
	bytes<4> make_eq(reg dest, reg a, reg b);
	bytes<4> make_neq(reg dest, reg a, reg b);
	bytes<2> make_mv_sp(reg dest);
	bytes<3> make_mv_reg_ui8(reg dest, uint8_t a);
	bytes<4> make_mv_reg_ui16(reg dest, uint16_t a);
	bytes<6> make_mv_reg_ui32(reg dest, uint32_t a);
	bytes<10> make_mv_reg_ui64(reg dest, uint64_t a);
	bytes<3> make_mv_reg_i8(reg dest, int8_t a);
	bytes<4> make_mv_reg_i16(reg dest, int16_t a);
	bytes<6> make_mv_reg_i32(reg dest, int32_t a);
	bytes<10> make_mv_reg_i64(reg dest, int64_t a);
	bytes<3> make_mv_reg_reg(uint8_t bytes, reg dest, reg a);
	bytes<3> make_mv8_reg_reg(reg dest, reg src);
	bytes<3> make_mv16_reg_reg(reg dest, reg src);
	bytes<3> make_mv32_reg_reg(reg dest, reg src);
	bytes<3> make_mv64_reg_reg(reg dest, reg src);
	bytes<3> make_mv_reg_loc(uint8_t bytes, reg dest, reg src);
	bytes<3> make_mv8_reg_loc(reg dest, reg src);
	bytes<3> make_mv16_reg_loc(reg dest, reg src);
	bytes<3> make_mv32_reg_loc(reg dest, reg src);
	bytes<3> make_mv64_reg_loc(reg dest, reg src);
	bytes<3> make_mv_loc_reg(uint8_t bytes, reg dest, reg src);
	bytes<3> make_mv8_loc_reg(reg dest, reg src);
	bytes<3> make_mv16_loc_reg(reg dest, reg src);
	bytes<3> make_mv32_loc_reg(reg dest, reg src);
	bytes<3> make_mv64_loc_reg(reg dest, reg src);
	bytes<2> make_push(uint8_t bytes, reg src);
	bytes<2> make_push8(reg src);
	bytes<2> make_push16(reg src);
	bytes<2> make_push32(reg src);
	bytes<2> make_push64(reg src);
	bytes<2> make_pop(uint8_t bytes, reg src);
	bytes<2> make_pop8(reg dest);
	bytes<2> make_pop16(reg dest);
	bytes<2> make_pop32(reg dest);
	bytes<2> make_pop64(reg dest);
	bytes<9> make_call_ui64(uint64_t ip);
	bytes<2> make_ret(byte a);
	bytes<5> make_jmpr_i32(int32_t offset);
	bytes<6> make_jrnz_i32(reg a, int32_t offset);
	bytes<6> make_jrz_i32(reg a, int32_t offset);
	bytes<5> make_lbl(uint32_t id);

	// far_lbl is used to refer to an instruction and the chunk it is a part of

	struct far_lbl
	{
		far_lbl() : chunk_id(0), ip(0) {}
		far_lbl(uint64_t chunk, uint64_t ip) : chunk_id(chunk), ip(ip) {}
		uint64_t chunk_id; 
		uint64_t ip;
		uint64_t make_ip()
		{
			return (chunk_id << 32) | ip;
		}
	};

	// near_lbl is used to refer to an instruction within a chunk of bytecode

	struct near_lbl 
	{
		near_lbl(uint64_t i) : ip(i) {}
		uint64_t ip; 
	};
	inline near_lbl operator+(const near_lbl& a, const near_lbl& b) { return near_lbl(a.ip + b.ip); }
	inline near_lbl operator-(const near_lbl& a, const near_lbl& b) { return near_lbl(a.ip - b.ip); }

	class bytecode
	{
		std::vector<byte> instructions;

	public:
		// Adds the bytes to the end of this bytecode, returning the address of the first byte
		template<int C> std::pair<near_lbl, uint32_t> add_instruction(bytes<C> in)
		{
			near_lbl l(instructions.size());
			for (int i = 0; i < C; i++) instructions.push_back(in[i]);
			return std::make_pair(l, in.size());
		}
		
		// Adds the vector of bytes to the end of this bytecode, returning the address of the first byte
		template<int... Cs> std::pair<near_lbl, uint32_t> add_instructions(bytes<Cs>... in)
		{
			near_lbl l(instructions.size());
			(add_instruction(in), ...);
			return std::make_pair(l, (Cs + ... + 0));
		}
		
		// Returns the C bytes starting at the given address, padded with op_kind::ERR bytes
		template<int C> bytes<C> get_instruction(near_lbl l) const
		{
			bytes<C> res;
			for (int i = 0; i < C; i++) res[i] = i + l.ip < instructions.size() ? instructions[l.ip + i] : op_to_byte(op_kind::ERR);
			return res;
		}

		// Sets the instruction at the given address to the new bytes 
		template<int C> void set_instruction(near_lbl l, bytes<C> b)
		{
			for (int i = 0; i < C; i++) instructions[l.ip + i] = b[i];
		}
		
		// Returns true if the given address maps to an instruction
		bool has_instruction(near_lbl) const;

		operator std::string() const;

		std::vector<byte>& data()
		{
			return this->instructions;
		}
	};

	class program
	{
		std::vector<bytecode> chunks;
		std::vector<std::function<void(machine_state&)>> interrupts;

	public:
		uint8_t add_chunk(bytecode);
		bytecode& get_chunk(uint8_t);
		size_t chunk_count();
		template<int C> bytes<C> get_instruction(far_lbl l)
		{
			return chunks.at(l.chunk_id).get_instruction<C>(l.ip);
		}

		void insert_padding(far_lbl loc, uint8_t size)
		{
			chunks.at(loc.chunk_id).data().insert(chunks.at(loc.chunk_id).data().begin() + loc.ip, size, byte(0));
		}

		uint8_t add_interrupt(std::function<void(machine_state&)> interrupt);
		std::function<void(machine_state&)> get_interrupt(uint8_t id);

		void print();
	};
}

namespace std
{
	template<> struct hash<fe::vm::reg>
	{
		size_t operator()(const fe::vm::reg& r) const
		{
			return std::hash<uint64_t>()(r.val);
		}
	};
}
