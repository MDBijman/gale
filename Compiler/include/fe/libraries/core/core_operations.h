#pragma once
#include "fe/data/module.h"

namespace fe::core::operations
{
	static void add_bin_op(constants_store& cs, ext_ast::type_scope& te, ext_ast::name_scope& se,
		std::string op_name, types::type& from, types::type& to)
	{
		auto& ref = cs.get<plain_identifier>(cs.create<plain_identifier>());
		ref.full = op_name + " " + from.operator std::string() + " -> " + to.operator std::string();
		se.declare_variable(ref.full);
		se.define_variable(ref.full);
		te.set_type(ref.full, types::make_unique(types::function_type(from, to)));
	}

	static void add_un_op(constants_store& cs, ext_ast::type_scope& te, ext_ast::name_scope& se,
		std::string op_name, types::type& from, types::type& to)
	{
		auto& ref = cs.get<plain_identifier>(cs.create<plain_identifier>());
		ref.full = op_name + " " + from.operator std::string() + " -> " + to.operator std::string();
		se.declare_variable(ref.full);
		se.define_variable(ref.full);
		te.set_type(ref.full, types::make_unique(types::function_type(from, to)));
	}

	static module load()
	{
		ext_ast::type_scope te;
		ext_ast::name_scope se;
		constants_store constants;

		{
			auto from = types::product_type();
			from.product.emplace_back(types::make_unique(types::i64()));
			from.product.emplace_back(types::make_unique(types::i64()));

			add_bin_op(constants, te, se, "eq", from, types::boolean());
			add_bin_op(constants, te, se, "lt", from, types::boolean());
			add_bin_op(constants, te, se, "lte", from, types::boolean());
			add_bin_op(constants, te, se, "gt", from, types::boolean());
			add_bin_op(constants, te, se, "gte", from, types::boolean());

			add_bin_op(constants, te, se, "sub", from, types::i64());
			add_bin_op(constants, te, se, "add", from, types::i64());
			add_bin_op(constants, te, se, "mul", from, types::i64());
			add_bin_op(constants, te, se, "div", from, types::i64());
			add_bin_op(constants, te, se, "mod", from, types::i64());
		}

		{
			auto from = types::product_type();
			from.product.emplace_back(types::make_unique(types::array_type(types::i32())));
			from.product.emplace_back(types::make_unique(types::i32()));
			add_bin_op(constants, te, se, "get", from, types::i32());
		}

		{
			auto from = types::product_type();
			from.product.emplace_back(types::make_unique(types::i32()));
			from.product.emplace_back(types::make_unique(types::i32()));

			add_bin_op(constants, te, se, "eq", from, types::boolean());
			add_bin_op(constants, te, se, "lt", from, types::boolean());
			add_bin_op(constants, te, se, "lte", from, types::boolean());
			add_bin_op(constants, te, se, "gt", from, types::boolean());
			add_bin_op(constants, te, se, "gte", from, types::boolean());

			add_bin_op(constants, te, se, "sub", from, types::i32());
			add_bin_op(constants, te, se, "add", from, types::i32());
			add_bin_op(constants, te, se, "mul", from, types::i32());
			add_bin_op(constants, te, se, "div", from, types::i32());
			add_bin_op(constants, te, se, "mod", from, types::i32());
		}

		{
			auto from = types::product_type();
			from.product.emplace_back(types::make_unique(types::f32()));
			from.product.emplace_back(types::make_unique(types::f32()));

			add_bin_op(constants, te, se, "eq", from, types::boolean());
			add_bin_op(constants, te, se, "lt", from, types::boolean());
			add_bin_op(constants, te, se, "lte", from, types::boolean());
			add_bin_op(constants, te, se, "gt", from, types::boolean());
			add_bin_op(constants, te, se, "gte", from, types::boolean());

			add_bin_op(constants, te, se, "sub", from, types::f32());
			add_bin_op(constants, te, se, "add", from, types::f32());
			add_bin_op(constants, te, se, "mul", from, types::f32());
			add_bin_op(constants, te, se, "div", from, types::f32());
		}

		{
			auto from = types::boolean();
			add_un_op(constants, te, se, "not", from, types::boolean());
		}


		return module{
			{"_core"},
			std::move(te),
			std::move(se),
			std::move(constants)
		};
	}
}
