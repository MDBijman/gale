#include "fe/pipeline/typechecker_stage.h"
#include "fe/data/ast_data.h"
#include "fe/data/ext_ast.h"
#include "fe/data/type_scope.h"
#include "fe/pipeline/error.h"

namespace fe::ext_ast
{
	// Helper
	static void copy_parent_scope(node &n, ast &ast)
	{
		auto &parent = ast.get_node(n.parent_id);
		assert(parent.type_scope_id != no_scope);
		n.type_scope_id = parent.type_scope_id;
	}

	static bool has_children(node &n, ast &ast)
	{
		if (n.children_id == no_children) return false;
		return ast.get_children(n.children_id).size() > 0;
	}

	void typecheck(node &n, ast &ast);

	// Typeof
	types::unique_type typeof_identifier(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(!has_children(n, ast));
		copy_parent_scope(n, ast);

		auto &scope = ast.get_type_scope(n.type_scope_id);
		auto &id = ast.get_data<identifier>(n.data_index);
		auto res = scope.resolve_variable(id, ast.type_scope_cb());
		assert(res);

		auto t = types::unique_type(res->type.copy());
		if (!tc.satisfied_by(*t))
			throw typecheck_error{ "Identifier " + id.operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };

		return t;
	}

	types::unique_type typeof_number(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::NUMBER);
		assert(!has_children(n, ast));
		copy_parent_scope(n, ast);

		auto &number_data = ast.get_data<number>(n.data_index);

		if (tc.satisfied_by(types::i8()))
		{
			assert(number_data.value >= std::numeric_limits<int8_t>::min());
			assert(number_data.value <= std::numeric_limits<int8_t>::max());
			number_data.type = number_type::I8;
			return types::unique_type(new types::i8());
		}

		if (tc.satisfied_by(types::ui8()))
		{
			assert(number_data.value >= std::numeric_limits<uint8_t>::min());
			assert(number_data.value <= std::numeric_limits<uint8_t>::max());
			number_data.type = number_type::UI8;
			return types::unique_type(new types::ui8());
		}

		if (tc.satisfied_by(types::i16()))
		{
			assert(number_data.value >= std::numeric_limits<int16_t>::min());
			assert(number_data.value <= std::numeric_limits<int16_t>::max());
			number_data.type = number_type::I16;
			return types::unique_type(new types::i16());
		}

		if (tc.satisfied_by(types::ui16()))
		{
			assert(number_data.value >= std::numeric_limits<uint16_t>::min());
			assert(number_data.value <= std::numeric_limits<uint16_t>::max());
			number_data.type = number_type::UI16;
			return types::unique_type(new types::ui16());
		}

		if (tc.satisfied_by(types::i32()))
		{
			assert(number_data.value >= std::numeric_limits<int32_t>::min());
			assert(number_data.value <= std::numeric_limits<int32_t>::max());
			number_data.type = number_type::I32;
			return types::unique_type(new types::i32());
		}

		if (tc.satisfied_by(types::ui32()))
		{
			assert(number_data.value >= std::numeric_limits<int32_t>::min());
			assert(number_data.value <= std::numeric_limits<int32_t>::max());
			number_data.type = number_type::UI32;
			return types::unique_type(new types::ui32());
		}

		if (tc.satisfied_by(types::i64()))
		{
			assert(number_data.value >= std::numeric_limits<int64_t>::min());
			assert(number_data.value <= std::numeric_limits<int64_t>::max());
			number_data.type = number_type::I64;
			return types::unique_type(new types::i64());
		}

		if (tc.satisfied_by(types::ui64()))
		{
			assert(number_data.value >= std::numeric_limits<uint64_t>::min());
			assert(number_data.value <= std::numeric_limits<uint64_t>::max());
			number_data.type = number_type::UI64;
			return types::unique_type(new types::ui64());
		}

		if (tc.satisfied_by(types::f32()))
		{
			return types::unique_type(new types::f32());
		}

