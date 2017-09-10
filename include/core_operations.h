#pragma once

namespace fe
{
	namespace core
	{
		namespace operations
		{
			static std::tuple<runtime_environment, typecheck_environment> load()
			{
				runtime_environment re{};
				typecheck_environment te{};
				te.set_type("add", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ { {"a", fe::types::name_type{{"std", "i32"}}}, { "b", fe::types::name_type{{"std", "i32"}}} }}), fe::types::make_unique(fe::types::integer_type{})));
				re.set_value("add", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val + b.val);
				}));
				te.set_type("sub", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ { {"a", fe::types::integer_type{}}, { "b", fe::types::integer_type{}} } }), fe::types::make_unique(fe::types::integer_type{})));
				re.set_value("sub", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val - b.val);
				}));
				te.set_type("mul", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ { {"a", fe::types::integer_type{}}, { "b", fe::types::integer_type{}} } }), fe::types::make_unique(fe::types::integer_type{})));
				re.set_value("mul", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val * b.val);
				}));
				te.set_type("div", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ { {"a", fe::types::integer_type{}}, { "b", fe::types::integer_type{}} } }), fe::types::make_unique(fe::types::integer_type{})));
				re.set_value("div", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val / b.val);
				}));
				te.set_type("lt", fe::types::function_type(fe::types::make_unique(fe::types::product_type{  { {"a", fe::types::integer_type{}}, { "b", fe::types::integer_type{}} } }), fe::types::make_unique(fe::types::boolean_type{})));
				re.set_value("lt", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::boolean(a.val < b.val);
				}));
				te.set_type("gte", fe::types::function_type(fe::types::make_unique(fe::types::product_type{  { {"a", fe::types::integer_type{}}, { "b", fe::types::integer_type{}} } }), fe::types::make_unique(fe::types::boolean_type{})));
				re.set_value("gte", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::boolean(a.val >= b.val);
				}));
				return { re, te };
			}
		}
	}
}
