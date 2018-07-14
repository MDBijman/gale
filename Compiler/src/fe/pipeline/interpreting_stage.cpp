#include "fe/pipeline/interpreting_stage.h"

#include <variant>

#include "fe/data/core_ast.h"
#include "fe/data/values.h"

namespace fe::core_ast
{
	static void copy_parent_scope(node& n, ast& ast)
	{
		assert(n.parent_id);
		auto& parent = ast.get_node(*n.parent_id);
		assert(parent.value_scope_id);
		n.value_scope_id = *parent.value_scope_id;
	}

	values::unique_value interpret_nop(node& n, ast& ast)
	{
		return values::unique_value(new values::void_value());
	}

	values::unique_value interpret_number(node& n, ast& ast)
	{
		assert(n.kind == node_type::NUMBER);
		assert(n.data_index);
		auto& num = ast.get_data<number>(*n.data_index);
		switch (num.type)
		{
			case number_type::I32:
				return values::unique_value(new values::i32(static_cast<int32_t>(num.value)));
				break;
			case number_type::I64:
				return values::unique_value(new values::i64(static_cast<int64_t>(num.value)));
				break;
			case number_type::UI32:
				return values::unique_value(new values::ui32(static_cast<uint32_t>(num.value)));
				break;
			case number_type::UI64:
				return values::unique_value(new values::ui64(static_cast<uint64_t>(num.value)));
				break;
			default:
				throw std::runtime_error("Error: unknown number type");
		}
	}

	values::unique_value interpret_string(node& n, ast& ast)
	{
		assert(n.kind == node_type::STRING);
		assert(n.data_index);
		auto& str = ast.get_data<string>(*n.data_index);
		return values::unique_value(new values::str(str.value));
	}

	values::unique_value interpret_boolean(node& n, ast& ast)
	{
		assert(n.kind == node_type::BOOLEAN);
		assert(n.data_index);
		auto& data = ast.get_data<boolean>(*n.data_index);
		return values::unique_value(new values::boolean(data.value));
	}

	values::unique_value interpret_identifier(node& n, ast& ast)
	{
		assert(n.kind == node_type::IDENTIFIER);
		assert(n.data_index);
		copy_parent_scope(n, ast);
		auto& data = ast.get_data<identifier>(*n.data_index);
		auto val = ast.get_runtime_context().get_value(*n.value_scope_id, data);
		assert(val);

		return values::unique_value((*val)->copy());
	}

	void set_lhs(node& lhs, values::unique_value v, ast& ast)
	{
		copy_parent_scope(lhs, ast);

		if (lhs.kind == node_type::IDENTIFIER)
		{
			assert(lhs.data_index);
			auto& data = ast.get_data<identifier>(*lhs.data_index);
			ast.get_runtime_context().set_value(*lhs.value_scope_id, data, std::move(v));
		}
		else if (lhs.kind == node_type::IDENTIFIER_TUPLE)
		{
			if (auto pv = dynamic_cast<values::tuple*>(v.get()))
			{
				assert(lhs.children.size() == pv->val.size());
				for(int i = 0; i < lhs.children.size(); i++)
				{
					auto& child = ast.get_node(lhs.children[i]);
					auto& value = pv->val[i];
					set_lhs(child, std::move(value), ast);
				}
			}
			else
			{
				throw std::runtime_error("Error: cannot assign non-tuple to identifier tuple");
			}
		}
	}

	values::unique_value interpret_set(node& n, ast& ast)
	{
		assert(n.kind == node_type::SET);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& rhs = ast.get_node(n.children[1]);
		auto& val = interpret(rhs, ast);

		auto& lhs = ast.get_node(n.children[0]);
		set_lhs(lhs, std::move(val), ast);

		return values::unique_value(new values::void_value());
	}

	values::unique_value interpret_function(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION);
		assert(n.children.size() == 2);
		auto parent_scope = ast.get_node(*n.parent_id).value_scope_id;
		assert(parent_scope);
		n.value_scope_id = ast.create_value_scope(*parent_scope);