		throw typecheck_error{ "Cannot infer number literal type" };
	}

	types::unique_type typeof_boolean(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(!has_children(n, ast));

		auto t = types::unique_type(new types::boolean());
		if (!tc.satisfied_by(*t))
			throw typecheck_error{ "boolean type does not match constraints\n" +
					       tc.operator std::string() };
		return t;
	}

	types::unique_type typeof_string(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::STRING);
		assert(!has_children(n, ast));
		copy_parent_scope(n, ast);

		auto t = types::unique_type(new types::str());
		if (!tc.satisfied_by(*t))
			throw typecheck_error{ "string type does not match constraints\n" +
					       tc.operator std::string() };
		return t;
	}

	types::unique_type typeof_tuple(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::TUPLE);
		copy_parent_scope(n, ast);

		types::product_type *pt = new types::product_type();
		auto res = types::unique_type(pt);

		size_t index = 0;
		auto &children = ast.children_of(n);
		for (auto child : children)
		{
			auto &child_node = ast.get_node(child);
			auto sub_constraint = tc.tuple_sub_constraints(index);
			if (!sub_constraint)
				throw typecheck_error{ "tuple value type cannot match constraint " +
						       tc.operator std::string() +
						       "\nsubconstraints with index " +
						       std::to_string(index) + " do not exist" };
			pt->product.push_back(typeof(child_node, ast, *sub_constraint));

			index++;
		}

		if (!tc.satisfied_by(*res))
			throw typecheck_error{ "Tuple " + res->operator std::string() +
					       " does not match constraints " +
					       tc.operator std::string() };

		return res;
	}

	types::unique_type typeof_array_value(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::ARRAY_VALUE);
		auto &children = ast.children_of(n);
		copy_parent_scope(n, ast);

		if (children.size() > 0)
		{
			auto element_constraints = tc.array_sub_constraints();
			if (!element_constraints)
				throw typecheck_error{ "array value type cannot match constraint " +
						       tc.operator std::string() +
						       "\nsubconstraints do not exist" };

			auto t1 = typeof(ast.get_node(children[0]), ast, *element_constraints);
			for (auto it = children.begin() + 1; it != children.end(); it++)
			{
				auto &child_node = ast.get_node(*it);
				typeof(child_node, ast, *element_constraints);
			}

			auto arr_type = types::array_type(*t1, children.size());
			if (!tc.satisfied_by(arr_type))
				throw typecheck_error{
					"array value type does not satisfy constraints " +
					tc.operator std::string()
				};
			return types::unique_type(arr_type.copy());
		}
		else
		{
			assert(!"todo");
			// #todo set type to void
		}
	}

	types::unique_type typeof_function_call(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &id_node = ast.get_node(children[0]);
		auto res = typeof(id_node, ast);
		auto function_type = dynamic_cast<types::function_type *>(res.get());

		auto &arg_node = ast.get_node(children[1]);
		auto arg_type = typeof(
		  arg_node, ast, type_constraints({ equality_constraint(*function_type->from) }));

		if (!tc.satisfied_by(*function_type->to))
			throw typecheck_error{ "function call type " +
					       function_type->to->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };

		return std::move(function_type->to);
	}

	types::unique_type typeof_array_access(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::ARRAY_ACCESS);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &subj_node = ast.get_node(children[0]);
		auto res = typeof(subj_node, ast, type_constraints());
		auto array_type = dynamic_cast<types::array_type *>(res.get());

		auto &idx_node = ast.get_node(children[1]);
		types::unique_type is_num(new types::ui64);
		auto idx_type =
		  typeof(idx_node, ast, type_constraints({ equality_constraint(*is_num.get()) }));

		if (!tc.satisfied_by(*array_type->element_type))
			throw typecheck_error{ "array access type " +
					       array_type->element_type->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };

		return types::unique_type(array_type->element_type->copy());
	}

	types::unique_type typeof_block(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::BLOCK);
		if (n.parent_id)
		{
			auto &parent = ast.get_node(n.parent_id);
			n.type_scope_id = ast.create_type_scope(parent.type_scope_id);
		}
		// If we dont have a parent we already have a type scope.
		else
			assert(n.type_scope_id != no_scope);

		auto &children = ast.children_of(n);
		if (children.size() == 0) return types::unique_type(new types::voidt());

		// Typecheck all children except for the last one, which might be a return value
		for (auto i = 0; i < children.size() - 1; i++)
		{
			auto &elem_node = ast.get_node(children[i]);
			typecheck(elem_node, ast);
		}

		types::unique_type t;
		auto &last_node = ast.get_node(children.back());
		if (last_node.kind == node_type::BLOCK_RESULT)
		{
			// Block with return value
			t = typeof(last_node, ast, tc);
		}
		else
		{
			// Block with only statements
			typecheck(last_node, ast);
			t = types::unique_type(new types::voidt());
			if (!tc.satisfied_by(*t))
				throw typecheck_error{ "block type " + t->operator std::string() +
						       " does not match constraints\n" +
						       tc.operator std::string() };
		}

		return t;
	}

	types::unique_type typeof_block_result(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::BLOCK_RESULT);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);
		auto &child = ast.get_node(children[0]);
		auto t = typeof(child, ast, tc);
		if (!tc.satisfied_by(*t))
			throw typecheck_error{ "block result type " + t->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };
		return t;
	}

	types::unique_type typeof_type_atom(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::ATOM_TYPE);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);

		auto &id_node = ast.get_node(children[0]);
		auto &id_data = ast.get_data<identifier>(id_node.data_index);

		auto &scope = ast.get_type_scope(n.type_scope_id);
		auto res = scope.resolve_type(id_data, ast.type_scope_cb());
		assert(res);
		auto t = types::unique_type(res->type.copy());
		if (!tc.satisfied_by(*t))
			throw typecheck_error{ "type atom " + t->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };
		return t;
	}

	types::unique_type typeof_binary_op(node &n, ast &ast, type_constraints tc)
	{
		assert(ext_ast::is_binary_op(n.kind));
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &lhs_node = ast.get_node(children[0]);
		auto &rhs_node = ast.get_node(children[1]);

		types::unique_type lhs_type, rhs_type, res_type;

		switch (n.kind)
		{
		case node_type::ADDITION:
		case node_type::SUBTRACTION:
		case node_type::MULTIPLICATION:
		case node_type::DIVISION:
		case node_type::MODULO:
		{
			lhs_type = typeof(lhs_node, ast, tc);
			rhs_type = typeof(rhs_node, ast,
					  type_constraints({ equality_constraint(*lhs_type) }));

			res_type = types::unique_type(lhs_type->copy());
			break;
		}
		case node_type::EQUALITY:
		case node_type::GREATER_OR_EQ:
		case node_type::GREATER_THAN:
		case node_type::LESS_OR_EQ:
		case node_type::LESS_THAN:
		case node_type::AND:
		case node_type::OR:
		{
			lhs_type = typeof(lhs_node, ast);
			rhs_type = typeof(rhs_node, ast,
					  type_constraints({ equality_constraint(*lhs_type) }));

			res_type = types::unique_type(new types::boolean());
			break;
		}
		default: throw std::runtime_error("Typeof for binary op not implemented");
		}

		if (!tc.satisfied_by(*res_type))
			throw typecheck_error{ "binary op type " +
					       res_type->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };
		return res_type;
	}

	types::unique_type typeof_unary_op(node &n, ast &ast, type_constraints tc)
	{
		assert(ext_ast::is_unary_op(n.kind));
		auto &children = ast.children_of(n);
		assert(children.size() == 1);
		copy_parent_scope(n, ast);

		auto &child = ast.get_node(children[0]);

		types::unique_type child_type;

		switch (n.kind)
		{
		case node_type::NOT:
		{
			child_type = typeof(child, ast, tc);
			break;
		}
		default: throw std::runtime_error("Typeof for unary op not implemented");
		}

		if (!tc.satisfied_by(*child_type))
			throw typecheck_error{ "unary op type " +
					       child_type->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };
		return child_type;
	}

	types::unique_type typeof_function_type(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::FUNCTION_TYPE);
		copy_parent_scope(n, ast);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto &from_node = ast.get_node(children[0]);
		// #todo use type constraints
		auto from_constraint = type_constraints();
		auto from_type = typeof(from_node, ast, from_constraint);

		auto &to_node = ast.get_node(children[1]);
		// #todo use type constraints
		auto to_constraint = type_constraints();
		auto to_type = typeof(to_node, ast, to_constraint);

		return types::unique_type(
		  new types::function_type(std::move(from_type), std::move(to_type)));
	}

	types::unique_type typeof_array_type(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::ARRAY_TYPE);
		copy_parent_scope(n, ast);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto &elem_node = ast.get_node(children[0]);
		auto elem_type = typeof(elem_node, ast, type_constraints());

		auto &idx_node = ast.get_node(children[1]);
		types::unique_type is_num(new types::ui64);
		auto idx_type =
		  typeof(idx_node, ast, type_constraints({ equality_constraint(*is_num) }));
		auto num = ast.get_data<number>(idx_node.data_index).value;

		return types::unique_type(new types::array_type(std::move(elem_type), num));
	}

	types::unique_type typeof_sum_type(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::SUM_TYPE);
		copy_parent_scope(n, ast);
		auto &children = ast.children_of(n);
		assert(children.size() >= 2);

		std::vector<types::unique_type> types;
		for (int i = 0; i < children.size(); i += 2)
		{
			auto &id = ast.get_node(children[i]);
			auto &id_data = ast.get_data<identifier>(id.data_index);
			auto &child = ast.get_node(children[i + 1]);
			auto child_type = typeof(child, ast, type_constraints());
			types.push_back(types::unique_type(
			  new types::nominal_type(id_data.full, std::move(child_type))));
		}
		types::sum_type as_sum(std::move(types));

		// Sum values have an additional tag
		std::vector<types::unique_type> product;
		product.push_back(types::unique_type(new types::ui8()));
		product.push_back(types::unique_type(as_sum.copy()));
		auto with_tag_type = types::product_type(std::move(product));

		for (int i = 0; i < children.size(); i += 2)
		{
			auto &id_child = ast.get_node(children[i]);
			auto &scope = ast.get_type_scope(id_child.type_scope_id);
			auto &id_data = ast.get_data<identifier>(id_child.data_index);
			scope.define_type(id_data.name, types::unique_type(with_tag_type.copy()));

			auto function_type = types::unique_type(new types::function_type(
			  types::unique_type(
			    dynamic_cast<types::nominal_type *>(as_sum.sum.at(i / 2).get())
			      ->inner->copy()),
			  types::unique_type(with_tag_type.copy())));
			scope.set_type(id_data.name, std::move(function_type));
		}

		return types::unique_type(with_tag_type.copy());
	}

	void declare_assignable(node &assignable, ast &ast, const types::type &t)
	{
		copy_parent_scope(assignable, ast);
		if (assignable.kind == node_type::IDENTIFIER)
		{
			auto &data = ast.get_data<identifier>(assignable.data_index);
			// #todo with better constraint solving this will be cleaner
			ast.get_type_scope(assignable.type_scope_id)
			  .set_type(data.full, types::unique_type(t.copy()));
		}
		else if (assignable.kind == node_type::IDENTIFIER_TUPLE)
		{
			auto tuple_t = dynamic_cast<const types::product_type *>(&t);

			auto i = 0;
			auto &children = ast.children_of(assignable);
			for (auto &child : children)
			{
				copy_parent_scope(ast.get_node(child), ast);
				auto &data =
				  ast.get_data<identifier>(ast.get_node(child).data_index);
				// #todo with better constraint solving this will be cleaner
				ast.get_type_scope(assignable.type_scope_id)
				  .set_type(data.full,
					    types::unique_type(tuple_t->product[i]->copy()));
				i++;
			}
		}
	}

	types::unique_type typeof_lambda(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::LAMBDA);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);

		auto &parent = ast.get_node(n.parent_id);
		n.type_scope_id = ast.create_type_scope(parent.type_scope_id);

		auto &constraint = std::get<equality_constraint>(tc.constraints[0]);
		auto function_constraint =
		  dynamic_cast<const types::function_type *>(&constraint.to);

		auto &assignable_node = ast.get_node(children.at(0));
		assert(assignable_node.kind == node_type::IDENTIFIER ||
		       assignable_node.kind == node_type::IDENTIFIER_TUPLE);

		declare_assignable(assignable_node, ast, *function_constraint->from);

		// Resolve body
		auto to_type =
		  typeof(ast.get_node(children[1]), ast,
			 type_constraints({ equality_constraint(*(function_constraint->to)) }));
		auto func_type = types::unique_type(new types::function_type(
		  types::unique_type(function_constraint->from->copy()), std::move(to_type)));

		if (!tc.satisfied_by(*func_type))
			throw typecheck_error{ "function type " +
					       func_type->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };

		return func_type;
	}

	types::unique_type typeof_type_tuple(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::TUPLE_TYPE);
		copy_parent_scope(n, ast);
		std::vector<types::unique_type> types;
		auto &children = ast.children_of(n);
		for (auto &child : children)
		{
			types.push_back(typeof(ast.get_node(child), ast, tc));
		}
		return types::unique_type(new types::product_type(std::move(types)));
	}

	void typecheck_pattern(node &n, ast &ast, types::type &curr)
	{
		copy_parent_scope(n, ast);
		auto &scope = ast.get_type_scope(n.name_scope_id);

		switch (n.kind)
		{
		case node_type::FUNCTION_CALL:
		{
			auto &id = ast.get_node(ast.get_children(n.children_id)[0]);
			auto &name = ast.get_data<identifier>(id.data_index);
			auto res = scope.resolve_type(name, ast.type_scope_cb());
			auto &t = res->type;

			auto &sum_type = *dynamic_cast<types::sum_type *>(
			  dynamic_cast<types::product_type *>(&t)->product[1].get());
			assert(sum_type == curr);

			types::type *inner = nullptr;
			for (auto i = 0; i < sum_type.sum.size(); i++)
			{
				auto nominal_type =
				  dynamic_cast<types::nominal_type *>(sum_type.sum[i].get());
				if (name.full == nominal_type->name)
					inner = nominal_type->inner.get();
			}
			assert(inner != nullptr);

			auto &param = ast.get_node(ast.get_children(n.children_id)[1]);
			typecheck_pattern(param, ast, *inner);
			break;
		}
		case node_type::IDENTIFIER:
		{
			auto &name = ast.get_data<identifier>(n.data_index).name;
			scope.set_type(name, types::unique_type(curr.copy()));
			break;
		}
		case node_type::TUPLE:
		{
			auto as_product = dynamic_cast<types::product_type *>(&curr);
			auto &children = ast.get_children(n.children_id);
			for (auto i = 0; i < children.size(); i++)
				typecheck_pattern(ast.get_node(children[i]), ast,
						  *as_product->product[i]);
			break;
		}
		case node_type::NUMBER:
		{
			auto &num = ast.get_data<number>(n.data_index);
			typeof_number(n, ast, type_constraints({ equality_constraint(curr) }));
			break;
		}
		case node_type::BOOLEAN: break;
		default: assert(!"Invalid pattern"); break;
		}
	}

	types::unique_type typeof_match_branch(node &n, ast &ast, types::type &subj,
					       type_constraints tc)
	{
		assert(n.kind == node_type::MATCH_BRANCH);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		n.type_scope_id = ast.create_type_scope(ast.get_node(n.parent_id).type_scope_id);

		auto &test_node = ast.get_node(children[0]);
		typecheck_pattern(test_node, ast, subj);

		auto &code_node = ast.get_node(children[1]);
		auto code_type = typeof(code_node, ast);
		if (!tc.satisfied_by(*code_type))
			throw typecheck_error{ "match branch type " +
					       code_type->operator std::string() +
					       " does not match constraints\n" +
					       tc.operator std::string() };
		return code_type;
	}

	types::unique_type typeof_match(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::MATCH);
		copy_parent_scope(n, ast);
		auto &children = ast.children_of(n);

		if (children.size() <= 1)
			throw typecheck_error{ "Match must contain at least one branch" };

		auto &subject = ast.get_node(children[0]);
		types::unique_type subject_type = typeof(subject, ast);
		types::type &structure =
		  *dynamic_cast<types::product_type *>(subject_type.get())->product[1];

		auto &first_branch = ast.get_node(children[1]);
		types::unique_type first_type =
		  typeof_match_branch(first_branch, ast, structure, type_constraints());

		if (!tc.satisfied_by(*first_type))
			throw typecheck_error{ "Match expression type " +
					       first_type->operator std::string() +
					       " does not satisfy constraints " +
					       tc.operator std::string() };

		for (auto i = 2; i < children.size(); i++)
		{
			auto &branch_node = ast.get_node(children[i]);
			auto branch_type = typeof_match_branch(
			  branch_node, ast, structure,
			  type_constraints({ equality_constraint(*first_type) }));
		}
		return first_type;
	}

	types::unique_type typeof_if_expr(node &n, ast &ast, type_constraints tc)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		copy_parent_scope(n, ast);

		auto &children = ast.children_of(n);

		auto &first_branch_node = ast.get_node(children[0]);
		typeof(first_branch_node, ast,
		       type_constraints({ equality_constraint(types::boolean()) }));
		auto &first_then_node = ast.get_node(children[1]);
		auto first_type = typeof(first_then_node, ast, tc);

		for (auto i = 2; i < children.size() - 1; i += 2)
		{
			auto &branch_node = ast.get_node(children[i]);
			typeof(branch_node, ast,
			       type_constraints({ equality_constraint(types::boolean()) }));
			auto &then_node = ast.get_node(children[i + 1]);
			auto type = typeof(then_node, ast, tc);
		}

		// Else
		if (children.size() % 2 == 1)
		{
			auto &else_node = ast.get_node(children.back());
			auto type = typeof(else_node, ast, tc);
		}

		return first_type;
	}

	// Typecheck

	void typecheck_function_call(node &n, ast &ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &id_node = ast.get_node(children[0]);
		auto res = typeof(id_node, ast);
		auto function_type = dynamic_cast<types::function_type *>(res.get());

		auto &param_node = ast.get_node(children[1]);
		auto param_type = typeof(
		  param_node, ast, type_constraints({ equality_constraint(*function_type->from) }));
	}

	void typecheck_block(node &n, ast &ast)
	{
		assert(n.kind == node_type::BLOCK);
		if (n.parent_id)
		{
			auto &parent = ast.get_node(n.parent_id);
			n.type_scope_id = ast.create_type_scope(parent.type_scope_id);
		}
		else
			assert(n.type_scope_id != no_scope);

		auto &children = ast.children_of(n);
		for (auto element : children)
		{
			auto &elem_node = ast.get_node(element);
			typecheck(elem_node, ast);
		}

		if (children.size() > 0)
		{
			// #todo set this type to the last child type if it is an expression,
			// otherwise void
		}
	}

	void typecheck_type_definition(node &n, ast &ast)
	{
		assert(n.kind == node_type::TYPE_DEFINITION);
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &id_node = ast.get_node(children[0]);
		auto &id_data = ast.get_data<identifier>(id_node.data_index);
		assert(id_data.module_path.size() == 0);

		auto &type_node = ast.get_node(children[1]);
		auto rhs = typeof(type_node, ast);

		auto &scope = ast.get_type_scope(n.type_scope_id);
		scope.define_type(id_data.name, types::unique_type(rhs->copy()));

		auto function_type = types::unique_type(new types::function_type(
		  types::unique_type(rhs->copy()), types::unique_type(rhs->copy())));
		scope.set_type(id_data.name, std::move(function_type));
	}

	types::unique_type product_type_element(types::type &t, ast &ast, size_t offsets)
	{
		if (auto pt = dynamic_cast<types::product_type *>(&t))
		{
			return types::unique_type(pt->product.at(offsets)->copy());
		}

		assert(!"Cannot get element of this type, not a product");
	}

	void typecheck_declaration(node &n, ast &ast)
	{
		assert(n.kind == node_type::DECLARATION);
		// id type value
		auto &children = ast.children_of(n);
		assert(children.size() == 3);
		copy_parent_scope(n, ast);

		auto &type_node = ast.get_node(children[1]);
		auto type = typeof(type_node, ast);

		auto &scope = ast.get_type_scope(n.type_scope_id);
		auto &lhs_node = ast.get_node(children[0]);
		if (lhs_node.kind == node_type::IDENTIFIER)
		{
			auto &id_data = ast.get_data<identifier>(lhs_node.data_index);
			assert(id_data.module_path.size() == 0);
			scope.set_type(id_data.name, types::unique_type(type->copy()));
		}
		else if (lhs_node.kind == node_type::IDENTIFIER_TUPLE)
		{
			size_t i = 0;
			auto &lhs_children = ast.children_of(lhs_node);
			for (auto child : lhs_children)
			{
				auto &child_node = ast.get_node(child);
				auto &child_id = ast.get_data<identifier>(child_node.data_index);

				auto res = product_type_element(*type, ast, i);
				assert(res);
				scope.set_type(child_id.name, std::move(res));
				i++;
			}
		}
		else
		{
			assert(!"Illegal lhs of declaration");
		}

		auto &val_node = ast.get_node(children[2]);
		auto val_type =
		  typeof(val_node, ast, type_constraints({ equality_constraint(*type) }));
	}

	void typecheck_assignment(node &n, ast &ast)
	{
		assert(n.kind == node_type::ASSIGNMENT);
		auto &children = ast.children_of(n);
		// id value
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &scope = ast.get_type_scope(n.type_scope_id);

		auto &id_node = ast.get_node(children[0]);
		auto &id_data = ast.get_data<identifier>(id_node.data_index);
		auto res = scope.resolve_variable(id_data, ast.type_scope_cb());
		assert(res);
		auto id_type = std::unique_ptr<types::type>((*res).type.copy());

		auto &value_node = ast.get_node(children[1]);
		auto value_type =
		  typeof(value_node, ast, type_constraints({ equality_constraint(*id_type) }));

		assert(*id_type == &*value_type);
	}

	void typecheck_reference(node &n, ast &ast)
	{
		assert(n.kind == node_type::REFERENCE);
		auto &children = ast.children_of(n);
		assert(children.size() == 1);

		auto &child_node = ast.get_node(children[0]);
		typecheck(child_node, ast);
		// #todo set this type to reference type
	}

	void typecheck_while_loop(node &n, ast &ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		// test body
		auto &children = ast.children_of(n);
		assert(children.size() == 2);
		copy_parent_scope(n, ast);

		auto &test_node = ast.get_node(children[0]);
		auto type = typeof(test_node, ast);

		auto bool_type = types::boolean();
		assert(*type == &bool_type);

		auto &body_node = ast.get_node(children[1]);
		typecheck(body_node, ast);
	}

	void typecheck_if_statement(node &n, ast &ast)
	{
		assert(n.kind == node_type::IF_STATEMENT);
		// test body
		auto &children = ast.children_of(n);
		assert(children.size() >= 2);
		copy_parent_scope(n, ast);

		auto if_test_type = typeof(ast.get_node(children[0]), ast);
		auto if_block_type = typeof(ast.get_node(children[1]), ast);

		bool contains_else = children.size() % 2 == 1 ? true : false;
		size_t size_excluding_else = children.size() - (children.size() % 2);
		for (int i = 2; i < size_excluding_else; i += 2)
		{
			auto test_type = typeof(ast.get_node(children[i]), ast);
			auto block_type = typeof(ast.get_node(children[i + 1]), ast);
			auto bool_type = types::boolean();
			assert(*test_type == &bool_type);
			assert(*block_type == &*if_block_type);
		}

		if (contains_else)
		{
			auto else_block_type = typeof(ast.get_node(children.back()), ast);
			assert(*else_block_type == &*if_block_type);
		}
	}

	void typecheck_function(node &n, ast &ast)
	{
		assert(n.kind == node_type::FUNCTION);
		auto &children = ast.children_of(n);
		assert(children.size() == 3);
		copy_parent_scope(n, ast);

		auto fun_type = typeof(ast[children[1]], ast);
		type_constraints ts({ equality_constraint(*fun_type) });
		typeof_lambda(ast[children[2]], ast, ts);
	}

	void typecheck(node &n, ast &ast)
	{
		switch (n.kind)
		{
		case node_type::FUNCTION_CALL: typecheck_function_call(n, ast); break;
		case node_type::BLOCK: typecheck_block(n, ast); break;
		case node_type::TYPE_DEFINITION: typecheck_type_definition(n, ast); break;
		case node_type::DECLARATION: typecheck_declaration(n, ast); break;
		case node_type::ASSIGNMENT: typecheck_assignment(n, ast); break;
		case node_type::REFERENCE: typecheck_reference(n, ast); break;
		case node_type::WHILE_LOOP: typecheck_while_loop(n, ast); break;
		case node_type::FUNCTION: typecheck_function(n, ast); break;
		case node_type::IF_STATEMENT:
			typecheck_if_statement(n, ast);
			break;
			// expression as statement
		case node_type::MATCH:
		case node_type::MATCH_BRANCH: typeof(n, ast); break;
		case node_type::MODULE_DECLARATION:
		case node_type::IMPORT_DECLARATION: break;
		default: assert(!"This node cannot be typechecked");
		}
	}

	types::unique_type typeof(node &n, ast &ast, type_constraints tc)
	{
		switch (n.kind)
		{
		case node_type::IDENTIFIER: return typeof_identifier(n, ast, tc);
		case node_type::NUMBER: return typeof_number(n, ast, tc);
		case node_type::BOOLEAN: return typeof_boolean(n, ast, tc);
		case node_type::STRING: return typeof_string(n, ast, tc);
		case node_type::ARRAY_VALUE: return typeof_array_value(n, ast, tc);
		case node_type::TUPLE: return typeof_tuple(n, ast, tc);
		case node_type::FUNCTION_CALL: return typeof_function_call(n, ast, tc);
		case node_type::ARRAY_ACCESS: return typeof_array_access(n, ast, tc);
		case node_type::BLOCK: return typeof_block(n, ast, tc);
		case node_type::BLOCK_RESULT: return typeof_block_result(n, ast, tc);
		case node_type::ATOM_TYPE: return typeof_type_atom(n, ast, tc);
		case node_type::FUNCTION_TYPE: return typeof_function_type(n, ast, tc);
		case node_type::ARRAY_TYPE: return typeof_array_type(n, ast, tc);
		case node_type::SUM_TYPE: return typeof_sum_type(n, ast, tc);
		case node_type::LAMBDA: return typeof_lambda(n, ast, tc);
		case node_type::TUPLE_TYPE: return typeof_type_tuple(n, ast, tc);
		case node_type::MATCH: return typeof_match(n, ast, tc);
		case node_type::IF_STATEMENT: return typeof_if_expr(n, ast, tc);
		default:
			if (ext_ast::is_binary_op(n.kind)) return typeof_binary_op(n, ast, tc);
			if (ext_ast::is_unary_op(n.kind)) return typeof_unary_op(n, ast, tc);

			assert(!"This node cannot be typeofed");
			throw std::runtime_error("Internal Error: Node cannot be typeofed");
		}
	}

	void typecheck(ast &ast, const interfaces &ifaces)
	{
		auto root = ast.root_id();
		typecheck(ast.get_node(root), ast);
	}
} // namespace fe::ext_ast