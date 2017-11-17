#pragma once
#include "typecheck_environment.h"
#include "types.h"

#include <variant>
#include <assert.h>

namespace tests
{
	struct typecheck_environment_tests 
	{
		fe::typecheck_environment t_env;

		typecheck_environment_tests() 
		{
		}
		
		void run_all()
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
			t_env.set_type(extended_ast::identifier{ {"i32"} }, types::make_unique(before_type));

			auto& after_type_or_error = t_env.typeof(extended_ast::identifier{ {"i32"} });
			assert(!std::holds_alternative<type_env_error>(after_type_or_error));

			auto& after_type = std::get<std::reference_wrapper<types::type>>(after_type_or_error).get();
			assert(before_type == &after_type);
		}

		void test_module()
		{
			using namespace fe;
			t_env = typecheck_environment();
			auto before_type = types::atom_type{ "i32" };

			t_env.add_module(typecheck_environment{ "std" });
			t_env.set_type(extended_ast::identifier{ {"std", "i32"} }, types::make_unique(before_type));

			auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"std", "i32"} });
			assert(!std::holds_alternative<type_env_error>(after_type_or_error));

			auto& after_type = std::get<std::reference_wrapper<types::type>>(after_type_or_error).get();
			assert(before_type == &after_type);
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
			t_env.set_type(extended_ast::identifier{ {"std", "child", "x"} }, types::make_unique(before_type));

			auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"std", "child", "x"} });
			assert(!std::holds_alternative<type_env_error>(after_type_or_error));

			auto& after_type = std::get<std::reference_wrapper<types::type>>(after_type_or_error).get();
			assert(before_type == &after_type);
		}

		void test_product_type()
		{
			using namespace fe;
			t_env = typecheck_environment();

			auto element_one = std::make_pair("a", types::make_unique(types::atom_type("i32")));
			auto element_two = std::make_pair("b", types::make_unique(types::atom_type("str")));
			auto before_type = types::product_type();
			before_type.product.push_back(std::move(element_one));
			before_type.product.push_back(std::move(element_two));

			t_env.set_type(extended_ast::identifier{ {"x"} }, types::unique_type(before_type.copy()));

			{
				auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"x", "a" } });
				assert(!std::holds_alternative<type_env_error>(after_type_or_error));

				auto& after_type = std::get<std::reference_wrapper<types::type>>(after_type_or_error).get();
				assert(after_type == before_type.product.at(0).second.get());
			}
			{
				auto after_type_or_error = t_env.typeof(extended_ast::identifier{ {"x", "b" } });
				assert(!std::holds_alternative<type_env_error>(after_type_or_error));

				auto& after_type = std::get<std::reference_wrapper<types::type>>(after_type_or_error).get();
				assert(after_type == before_type.product.at(1).second.get());
			}
		}

		void test_build_access_pattern()
		{
			using namespace fe;
			t_env = typecheck_environment();

			auto element_one = std::make_pair("a", types::make_unique(types::atom_type("i32")));
			auto element_two_product = types::product_type();
			element_two_product.product.push_back({ "c", types::make_unique(types::atom_type("str")) });

			auto element_two = std::make_pair("b", types::make_unique(std::move(element_two_product)));

			auto before_product = types::product_type();
			before_product.product.push_back(std::move(element_one));
			before_product.product.push_back(std::move(element_two));
			auto before_type = types::make_unique(std::move(before_product));

			t_env.set_type(extended_ast::identifier{ {"x"} }, types::unique_type(before_type->copy()));

			auto id = extended_ast::identifier{ {"x", "b", "c"} };
			t_env.build_access_pattern(id);
			assert(id.offsets.size() == 2);
			assert(id.offsets.at(0) == 1);
			assert(id.offsets.at(1) == 0);
		}
	};
}