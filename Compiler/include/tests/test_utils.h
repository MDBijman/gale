#pragma once
#include "fe/data/module.h"
#include "fe/libraries/std/std_io.h"
#include <assert.h>
#include <iostream>

namespace testing
{
	struct test_failure
	{
		std::string expected;
		std::string actual;
	};

	class test_iostream : public fe::stdlib::io::iostream
	{
		std::string should_print;
		bool has_printed_;
	public:
		test_iostream(std::string s) : should_print(s), has_printed_(false) {}

		bool has_printed()
		{
			return has_printed_;
		}

		virtual void send_stdout(const std::string& s)
		{
			if (has_printed_) throw test_failure{ "", s };
			if (s != should_print) throw test_failure{ should_print, s };
			has_printed_ = true;
		}
	};
}