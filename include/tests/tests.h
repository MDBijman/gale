#pragma once
#include "tests/fe/typecheck_env_tests.h"

#include "tests/tools/grammar_tests.h"
#include "tests/tools/lalr_tests.h"
#include "tests/tools/performance_tests.h"

namespace tests
{
	void run_all()
	{
		//performance_tests()
		//	.run_all();

		lalr::lalr_generator_tests()
			.run_all();

		id_parsing_tests()
			.run_all();

		typecheck_environment_tests()
			.run_all();

		std::cout << "Succesfully ran tests" << std::endl;
	}
}

