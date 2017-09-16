#pragma once
#include "grammar_tests.h"
#include "typecheck_env_tests.h"

namespace tests
{
	void run_all()
	{
		id_parsing_tests()
			.run_all();

		typecheck_environment_tests()
			.run_all();

		std::cout << "Succesfully ran tests" << std::endl;
	}
}

