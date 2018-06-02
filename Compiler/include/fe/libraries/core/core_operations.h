#pragma once
#include "fe/data/scope.h"

namespace fe::core::operations
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
	static void add_bin_op(value_scope& re, ext_ast::type_scope& te, ext_ast::name_scope& se,
		std::string name, types::type& from, types::type& to)
	{
		name += " ";
		name += from.operator std::string();
		name += " -> ";
		name += to.operator std::string();
		se.declare_variable(name);
		se.define_variable(name);
		te.set_type(name, types::make_unique(types::function_type(from, to)));
		re.set_value(name, values::make_unique(values::native_function(bin_op<LhsData, RhsData, Op, ResData>)));
	}

	template<class InData, class Op, class ResData>
	static void add_bin_op(value_scope& re, ext_ast::type_scope& te, ext_ast::name_scope& se,
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

	template<class InData, class Op, class ResData>
	static values::unique_value un_op(values::unique_value val)
	{
		auto t = dynamic_cast<InData*>(val.get());
		return values::make_unique(ResData(Op()(*t)));
	}

	template<class InData, class Op, class ResData>
	static void add_un_op(value_scope& re, ext_ast::type_scope& te, ext_ast::name_scope& se,
		std::string name, types::type& from, types::type& to)
	{
		name += " ";
		name += from.operator std::string();
		name += " -> ";
		name += to.operator std::string();
		se.declare_variable(name);
		se.define_variable(name);
		te.set_type(name, types::make_unique(types::function_type(from, to)));
		re.set_value(name, values::make_unique(values::native_function(un_op<InData, Op, ResData>)));
	}


	static scope load()
	{
		ext_ast::type_scope te;
		ext_ast::name_scope se;
		value_scope re;

		{
			auto from = types::product_type();
			from.product.emplace_back(types::make_unique(types::i64()));
			from.product.emplace_back(types::make_unique(types::i64()));

			add_bin_op<values::i64, std::equal_to<values::i64>, values::boolean>(re, te, se, "eq", from, types::boolean());
			add_bin_op<values::i64, std::less<values::i64>, values::boolean>(re, te, se, "lt", from, types::boolean());
			add_bin_op<values::i64, std::less_equal<values::i64>, values::boolean>(re, te, se, "lte", from, types::boolean());
			add_bin_op<values::i64, std::greater<values::i64>, values::boolean>(re, te, se, "gt", from, types::boolean());
			add_bin_op<values::i64, std::greater_equal<values::i64>, values::boolean>(re, te, se, "gte", from, types::boolean());

			add_bin_op<values::i64, std::minus<values::i64>, values::i64>(re, te, se, "sub", from, types::i64());
			add_bin_op<values::i64, std::plus<values::i64>, values::i64>(re, te, se, "add", from, types::i64());
			add_bin_op<values::i64, std::multiplies<values::i64>, values::i64>(re, te, se, "mul", from, types::i64());
			add_bin_op<values::i64, std::divides<values::i64>, values::i64>(re, te, se, "div", from, types::i64());
			add_bin_op<values::i64, std::modulus<values::i64>, values::i64>(re, te, se, "mod", from, types::i64());
		}

		{
			auto from = types::product_type();
			from.product.emplace_back(types::make_unique(types::array_type(types::i32())));
			from.product.emplace_back(types::make_unique(types::i32()));
			add_bin_op<values::tuple, values::i32, get_op<values::i32>, values::i32>(re, te, se, "get", from, types::i32());
		}

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
			from.product.emplace_back(types::make_unique(types::f32()));
			from.product.emplace_back(types::make_unique(types::f32()));

			add_bin_op<values::f32, std::equal_to<values::f32>, values::boolean>(re, te, se, "eq", from, types::boolean());
			add_bin_op<values::f32, std::less<values::f32>, values::boolean>(re, te, se, "lt", from, types::boolean());
			add_bin_op<values::f32, std::less_equal<values::f32>, values::boolean>(re, te, se, "lte", from, types::boolean());
			add_bin_op<values::f32, std::greater<values::f32>, values::boolean>(re, te, se, "gt", from, types::boolean());
			add_bin_op<values::f32, std::greater_equal<values::f32>, values::boolean>(re, te, se, "gte", from, types::boolean());

			add_bin_op<values::f32, std::minus<values::f32>, values::f32>(re, te, se, "sub", from, types::f32());
			add_bin_op<values::f32, std::plus<values::f32>, values::f32>(re, te, se, "add", from, types::f32());
			add_bin_op<values::f32, std::multiplies<values::f32>, values::f32>(re, te, se, "mul", from, types::f32());
			add_bin_op<values::f32, std::divides<values::f32>, values::f32>(re, te, se, "div", from, types::f32());
		}

		{
			auto from = types::boolean();
			add_un_op<values::boolean, std::logical_not<values::boolean>, values::boolean>(re, te, se, "not", from, types::boolean());
		}


		return scope(re, te, se);
	}
}
