#pragma once
#include "typecheck_environment.h"
#include "test.h"
#include "types.h"

#include <variant>

namespace tests
{
	struct typecheck_environment_tests : public test
	{
		fe::typecheck_environment t_env;

		typecheck_environment_tests() : test("typecheck_environment")
		{
		}
		
		void run_all() override
		{
			std::cout << "Testing typecheck environment\n";
			test_types();
			test_module();
			test_nested_module();
			test_product_type();
			test_build_access_pattern();
		}

		void test_types()
		{
			using namespace fe;
			t_env = typecheck_environment();
			auto before_type = types::atom_type{ "i32" };

			t_env.add_module(typecheck_environment{});
			t_env.set_type(extended_ast::identifier{ {"i32"} }, before_type);

			auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"i32"} });
			assert(!std::holds_alternative<type_env_error>(after_type_or_error));

			auto after_type = std::get<std::reference_wrapper<const types::type>>(after_type_or_error);
			assert(types::type(before_type) == after_type.get());
		}

		void test_module()
		{
			using namespace fe;
			t_env = typecheck_environment();
			auto before_type = types::atom_type{ "i32" };

			t_env.add_module(typecheck_environment{ "std" });
			t_env.set_type(extended_ast::identifier{ {"std", "i32"} }, before_type);

			auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"std", "i32"} });
			assert(!std::holds_alternative<type_env_error>(after_type_or_error));

			auto after_type = std::get<std::reference_wrapper<const types::type>>(after_type_or_error);
			assert(types::type(before_type) == after_type.get());
		}

		void test_nested_module()
		{
			using namespace fe;
			t_env = typecheck_environment();
			auto std_env = typecheck_environment{ "std" };
			auto child_env = typecheck_environment{"child"};
			std_env.add_module(std::move(child_env));
			t_env.add_module(std::move(std_env));

			auto before_type = types::atom_type{ "i32" };
			t_env.set_type(extended_ast::identifier{ {"std", "child", "x"} }, before_type);

			auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"std", "child", "x"} });
			assert(!std::holds_alternative<type_env_error>(after_type_or_error));

			auto after_type = std::get<std::reference_wrapper<const types::type>>(after_type_or_error);
			assert(types::type(before_type) == after_type.get());
		}

		void test_product_type()
		{
			using namespace fe;
			t_env = typecheck_environment();

			auto element_one = std::make_pair("a", types::atom_type("i32"));
			auto element_two = std::make_pair("b", types::atom_type("str"));
			auto before_type = types::product_type({ element_one, element_two });

			t_env.set_type(extended_ast::identifier{ {"x"} }, before_type);

			{
				auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"x", "a" } });
				assert(!std::holds_alternative<type_env_error>(after_type_or_error));

				auto after_type = std::get<std::reference_wrapper<const types::type>>(after_type_or_error);
				assert(types::type(after_type) == types::type(element_one.second));
			}
			{
				auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"x", "b" } });
				assert(!std::holds_alternative<type_env_error>(after_type_or_error));

				auto after_type = std::get<std::reference_wrapper<const types::type>>(after_type_or_error);
				assert(types::type(after_type) == types::type(element_two.second));
			}
		}

		void test_build_access_pattern()
		{
			using namespace fe;
			t_env = typecheck_environment();

			auto element_one = std::make_pair("a", types::atom_type("i32"));
			auto element_two = std::make_pair("b", types::product_type({ { "c", types::atom_type("str") } }));
			auto before_type = types::product_type({ element_one, element_two });

			t_env.set_type(extended_ast::identifier{ {"x"} }, before_type);

			auto id = extended_ast::identifier{ {"x", "b", "c"} };
			id = t_env.build_access_pattern(std::move(id));
			assert(id.offsets.size() == 2);
			assert(id.offsets.at(0) == 1);
			assert(id.offsets.at(1) == 0);
		}
	};
}