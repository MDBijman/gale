#pragma once
#include "fe/data/typecheck_environment.h"
#include "fe/data/types.h"

#include <catch2/catch.hpp>
#include <variant>

SCENARIO("type environments can manage types", "[type_environment]")
{
	using namespace fe::types;

	GIVEN("an empty type environment")
	{
		fe::typecheck_environment t_env;

		WHEN("a type is added to the environment")
		{
			auto before_type = atom_type("std.i32");
			t_env.set_type("test", unique_type(before_type.copy()));

			THEN("retrieving a type with that name should return the correct type")
			{
				auto t = t_env.typeof("test");
				REQUIRE(std::holds_alternative<std::reference_wrapper<type>>(t));

				auto& res = std::get<std::reference_wrapper<type>>(t);
				REQUIRE(before_type == &res.get());
			}
		}

		WHEN("a type is added to the environment within a namespace")
		{
			auto before_type = atom_type("std.i32");
			t_env.add_module(fe::typecheck_environment{ "std" });
			t_env.set_type(fe::extended_ast::identifier{ {"std", "i32"} }, unique_type(before_type.copy()));

			THEN("retrieving a type within a namespace should return the correct type")
			{
				auto after_type_or_error = t_env.typeof(fe::extended_ast::identifier{ {"std", "i32"} });
				REQUIRE(std::holds_alternative<std::reference_wrapper<type>>(after_type_or_error));

				auto& after_type = std::get<std::reference_wrapper<type>>(after_type_or_error).get();
				REQUIRE(before_type == &after_type);
			}
		}

		WHEN("a named product type is added to the environment")
		{
			auto element_one = std::make_pair("a", unique_type(new atom_type("std.i32")));
			auto element_two = std::make_pair("b", unique_type(new atom_type("std.str")));
			auto before_type = product_type();
			before_type.product.push_back(std::move(element_one));
			before_type.product.push_back(std::move(element_two));

			t_env.set_type(fe::extended_ast::identifier{ {"x"} }, unique_type(before_type.copy()));

			THEN("retrieving the type of an element of the product should return the correct type")
			{
				auto after_type_or_error = t_env.typeof(fe::extended_ast::identifier{ {"x", "a" } });

				REQUIRE(std::holds_alternative<std::reference_wrapper<type>>(after_type_or_error));
				auto& after_type = std::get<std::reference_wrapper<type>>(after_type_or_error).get();

				REQUIRE(dynamic_cast<atom_type*>(&after_type));
				auto pt = dynamic_cast<atom_type*>(&after_type);

				REQUIRE(*before_type.product.at(0).second.get() == pt);
			}
		}

		WHEN("a nested product type is added to the environment")
		{
			using namespace fe;
			t_env = typecheck_environment();

			auto element_one = std::make_pair("a", types::make_unique(types::atom_type("std.i32")));
			auto element_two_product = types::product_type();
			element_two_product.product.push_back({ "c", types::make_unique(types::atom_type("std.str")) });

			auto element_two = std::make_pair("b", types::make_unique(std::move(element_two_product)));

			auto before_product = types::product_type();
			before_product.product.push_back(std::move(element_one));
			before_product.product.push_back(std::move(element_two));
			auto before_type = types::make_unique(std::move(before_product));

			t_env.set_type(extended_ast::identifier{ {"x"} }, types::unique_type(before_type->copy()));

			THEN("building the access pattern should calculate the correct offsets")
			{
				auto id = fe::extended_ast::identifier{ {"x", "b", "c"} };
				t_env.build_access_pattern(id);
				REQUIRE(id.offsets.size() == 2);
				REQUIRE(id.offsets.at(0) == 1);
				REQUIRE(id.offsets.at(1) == 0);
			}
		}
	}
}
