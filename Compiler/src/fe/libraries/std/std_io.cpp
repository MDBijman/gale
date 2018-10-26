#include "fe/libraries/std/std_io.h"
#include "fe/pipeline/vm_stage.h"
#include <iostream>
#include <chrono>

extern "C" int fe_print(uint64_t* regs, uint8_t* stack)
{
	std::cout << regs[0];
	return 0;
}

extern "C" int fe_println(uint64_t* regs, uint8_t* stack)
{
	std::cout << regs[0] << std::endl;
	return 0;
}

extern "C" int fe_time(uint64_t* regs, uint8_t* stack)
{
	regs[fe::vm::ret_reg] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	return 0;
}