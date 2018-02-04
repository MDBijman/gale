#pragma once
#include "fe/modes/module.h"
#include "fe/data/runtime_environment.h"
#include "fe/data/type_environment.h"

namespace fe
{
	namespace core
	{
		namespace operations
		{
			static std::tuple<runtime_environment, type_environment, scope_environment> load()
			{
				runtime_environment re;
				type_environment te;
				scope_environment se;

				using namespace fe::types;
				using namespace fe::values;

				auto from = product_type();
				from.product.emplace_back(types::make_unique(atom_type{"std.i32"}));
				from.product.emplace_back(types::make_unique(atom_type{"std.i32"}));

				se.declare(extended_ast::identifier({ "add" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "add" }));
				te.set_type(extended_ast::identifier({ "add" }),
					types::make_unique(function_type(from, atom_type{ "std.i32" })));
				re.set_value("add", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val + b->val));
				})));


				se.declare(extended_ast::identifier({ "sub" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "sub" }));
				te.set_type(extended_ast::identifier({ "sub" }),
					types::make_unique(function_type(from, atom_type{ "std.i32" })));
				re.set_value("sub", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val - b->val));
				})));

				se.declare(extended_ast::identifier({ "mul" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "mul" }));
				te.set_type(extended_ast::identifier({ "mul" }),
					types::make_unique(function_type(from, atom_type{ "std.i32" })));
				re.set_value("mul", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val * b->val));
				})));

				se.declare(extended_ast::identifier({ "div" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "div" }));
				te.set_type(extended_ast::identifier({ "div" }),
					types::make_unique(function_type(from, atom_type{ "std.i32" })));
				re.set_value("div", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val / b->val));
				})));

				se.declare(extended_ast::identifier({ "lt" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "lt" }));
				te.set_type(extended_ast::identifier({ "lt" }),
					types::make_unique(function_type(from, atom_type{ "std.i32" })));
				re.set_value("lt", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(boolean(a->val < b->val));
				})));

				se.declare(extended_ast::identifier({ "gte" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "gte" }));
				te.set_type(extended_ast::identifier({ "gte" }),
					types::make_unique(function_type(from, atom_type{ "boolean" })));
				re.set_value("gte", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(boolean(a->val >= b->val));
				})));

				se.declare(extended_ast::identifier({ "eq" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "eq" }));
				te.set_type(extended_ast::identifier({ "eq" }),
					types::make_unique(function_type(from, atom_type{ "boolean" })));
				re.set_value("eq", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(boolean(a->val == b->val));
				})));

				se.declare(extended_ast::identifier({ "get" }), extended_ast::identifier({ "_function" }));
				se.define(extended_ast::identifier({ "get" }));
				te.set_type(extended_ast::identifier({ "get" }),
					types::make_unique(function_type(from, atom_type{ "std.i32" })));
				re.set_value("get", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<tuple*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return unique_value(a->content.at(b->val)->copy());
				})));

				return { re, te, se };
			}

			static native_module* load_as_module()
			{
				auto[re, te, se] = load();
				return new native_module("core", std::move(re), std::move(te), std::move(se));
			}
		}
	}
}
