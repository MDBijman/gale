#include "bytecode_parser.h"
#include "fe/data/bytecode.h"
#include <string>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fe::vm
{

	/*
	* Set of helper functions to read integers in the bytecode file. The numbers in a bytecode file are split up in bytes.
	*/

	uint8_t parse_uint8(std::istringstream& ss)
	{
		// #performance This is probably quite slow, and since it is used in all others is might be worthwhile to speed it up.
		int a;
		ss >> a;
		return static_cast<uint8_t>(a);
	}

	uint16_t parse_uint16(std::istringstream& ss)
	{
		return read_ui16({ parse_uint8(ss), parse_uint8(ss) });
	}

	uint32_t parse_uint32(std::istringstream& ss)
	{
		return read_ui32({ parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss) });
	}

	uint64_t parse_uint64(std::istringstream& ss)
	{
		return read_ui64({ parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss) });
	}

	int32_t parse_int32(std::istringstream& ss)
	{
		return read_i32({ parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss) });
	}
	
	int64_t parse_int64(std::istringstream& ss)
	{
		return read_i64({ parse_uint8(ss), parse_uint8(ss), parse_uint8(ss), parse_uint8(ss) });
	}

	/*
	* Parses a single instruction/line and appends it to the given bytecode
	*/
	void parse_line(const std::string& line, bytecode& bc)
	{
		std::string instruction;
		std::istringstream iss(line);
		iss >> instruction;
		op_kind op = string_to_op(instruction);
		switch (op)
		{
		case op_kind::NOP:
			bc.add_instruction(make_nop());
			break;
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
		case op_kind::LTE_REG_REG_I8:
		case op_kind::EQ_REG_REG_REG:
		case op_kind::NEQ_REG_REG_REG:
		case op_kind::AND_REG_REG_REG:
		case op_kind::AND_REG_REG_UI8:
		case op_kind::OR_REG_REG_REG:
		case op_kind::XOR_REG_REG_UI8: {
			uint8_t a = parse_uint8(iss);
			uint8_t b = parse_uint8(iss);
			uint8_t c = parse_uint8(iss);
			bc.add_instruction(bytes<4>{op_to_byte(op), a, b, c});
			break;
		}
		case op_kind::MV_REG_SP:
		case op_kind::MV_REG_IP:
		case op_kind::PUSH8_REG:
		case op_kind::PUSH16_REG:
		case op_kind::PUSH32_REG:
		case op_kind::PUSH64_REG:
		case op_kind::POP8_REG:
		case op_kind::POP16_REG:
		case op_kind::POP32_REG:
		case op_kind::POP64_REG: {
			uint8_t a = parse_uint8(iss);
			bc.add_instruction(bytes<2>{op_to_byte(op), a });
			break;
		}
		case op_kind::MV_REG_UI8: {
			uint8_t a = parse_uint8(iss);
			uint8_t b = parse_uint8(iss);
			bc.add_instruction(bytes<3>{op_to_byte(op), a, b });
			break;
		}
		case op_kind::MV_REG_UI16: {
			uint8_t a = parse_uint8(iss);
			uint16_t b = parse_uint16(iss);
			bc.add_instruction(make_mv_reg_ui16(a, b));
			break;
		}
		case op_kind::MV_REG_UI32: {
			uint8_t a = parse_uint8(iss);
			uint32_t b = parse_uint32(iss);
			bc.add_instruction(make_mv_reg_ui32(a, b));
			break;
		}
		case op_kind::MV_REG_UI64: {
			uint8_t a = parse_uint8(iss);
			uint64_t b = parse_uint64(iss);
			bc.add_instruction(make_mv_reg_ui64(a, b));
			break;
		}
		case op_kind::MV_REG_I8:
		case op_kind::MV_REG_I16:
		case op_kind::MV_REG_I32:
		case op_kind::MV_REG_I64:
			assert(!"nyi");
		case op_kind::MV8_REG_REG:
		case op_kind::MV16_REG_REG:
		case op_kind::MV32_REG_REG:
		case op_kind::MV64_REG_REG:
		case op_kind::MV8_LOC_REG:
		case op_kind::MV16_LOC_REG:
		case op_kind::MV32_LOC_REG:
		case op_kind::MV64_LOC_REG:
		case op_kind::MV8_REG_LOC:
		case op_kind::MV16_REG_LOC:
		case op_kind::MV32_REG_LOC:
		case op_kind::MV64_REG_LOC: {
			uint8_t a = parse_uint8(iss);
			uint8_t b = parse_uint8(iss);
			bc.add_instruction(bytes<3>{ op_to_byte(op), a, b });
			break;
		}
		case op_kind::LBL_UI32:
			throw std::runtime_error("Labels should not be in executables");
		case op_kind::JMPR_I32: {
			int32_t a = parse_int32(iss);
			bc.add_instruction(make_jmpr_i32(a));
			break;
		}
		case op_kind::JRNZ_REG_I32: {
			uint8_t a = parse_uint8(iss);
			int32_t b = parse_int32(iss);
			bc.add_instruction(make_jrnz_i32(a, b));
			break;
		}
		case op_kind::JRZ_REG_I32: {
			uint8_t a = parse_uint8(iss);
			int32_t b = parse_int32(iss);
			bc.add_instruction(make_jrz_i32(a, b));
			break;
		}
		case op_kind::CALL_UI64: {
			uint64_t a = parse_uint64(iss);
			bc.add_instruction(make_call_ui64(a));
			break;
		}
		case op_kind::CALL_NATIVE_UI64: {
			uint64_t a = parse_uint64(iss);
			bc.add_instruction(make_call_native_ui64(a));
			break;
		}
		case op_kind::RET_UI8: {
			uint8_t a = parse_uint8(iss);
			bc.add_instruction(make_ret(a));
			break;
		}
		case op_kind::SALLOC_REG_UI8: {
			uint8_t a = parse_uint8(iss);
			uint8_t b = parse_uint8(iss);
			bc.add_instruction(make_salloc_reg_ui8(a, b));
			break;
		}
		case op_kind::SDEALLOC_UI8: {
			uint8_t a = parse_uint8(iss);
			bc.add_instruction(make_sdealloc_ui8(a));
			break;
		}
		case op_kind::EXIT: {
			bc.add_instruction(make_exit());
			break;
		}
		}
	}

	executable parse_bytecode(const std::string& filename)
	{
		bytecode bc;

		std::ifstream i;
		i.open(filename);

		std::string line;
		while (std::getline(i, line))
		{
			parse_line(line, bc);
		}

		return executable(bc, {});
	}
}