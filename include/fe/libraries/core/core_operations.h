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
			template<class LhsData, class RhsData, class Op, class ResData>
			static values::unique_value bin_op(values::unique_value val)
			{
				auto t = dynamic_cast<values::tuple*>(val.get());

				auto a = dynamic_cast<LhsData*>(t->val[0].get());
				auto b = dynamic_cast<RhsData*>(t->val[1].get());

				return values::make_unique(ResData(Op()(*a, *b)));
			}

			template<class LhsData, class RhsData, class Op, class ResData>
			static void add_bin_op(runtime_environment& re, type_environment& te, resolution::scope_environment& se, 
				std::string name, types::type& from, types::type& to)
			{
				se.get_root().declare_var_id(name, extended_ast::identifier("_function"));
				se.get_root().define_var_id(name);
				te.set_type(extended_ast::identifier(name), types::make_unique(types::function_type(from, to)));
				re.set_value(name, values::make_unique(values::native_function(bin_op<LhsData, RhsData, Op, ResData>)));
			}

			template<class InData, class Op, class ResData>
			static void add_bin_op(runtime_environment& re, type_environment& te, resolution::scope_environment& se, 
				std::string name, types::type& from, types::type& to)
			{
				add_bin_op<InData, InData, Op, ResData>(re, te, se, name, from, to);
			}

			template<class Elem>
			struct get_op
			{
				decltype(Elem::val) operator()(values::tuple& t, values::i32& i)
				{
					return dynamic_cast<Elem*>(t.val.at(i.val).get())->val;
				}
			};

			static std::tuple<runtime_environment, type_environment, resolution::scope_environment> load()
			{
				runtime_environment re;
				type_environment te;
				resolution::scope_environment se;

				{
					auto from = types::product_type();
					from.product.emplace_back(types::make_unique(types::i32()));
					from.product.emplace_back(types::make_unique(types::i32()));

					add_bin_op<values::i32, std::equal_to<values::i32>, values::boolean>(re, te, se, "eq", from, types::boolean());
					add_bin_op<values::i32, std::less<values::i32>, values::boolean>(re, te, se, "lt", from, types::boolean());
					add_bin_op<values::i32, std::less_equal<values::i32>, values::boolean>(re, te, se, "lte", from, types::boolean());
					add_bin_op<values::i32, std::greater<values::i32>, values::boolean>(re, te, se, "gt", from, types::boolean());
					add_bin_op<values::i32, std::greater_equal<values::i32>, values::boolean>(re, te, se, "gte", from, types::boolean());

					add_bin_op<values::i32, std::minus<values::i32>, values::i32>(re, te, se, "sub", from, types::i32());
					add_bin_op<values::i32, std::plus<values::i32>, values::i32>(re, te, se, "add", from, types::i32());
					add_bin_op<values::i32, std::multiplies<values::i32>, values::i32>(re, te, se, "mul", from, types::i32());
					add_bin_op<values::i32, std::divides<values::i32>, values::i32>(re, te, se, "div", from, types::i32());
					add_bin_op<values::i32, std::modulus<values::i32>, values::i32>(re, te, se, "mod", from, types::i32());
				}

				{
					auto from = types::product_type();
					from.product.emplace_back(types::make_unique(types::i64()));
					from.product.emplace_back(types::make_unique(types::i64()));

					//add_bin_op<values::i64, std::equal_to<values::i64>, values::boolean>(re, te, se, "eq", from, types::boolean());
				}


				auto from = types::product_type();
				from.product.emplace_back(types::make_unique(types::array_type(types::i32())));
				from.product.emplace_back(types::make_unique(types::i32()));
				add_bin_op<values::tuple, values::i32, get_op<values::i32>, values::i32>(re, te, se, "get", from, types::i32());

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
