#pragma once
#include "tests/grammar_tests.h"
#include "tests/end2end_tests.h"

namespace tests
{
	void run_all()
	{
		id_parsing_tests()
			.run_all();

		std::cout << "Succesfully ran tests" << std::endl;
	}
}

