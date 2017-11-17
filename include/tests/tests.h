#pragma once
#define CATCH_CONFIG_RUNNER
#include "catch2/catch.hpp"
#include "tests/fe/typecheck_env_tests.h"
#include "tests/fe/branching_tests.h"
#include "tests/tools/grammar_tests.h"
#include "tests/tools/lr_tests.h"
#include "tests/tools/performance_tests.h"

namespace tests
{
	int run()
	{
		Catch::Session session;
		int result = session.run();
		std::cin.get();
		return result;
	}
}
