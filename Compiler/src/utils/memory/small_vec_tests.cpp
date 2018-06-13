#include <catch2/catch.hpp>
#include "utils/memory/small_vector.h"

TEST_CASE("small vector", "[memory]")
{
	memory::small_vector<int, 3> sv;

	REQUIRE(sv.size() == 0);

	SECTION("not to heap")
	{
		sv.push_back(1);
		sv.push_back(2);
		sv.push_back(3);
		REQUIRE(sv.size() == 3);
		REQUIRE(sv.at(0) == 1);
		REQUIRE(sv.at(1) == 2);
		REQUIRE(sv.at(2) == 3);
	}

	SECTION("to heap")
	{
		sv.push_back(1);
		sv.push_back(2);
		sv.push_back(3);
		sv.push_back(4);
		REQUIRE(sv.size() == 4);
		REQUIRE(sv.at(0) == 1);
		REQUIRE(sv.at(1) == 2);
		REQUIRE(sv.at(2) == 3);
		REQUIRE(sv.at(3) == 4);
	}
}