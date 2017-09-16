#pragma once

namespace tests
{
	struct test
	{
		test(std::string name) : name(name) {}

		virtual void run_all() = 0;

		std::string name;
	};
}