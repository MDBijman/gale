#pragma once
#include "fe/modes/module.h"
#include "fe/data/runtime_environment.h"
#include "fe/data/typecheck_environment.h"

namespace fe
{
	namespace core
	{
		namespace operations
		{
			static std::tuple<runtime_environment, typecheck_environment> load()
			{
				runtime_environment re;
				typecheck_environment te;

				using namespace fe::types;
				using namespace fe::values;

				auto from = product_type();
				from.product.emplace_back("a", types::make_unique(atom_type{"i32"}));
				from.product.emplace_back("b", types::make_unique(atom_type{"i32"}));


				te.set_type("add", types::make_unique(function_type(from, atom_type{ "i32" })));
				re.set_value("add", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val + b->val));
				})));
				te.set_type("sub", types::make_unique(function_type(from, atom_type{ "i32" })));
				re.set_value("sub", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val - b->val));
				})));
				te.set_type("mul", types::make_unique(function_type(from, atom_type{ "i32" })));
				re.set_value("mul", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val * b->val));
				})));
				te.set_type("div", types::make_unique(function_type(from, atom_type{ "i32" })));
				re.set_value("div", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(integer(a->val / b->val));
				})));
				te.set_type("lt", types::make_unique(function_type(from, atom_type{ "i32" })));
				re.set_value("lt", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(boolean(a->val < b->val));
				})));
				te.set_type("gte", types::make_unique(function_type(from, atom_type{ "boolean" })));
				re.set_value("gte", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(boolean(a->val >= b->val));
				})));
				te.set_type("eq", types::make_unique(function_type(from, atom_type{ "boolean" })));
				re.set_value("eq", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<integer*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return values::make_unique(boolean(a->val == b->val));
				})));
				te.set_type("get", types::make_unique(function_type(from, atom_type{ "i32" })));
				re.set_value("get", values::make_unique(native_function([](unique_value val) -> unique_value {
					auto t = dynamic_cast<tuple*>(val.get());

					auto a = dynamic_cast<tuple*>(t->content[0].get());
					auto b = dynamic_cast<integer*>(t->content[1].get());

					return unique_value(a->content.at(b->val)->copy());
				})));

				return { re, te };
			}

			static native_module* load_as_module()
			{
				auto[re, te] = load();
				return new native_module("core", std::move(re), std::move(te));
			}
		}
	}
}