		return values::unique_value(new values::function(n.id));
	}

	values::unique_value interpret_tuple(node& n, ast& ast)
	{
		assert(n.kind == node_type::TUPLE);
		copy_parent_scope(n, ast);

		auto val = new values::tuple();
		for (auto child_id : n.children)
		{
			auto& child = ast.get_node(child_id);
			val->val.push_back(std::move(interpret(child, ast)));
		}
		return values::unique_value(std::move(val));
	}

	values::unique_value interpret_block(node& n, ast& ast)
	{
		assert(n.kind == node_type::BLOCK);
		if (n.parent_id)
		{
			if (n.value_scope_id)
				ast.get_runtime_context().get_scope(*n.value_scope_id).clear();
			else
			{
				auto& parent = ast.get_node(*n.parent_id);
				assert(parent.value_scope_id);
				n.value_scope_id = ast.create_value_scope(*parent.value_scope_id);
			}
		}
		else
			assert(n.value_scope_id);

		values::unique_value last_val;
		for (auto child_id : n.children)
		{
			auto& child = ast.get_node(child_id);
			last_val = interpret(child, ast);
		}

		return std::move(last_val);
	}

	values::unique_value interpret_function_call(node& n, ast& ast)
	{
		assert(n.kind == node_type::FUNCTION_CALL);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		// Interpret parameter
		auto& val_node = ast.get_node(n.children[1]);
		values::unique_value arg = interpret(val_node, ast);

		// Lookup function name
		auto& id_node = ast.get_node(n.children[0]);
		assert(id_node.data_index);
		auto& id_data = ast.get_data<identifier>(*id_node.data_index);
		std::optional<values::value*> id_val = ast
			.get_runtime_context()
			.get_value(*n.value_scope_id, id_data);
		assert(id_val);

		// Call function
		if (values::function* func = dynamic_cast<values::function*>(*id_val))
		{
			auto& func_node = ast.get_node(func->func);

			// Copy the scope of the function and restore it after the call
			value_scope& func_scope = ast.get_runtime_context().get_scope(*func_node.value_scope_id);
			value_scope scope_copy = func_scope;
			func_scope.clear();

			auto& params_node = ast.get_node(func_node.children[0]);
			set_lhs(params_node, std::move(arg), ast);

			auto res = interpret(ast.get_node(func_node.children[1]), ast);

			ast.get_runtime_context().get_scope(*func_node.value_scope_id) = std::move(scope_copy);
			return res;
		}
		else if (values::native_function* func = dynamic_cast<values::native_function*>(*id_val))
		{
			return func->function(std::move(arg));
		}
		else
		{
			throw interp_error{ "Error: cannot apply non function value" };
		}
	}

	values::unique_value interpret_branch(node& n, ast& ast)
	{
		assert(n.kind == node_type::BRANCH);
		assert(n.children.size() >= 2);

		copy_parent_scope(n, ast);

		for (int i = 0; i < n.children.size(); i += 2)
		{
			auto& test_node = ast.get_node(n.children[i]);

			auto test_val = interpret(test_node, ast);
			values::boolean* b = dynamic_cast<values::boolean*>(test_val.get());
			if (!b) 
				throw std::runtime_error("Error: if test must be of boolean value");

			if (b->val)
			{
				auto& body_node = ast.get_node(n.children[i + 1]);
				return interpret(body_node, ast);
			}
		}

		return values::unique_value(new values::void_value());
	}

	values::unique_value interpret_reference(node& n, ast& ast)
	{
		throw std::runtime_error("Error: unimplemented");
	}

	values::unique_value interpret_while_loop(node& n, ast& ast)
	{
		assert(n.kind == node_type::WHILE_LOOP);
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto& test_node = ast.get_node(n.children[0]);
		auto& body_node = ast.get_node(n.children[1]);

		values::unique_value test_val = interpret(test_node, ast);
		assert(dynamic_cast<values::boolean*>(test_val.get()));

		while (static_cast<values::boolean*>(test_val.get())->val)
		{
			interpret(body_node, ast);
			test_val = interpret(test_node, ast);
			assert(dynamic_cast<values::boolean*>(test_val.get()));
		}

		return values::unique_value(new values::void_value());
	}

	template<typename T>
	values::unique_value op(node_type n, T t1, T t2)
	{
		switch (n)
		{
		case node_type::ADD: return values::unique_value(new T(t1 + t2)); break;
		case node_type::SUB: return values::unique_value(new T(t1 - t2)); break;
		case node_type::MUL: return values::unique_value(new T(t1 * t2)); break;
		case node_type::DIV: return values::unique_value(new T(t1 / t2)); break;
		case node_type::MOD:
			if constexpr(std::is_same<T, values::i32>::value || std::is_same<T, values::i64>::value
				|| std::is_same<T, values::ui32>::value || std::is_same<T, values::ui64>::value)
				return values::unique_value(new T(t1 % t2));
			else
				throw interp_error{ "Error: mod on invalid values" }
			break;
		case node_type::EQ:  return values::unique_value(new values::boolean(t1 == t2)); break;
		case node_type::GEQ: return values::unique_value(new values::boolean(t1 >= t2)); break;
		case node_type::GT:  return values::unique_value(new values::boolean(t1 > t2));  break;
		case node_type::LEQ: return values::unique_value(new values::boolean(t1 <= t2)); break;
		case node_type::LT:  return values::unique_value(new values::boolean(t1 < t2));  break;
		case node_type::AND: return values::unique_value(new values::boolean(t1 && t2)); break;
		case node_type::OR:  return values::unique_value(new values::boolean(t1 || t2)); break;
		default: throw lower_error{ "Unknown binary op" };
		}
	}

	values::unique_value interpret_bin_op(node& n, ast& ast)
	{
		assert(is_binary_op(n.kind));
		assert(n.children.size() == 2);
		copy_parent_scope(n, ast);

		auto lhs_val = interpret(ast.get_node(n.children[0]), ast);
		auto rhs_val = interpret(ast.get_node(n.children[1]), ast);

		if (auto x = dynamic_cast<values::i64*>(lhs_val.get()))
		{
			if (auto y = dynamic_cast<values::i64*>(rhs_val.get()))
				return op<values::i64>(n.kind, *x, *y);
			else if (auto y = dynamic_cast<values::i32*>(rhs_val.get()))
				return op<values::i64>(n.kind, *x, values::i64(y->val));
		}
		else if (auto x = dynamic_cast<values::i32*>(lhs_val.get()))
		{
			if (auto y = dynamic_cast<values::i32*>(rhs_val.get()))
				return op<values::i32>(n.kind, *x, *y);
			else if (auto y = dynamic_cast<values::i64*>(rhs_val.get()))
				return op<values::i64>(n.kind, values::i64(x->val), *y);
		}
		else if (auto x = dynamic_cast<values::ui64*>(lhs_val.get()))
		{
			if (auto y = dynamic_cast<values::ui64*>(rhs_val.get()))
				return op<values::ui64>(n.kind, *x, *y);
			else if (auto y = dynamic_cast<values::ui32*>(rhs_val.get()))
				return op<values::ui64>(n.kind, *x, values::ui64(y->val));
		}
		else if (auto x = dynamic_cast<values::ui32*>(lhs_val.get()))
		{
			if (auto y = dynamic_cast<values::ui32*>(rhs_val.get()))
				return op<values::ui32>(n.kind, *x, *y);
			else if (auto y = dynamic_cast<values::ui64*>(rhs_val.get()))
				return op<values::ui64>(n.kind, values::ui64(x->val), *y);
		}
		else if (auto x = dynamic_cast<values::f32*>(lhs_val.get()))
		{
			if (auto y = dynamic_cast<values::f32*>(rhs_val.get()))
				return op<values::f32>(n.kind, *x, *y);
			else if (auto y = dynamic_cast<values::f64*>(rhs_val.get()))
				return op<values::f64>(n.kind, values::f64(x->val), *y);
		}
		else if (auto x = dynamic_cast<values::f64*>(lhs_val.get()))
		{
			if (auto y = dynamic_cast<values::f64*>(rhs_val.get()))
				return op<values::f64>(n.kind, *x, *y);
			else if (auto y = dynamic_cast<values::f32*>(rhs_val.get()))
				return op<values::f64>(n.kind, *x, values::f64(y->val));
		}
		else throw interp_error{ "Invalid op" };

	}

	values::unique_value interpret(node& n, ast& ast)
	{
		switch (n.kind)
		{
		case node_type::NOP:           return interpret_nop(n, ast);
		case node_type::NUMBER:        return interpret_number(n, ast);
		case node_type::STRING:        return interpret_string(n, ast);
		case node_type::BOOLEAN:       return interpret_boolean(n, ast);
		case node_type::IDENTIFIER:    return interpret_identifier(n, ast);
		case node_type::SET:           return interpret_set(n, ast);
		case node_type::FUNCTION:      return interpret_function(n, ast);
		case node_type::TUPLE:         return interpret_tuple(n, ast);
		case node_type::BLOCK:         return interpret_block(n, ast);
		case node_type::FUNCTION_CALL: return interpret_function_call(n, ast);
		case node_type::BRANCH:        return interpret_branch(n, ast);
		case node_type::REFERENCE:     return interpret_reference(n, ast);
		case node_type::WHILE_LOOP:    return interpret_while_loop(n, ast);
		default:
			if (is_binary_op(n.kind)) return interpret_bin_op(n, ast);
			throw std::runtime_error("Error: unknown node type");
		}
	}

	std::variant<values::unique_value, interp_error> interpret(ast& ast)
	{
		auto& root_node = ast.get_node(ast.root_id());

		return interpret(root_node, ast);
	}
}