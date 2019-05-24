#include <stdint.h>
#include <iostream>
#include <string>
#include "fe/data/bytecode.h"

void** vm_fn_pointers;

extern "C" uint16_t *vm_init(void** fn_pointers)
{
    vm_fn_pointers = fn_pointers;
    return nullptr;
}

struct frame
{
	std::vector<uint8_t> registers;
	uint8_t return_register;
	uint64_t return_ip;
};

extern "C" uint64_t *vm_interpret(const fe::vm::byte* ops)
{
    std::vector<frame> stack;

    using namespace fe::vm;

    uint64_t ip = 0;

	while(1)
	{
		const byte* op = ops + ip;

		switch(byte_to_op(op->val))
		{
			case op_kind::NOP:
			ip++;
			break;

			case op_kind::EXIT:
			return nullptr;

			case op_kind::ERR:assert(!"error");break;
			case op_kind::ADD_R64_R64_R64:assert(!"todo");break;
			case op_kind::ADD_R64_R64_UI8:assert(!"todo");break;
			case op_kind::SUB_R64_R64_R64:assert(!"todo");break;
			case op_kind::SUB_R64_R64_UI8:assert(!"todo");break;
			case op_kind::MUL_R64_R64_R64:assert(!"todo");break;
			case op_kind::DIV_R64_R64_R64:assert(!"todo");break;
			case op_kind::MOD_R64_R64_R64: {
				auto destination = (op + 1)->val;
				auto lhs_register = (op + 2)->val;
				auto rhs_register = (op + 3)->val;

				auto lhs = *(uint64_t*)(&stack.back().registers[lhs_register - 7]);
				auto rhs = *(uint64_t*)(&stack.back().registers[rhs_register - 7]);

				*(uint64_t*)(&stack.back().registers[destination - 7]) = lhs % rhs;

				ip += 4;
				break;
			}
			case op_kind::GT_R8_R64_R64:assert(!"todo");break;
			case op_kind::GT_R8_R8_R8:assert(!"todo");break;
			case op_kind::GTE_R8_R64_R64:assert(!"todo");break;
			case op_kind::GTE_R8_R8_R8:assert(!"todo");break;
			case op_kind::LT_R8_R64_R64:assert(!"todo");break;
			case op_kind::LT_R8_R8_R8:assert(!"todo");break;
			case op_kind::LTE_R8_R64_R64:assert(!"todo");break;
			case op_kind::LTE_R8_R8_R8:assert(!"todo");break;
			case op_kind::EQ_R8_R64_R64: {
				auto destination = (op + 1)->val;
				auto lhs_register = (op + 2)->val;
				auto rhs_register = (op + 3)->val;

				auto lhs = *(uint64_t*)(&stack.back().registers[lhs_register - 7]);
				auto rhs = *(uint64_t*)(&stack.back().registers[rhs_register - 7]);

				stack.back().registers[destination] = (uint8_t)(lhs == rhs);

				ip += 4;
				break;
			}
			case op_kind::EQ_R8_R8_R8:assert(!"todo");break;
			case op_kind::NEQ_R8_R64_R64:assert(!"todo");break;
			case op_kind::NEQ_R8_R8_R8:assert(!"todo");break;
			case op_kind::AND_R64_R64_R64:assert(!"todo");break;
			case op_kind::AND_R8_R8_R8:assert(!"todo");break;
			case op_kind::AND_R8_R8_UI8:assert(!"todo");break;
			case op_kind::OR_R64_R64_R64:assert(!"todo");break;
			case op_kind::OR_R8_R8_R8:assert(!"todo");break;
			case op_kind::XOR_R8_R8_UI8:assert(!"todo");break;

			case op_kind::MV_REG_UI8:{
				auto register_id = (op + 1)->val;
				auto literal = read_ui8(&(op + 2)->val);
				stack.back().registers[register_id] = literal;
				ip += 3;
				break;
			}
			case op_kind::MV_REG_UI16:{
				auto register_id = (op + 1)->val;
				auto literal = read_ui16(&(op + 2)->val);
				*(uint16_t*)(&stack.back().registers[register_id - 1]) = literal;
				ip += 4;
				break;
			}
			case op_kind::MV_REG_UI32: {
				auto register_id = (op + 1)->val;
				auto literal = read_ui32(&(op + 2)->val);
				*(uint32_t*)(&stack.back().registers[register_id - 3]) = literal;
				ip += 6;
				break;
			}
			case op_kind::MV_REG_UI64: {
				auto register_id = (op + 1)->val;
				auto literal = read_ui64(&(op + 2)->val);
				*(uint64_t*)(&stack.back().registers[register_id - 7]) = literal;
				ip += 10;
				break;
			}

			case op_kind::MV_REG_I8:assert(!"todo");break;
			case op_kind::MV_REG_I16:assert(!"todo");break;
			case op_kind::MV_REG_I32:assert(!"todo");break;
			case op_kind::MV_REG_I64:assert(!"todo");break;

			case op_kind::MV_RN_RN: {
				auto count = (op + 1)->val;
				auto destination = (op + 2)->val;
				auto source = (op + 3)->val;

				for(int i = 0; i < count; i++)
				{
					stack.back().registers[destination - i] = stack.back().registers[source - i];
				}

				ip += 4;
				break;
			}

			case op_kind::MV_RN_LN:assert(!"todo");break;
			case op_kind::JMPR_I32: {
				auto offset = read_i32(&(op + 1)->val);
				ip += offset;
				break;
			}
			case op_kind::JRNZ_REG_I32:
			case op_kind::JRZ_REG_I32: {
				auto test_register = (op + 1)->val;
				auto test_value = stack.back().registers[test_register];
				auto offset = read_i32(&(op + 2)->val);

				if(byte_to_op(op->val) == op_kind::JRZ_REG_I32 && test_value == 0)
					ip += offset;
				else if(byte_to_op(op->val) == op_kind::JRNZ_REG_I32 && test_value != 0)
					ip += offset;
				else
					ip += 6;
				
				break;
			}

			case op_kind::CALL_UI64_UI8_UI8_UI8: {
				auto new_frame = frame();
				auto first_arg = (op + 9)->val;
				auto arg_count = (op + 10)->val;
				auto ret_reg =  (op + 11)->val;

				new_frame.return_register = ret_reg;
				new_frame.return_ip = ip + 12;

				for(int i = 0; i < arg_count; i++)
				{
					new_frame.registers.push_back(stack.back().registers[i - first_arg]);
				}

				stack.push_back(new_frame);

				ip = read_ui64(&(op + 1)->val);
				break;
			}

			case op_kind::CALL_NATIVE_UI64_UI8_UI8: {
				auto id = read_ui64(&(op + 1)->val);
				auto first_arg = (op + 9)->val;
				auto first_res = (op + 10)->val;
				((void(*)(uint8_t*, uint8_t, uint8_t))(vm_fn_pointers[id]))(stack.back().registers.data(), first_arg, first_res);
				ip += 11;
				break;
			}

			case op_kind::ALLOC_UI8: {
				auto additional_size = (op + 1)->val;
				auto& current_frame = stack.back();
				current_frame.registers.resize(current_frame.registers.size() + additional_size);
				ip += 2;
				break;
			}

			case op_kind::RET_UI8_UI8_UI8_UI8: {
				auto argument_count = (op + 1)->val;
				auto additional_frame_size = (op + 2)->val;
				auto first_return_register = (op + 3)->val;
				auto return_count = (op + 4)->val;

				auto return_register = stack.back().return_register;

				// #todo put return values into return register
				for (int i = 0; i < return_count; i++)
				{
					(stack.rbegin() + 1)->registers[return_register - i] = 
						stack.back().registers[first_return_register - 1 - i];
				}

				ip = stack.back().return_ip;

				stack.pop_back();

				break;
			}
		}
	}
}
