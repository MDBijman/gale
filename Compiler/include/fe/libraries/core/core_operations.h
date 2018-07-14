#pragma once
#include "fe/data/module.h"

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
	static void add_bin_op(constants_store& cs, core_ast::value_scope& re, ext_ast::type_scope& te, ext_ast::name_scope& se,
		std::string op_name, types::type& from, types::type& to)
	{
		auto& ref = cs.get<plain_identifier>(cs.create<plain_identifier>());
		ref.full = op_name + " " + from.operator std::string() + " -> " + to.operator std::string();
		se.declare_variable(ref.full);
		se.define_variable(ref.full);
		te.set_type(ref.full, types::make_unique(types::function_type(from, to)));
		re.set_value(ref.full, values::make_unique(values::native_function(bin_op<LhsData, RhsData, Op, ResData>)));
	}

	template<class InData, class Op, class ResData>
	static void add_bin_op(constants_store& cs, core_ast::value_scope& re, ext_ast::type_scope& te, ext_ast::name_scope& se,
		std::string name, types::type& from, types::type& to)
	{
		add_bin_op<InData, InData, Op, ResData>(cs, re, te, se, name, from, to);
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
	static void add_un_op(constants_store& cs, core_ast::value_scope& re, ext_ast::type_scope& te, ext_ast::name_scope& se,
		std::string op_name, types::type& from, types::type& to)
	{
		auto& ref = cs.get<plain_identifier>(cs.create<plain_identifier>());
		ref.full = op_name + " " + from.operator std::string() + " -> " + to.operator std::string();
		se.declare_variable(ref.full);
		se.define_variable(ref.full);
		te.set_type(ref.full, types::make_unique(types::function_type(from, to)));
		re.set_value(ref.full, values::make_unique(values::native_function(un_op<InData, Op, ResData>)));
	}

	static module load()
	{
		ext_ast::type_scope te;
		ext_ast::name_scope se;
		core_ast::value_scope re;
		constants_store constants;

		{
			auto from = types::product_type();
			from.product.emplace_back(types::make_unique(types::array_type(types::i32())));
			from.product.emplace_back(types::make_unique(types::i32()));
			add_bin_op<values::tuple, values::i32, get_op<values::i32>, values::i32>(constants, re, te, se, "get", from, types::i32());
		}
		{
			auto from = types::boolean();
			add_un_op<values::boolean, std::logical_not<values::boolean>, values::boolean>(constants, re, te, se, "not", from, types::boolean());
		}

		return module{
			{"_core"},
			std::move(re),
			std::move(te),
			std::move(se),
			std::move(constants)
		};
	}
}
