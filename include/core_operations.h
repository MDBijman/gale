#pragma once

namespace fe
{
	namespace core
	{
		namespace operations
		{
			static environment load()
			{
				environment e{};
				e.set_type("_add", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ {fe::types::integer_type{}, fe::types::integer_type{}} }), fe::types::make_unique(fe::types::integer_type{})));
				e.set_value("_add", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val + b.val);
				}));
				e.set_type("_subtract", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ {fe::types::integer_type{}, fe::types::integer_type{}} }), fe::types::make_unique(fe::types::integer_type{})));
				e.set_value("_subtract", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val - b.val);
				}));
				e.set_type("_multiply", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ {fe::types::integer_type{}, fe::types::integer_type{}} }), fe::types::make_unique(fe::types::integer_type{})));
				e.set_value("_multiply", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val * b.val);
				}));
				e.set_type("_divide", fe::types::function_type(fe::types::make_unique(fe::types::product_type{ {fe::types::integer_type{}, fe::types::integer_type{}} }), fe::types::make_unique(fe::types::integer_type{})));
				e.set_value("_divide", fe::values::native_function([](fe::values::value t) -> fe::values::value {
					auto& tuple = std::get<fe::values::tuple>(t);

					auto a = std::get<fe::values::integer>(tuple.content[0]);
					auto b = std::get<fe::values::integer>(tuple.content[1]);

					return fe::values::integer(a.val / b.val);
				}));
				return e;
			}
		}
	}
}
