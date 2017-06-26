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
				auto inte = std::make_unique<core_ast::integer>(values::integer(4));

				auto lines = std::vector<std::unique_ptr<core_ast::node>>{
					std::move(inte)
				};

				auto code = std::make_unique<core_ast::tuple>(
					std::move(lines)
					);


				return std::move<std::unordered_map<std::string, std::shared_ptr<values::value>>>({
					{
						"Terminal",
						std::move(std::make_shared<values::function>(
							std::move(std::vector<core_ast::identifier>{
								core_ast::identifier("x")
							}),
							std::move(code)
						))
					}
				});
			}
		};
	}
}