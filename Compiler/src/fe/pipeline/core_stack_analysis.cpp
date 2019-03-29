#include "fe/pipeline/core_stack_analysis.h"

namespace fe::core_ast
{
	constexpr uint8_t BOOL_SIZE = 1;

	node_id predecessor(node_id n, ast &ast)
	{
		auto &node = ast.get_node(n);
		auto &parent = ast.parent_of(n);

		if (auto it = std::find(parent.children.begin(), parent.children.end(), node.id);
		    it != parent.children.begin())
			return *(--it);
		else
			return predecessor(parent.id, ast);
	}

	size_t predecessor_size(node_id n, ast &ast, stack_analysis_result &res)
	{
		auto &node = ast.get_node(n);
		auto &parent = ast.parent_of(n);

		if (parent.kind == node_type::FUNCTION)
		{
			// return
			// ast.get_node_data<function_data>(parent.id).in_size + 8;
			return res.node_stack_sizes.at(parent.id);
		}

		if (auto it = std::find(parent.children.begin(), parent.children.end(), node.id);
		    it != parent.children.begin())
		{ return res.node_stack_sizes.at(*(--it)); } else
			return predecessor_size(parent.id, ast, res);
	}

	void analyze_stack(node_id n, ast &ast, stack_analysis_result &res)
	{
		auto &node = ast.get_node(n);

		switch (node.kind)
		{
		case node_type::NOP: break;
		case node_type::NUMBER:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] = predecessor_size(n, ast, res) +
						  number_size(ast.get_node_data<number>(n).type);
			break;
		}
		case node_type::STRING:
		{
			assert(!"nyi");
			break;
		}
		case node_type::BOOLEAN:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] = predecessor_size(n, ast, res) + BOOL_SIZE;
			break;
		}
		case node_type::FUNCTION:
		{
			res.node_stack_sizes[n] = predecessor_size(n, ast, res);
			break;
		}
		case node_type::BLOCK:
		case node_type::TUPLE:
		{
			auto before_size = predecessor_size(n, ast, res);
			res.pre_node_stack_sizes[n] = before_size;
			auto &node = ast.get_node(n);
			for (auto &child : node.children) analyze_stack(child, ast, res);

			if (node.children.size() == 0)
				res.node_stack_sizes[n] = before_size;
			else
				res.node_stack_sizes[n] =
				  res.node_stack_sizes[node.children.back()];
			break;
		}
		case node_type::FUNCTION_CALL:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] =
			  predecessor_size(n, ast, res) + *ast.get_node(n).size;
			for (auto &child : node.children) analyze_stack(child, ast, res);
			break;
		}
		case node_type::REFERENCE:
		{
			assert(!"nyi");
			break;
		}
		case node_type::RET:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] =
			  predecessor_size(n, ast, res) - ast.get_node_data<return_data>(n).in_size;
			analyze_stack(ast.get_node(n).children[0], ast, res);
			break;
		}
		case node_type::PUSH:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			auto first_child = ast.get_node(ast.get_node(n).children[0]);
			// Dynamic access pops an 8 byte value off the stack for
			// indexing
			if (first_child.kind == node_type::DYNAMIC_PARAM ||
			    first_child.kind == node_type::DYNAMIC_VARIABLE)
				res.node_stack_sizes[n] = predecessor_size(n, ast, res) - 8 +
							  ast.get_node_data<size>(n).val;
			else
				res.node_stack_sizes[n] =
				  predecessor_size(n, ast, res) + ast.get_node_data<size>(n).val;
			break;
		}
		case node_type::STACK_ALLOC:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] =
			  predecessor_size(n, ast, res) + ast.get_node_data<size>(n).val;
			break;
		}
		case node_type::STACK_DEALLOC:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] =
			  predecessor_size(n, ast, res) - ast.get_node_data<size>(n).val;
			break;
		}
		case node_type::JMP:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] = predecessor_size(n, ast, res);
			break;
		}
		case node_type::JZ:
		case node_type::JNZ:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] = predecessor_size(n, ast, res) - BOOL_SIZE;
			break;
		}
		case node_type::LABEL:
		{
			auto before = ast.get_node(predecessor(n, ast));
			auto kind = before.kind;
			if (kind != node_type::JMP && kind != node_type::JZ &&
			    kind != node_type::JNZ)
				res.node_stack_sizes[n] = predecessor_size(n, ast, res);

			auto lbl = ast.get_node_data<label>(n);
			ast_helper ah(ast);
			ah.for_all([&res, &ast, &lbl, n](auto &jmp) {
				auto kind = jmp.kind;
				if (kind != node_type::JMP && kind != node_type::JZ &&
				    kind != node_type::JNZ)
					return;

				if (ast.get_node_data<label>(jmp.id).id == lbl.id)
				{
					auto jmp_it = res.node_stack_sizes.find(jmp.id);
					auto n_it = res.node_stack_sizes.find(n);
					auto end = res.node_stack_sizes.end();
					// Jmp has size, label does not
					if (jmp_it != end && n_it == end)
						res.node_stack_sizes[n] = jmp_it->second;
					// Both no size
					if (jmp_it == end && n_it == end) { assert(!"nyi"); }
					// Label has size, jmp does not
					if (jmp_it == end && n_it != end)
						res.node_stack_sizes[jmp.id] = n_it->second;
					// Both have size, check equals
					if (jmp_it != end && n_it != end)
					{ assert(jmp_it->second == n_it->second); } }
			});

			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			break;
		}
		case node_type::POP:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] =
			  predecessor_size(n, ast, res) - ast.get_node_data<size>(n).val;
			break;
		}
		case node_type::STACK_LABEL:
		{
			res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
			res.node_stack_sizes[n] = predecessor_size(n, ast, res);
			break;
		}
		default:
		{
			if (is_binary_op(node.kind))
			{
				res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);

				analyze_stack(ast.get_node(n).children[0], ast, res);
				analyze_stack(ast.get_node(n).children[1], ast, res);
				switch (node.kind)
				{
				case core_ast::node_type::ADD:
				case core_ast::node_type::SUB:
				case core_ast::node_type::MUL:
				case core_ast::node_type::DIV:
				case core_ast::node_type::MOD:
				case core_ast::node_type::AND:
				case core_ast::node_type::OR:
					res.node_stack_sizes[n] =
					  res.node_stack_sizes[ast.get_node(n).children[0]];
					break;
				case core_ast::node_type::LT:
				case core_ast::node_type::LEQ:
				case core_ast::node_type::GT:
				case core_ast::node_type::GEQ:
				case core_ast::node_type::EQ:
				case core_ast::node_type::NEQ:
					res.node_stack_sizes[n] =
					  predecessor_size(n, ast, res) + BOOL_SIZE;
					break;
				}
			}
			else if (is_unary_op(node.kind))
			{
				res.pre_node_stack_sizes[n] = predecessor_size(n, ast, res);
				analyze_stack(ast.get_node(n).children[0], ast, res);
				assert(node.kind == core_ast::node_type::NOT);
				res.node_stack_sizes[n] = predecessor_size(n, ast, res) + BOOL_SIZE;
			}
			else
			{
				assert(!"Cannot calculate stack size");
			}
		}
		}
	}

	stack_analysis_result analyze_stack(node_id id, ast &ast)
	{
		stack_analysis_result r;

		auto &node = ast.get_node(id);
		if (node.kind == core_ast::node_type::FUNCTION)
		{
			r.node_stack_sizes[id] = ast.get_node_data<function_data>(id).in_size +
						 ast.get_node_data<function_data>(id).locals_size;
			analyze_stack(ast.get_node(id).children[0], ast, r);
		}
		else
		{
			assert(!"Stack analysis for this node is not possible");
		}

		return r;
	}
} // namespace fe::core_ast