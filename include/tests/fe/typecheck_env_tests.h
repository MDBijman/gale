#pragma once
#include "fe/data/type_environment.h"
#include "fe/data/types.h"

#include <catch2/catch.hpp>
#include <variant>

/*
SCENARIO("type environments can manage types", "[type_environment]")
{
	using namespace fe::types;

	GIVEN("an empty type environment")
	{
		fe::type_environment t_env;

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
			t_env.add_module(fe::type_environment{ "std" });
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
			auto element_one = std::unique_ptr<type>(std::make_unique<atom_type>("std.i32"));
			auto element_two = std::unique_ptr<type>(std::make_unique<atom_type>("std.str"));
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

				REQUIRE(*before_type.product.at(0).get() == pt);
			}
		}
	}
}
*/
