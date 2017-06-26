#pragma once
#include "module.h"
#include "values.h"
#include "core_ast.h"

namespace fe
{
	namespace modules
	{
		class language_api : public language::module
		{
		public:
			virtual std::unordered_map<std::string, types::type> get_types() override
			{
				return {
					{ 
						"Terminal", 
						types::function_type(
							types::product_type(), 
							types::product_type({ types::integer_type() })
						) 
					}
				};
			}

			virtual std::unordered_map<std::string, std::shared_ptr<values::value>> get_values() override
			{
				// TODO write in extended ast
				auto lines = std::vector<std::unique_ptr<core_ast::node>>();
				lines.push_back(std::make_unique<core_ast::integer>(values::integer(4)));

				auto code = std::make_unique<core_ast::tuple>(std::move(lines));

				auto params = std::vector<core_ast::identifier>();
				params.push_back(core_ast::identifier("x", types::integer_type()));

				auto values = std::unordered_map<std::string, std::shared_ptr<values::value>>();
				values.insert(std::make_pair("Terminal", std::make_shared<values::function>(
					std::move(params),
					std::move(code)
				)));
				return std::move(values);
			}
		};
	}
}