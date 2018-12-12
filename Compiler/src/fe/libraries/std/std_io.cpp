#include "fe/libraries/std/std_io.h"
#include "fe/pipeline/vm_stage.h"
#include <iostream>
#include <chrono>

static std::unique_ptr<fe::stdlib::io::iostream> io;

extern "C" int fe_print(uint64_t* regs, uint8_t* stack)
{
	io->send_stdout(std::to_string(regs[0]));
	return 0;
}

extern "C" int fe_println(uint64_t* regs, uint8_t* stack)
{
	io->send_stdout(std::to_string(regs[0]));
	return 0;
}

extern "C" int fe_time(uint64_t* regs, uint8_t* stack)
{
	regs[fe::vm::ret_reg] = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	return 0;
}


namespace fe::stdlib::io
{
	void iostream::send_stdout(const std::string& in)
	{
		std::cout << in;
	}
	void iostream::send_stderr(const std::string& in)
	{
		std::cerr << in;
	}

	void set_iostream(std::unique_ptr<iostream> new_io)
	{
		::io = std::move(new_io);
	}
}
