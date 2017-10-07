#pragma once
#include <memory>

#include "core_ast.h"
#include "extended_ast.h"
#include "error.h"
#include "typecheck_environment.h"
#include "tags.h"

namespace fe
{
	class typechecker_stage : public language::typechecking_stage<extended_ast::unique_node, extended_ast::unique_node, typecheck_environment, typecheck_error>
	{
	private:
		typecheck_environment base_environment;
	
	public:
		typechecker_stage() {}
		typechecker_stage(typecheck_environment environment) : base_environment(environment) {}

		std::variant<std::tuple<extended_ast::unique_node, typecheck_environment>, typecheck_error> typecheck(extended_ast::unique_node node, typecheck_environment env) override
		{
			try
			{
				node->typecheck(env);
			}
			catch (typecheck_error e)
			{
				return e;
			}

			return std::make_tuple(std::move(node), env);

			//auto visitor = [&](auto& n) {
			//	return typecheck(std::move(n), std::move(env));
			//};

			////// Pre- and post-order traversal of nodes
			////std::function<std::pair<extended_ast::node, typecheck_environment>(extended_ast::node, typecheck_environment)> traverse = [&](auto node, auto env) {
			////	auto pre_visitor = [&](auto& n) -> std::pair<extended_ast::node, typecheck_environment> {
			////		return pre_typecheck(std::move(n), std::move(env));
			////	};
			////	auto[node, env] = std::visit(pre_visitor, node);

			////	std::vector<extended_ast::node> children = std::visit(extended_ast::get_children, node);

			////	for (decltype(auto) child : children)
			////	{
			////		auto[new_c, new_e] = traverse(std::move(child), std::move(env));
			////		child = std::move(new_c);
			////		env = std::move(new_e);
			////	}

			////	auto post_visitor = [&](auto& n) -> std::pair<extended_ast::node, typecheck_environment> {
			////		return post_typecheck(std::move(n), std::move(env));
			////	};
			////	auto[node, env] = std::visit(post_visitor, node);

			////	return std::make_pair(std::move(node), std::move(env));
			////};


			//return std::visit(visitor, node);
		}

		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::tuple&& tuple, typecheck_environment&& env)
		//{
		//	auto new_type = types::product_type();

		//	for (decltype(auto) element : tuple.children)
		//	{
		//		auto res = typecheck(std::move(element), std::move(env));

		//		if (std::holds_alternative<typecheck_error>(res))
		//			return std::get<typecheck_error>(res);

		//		auto&[new_node, new_env] = std::get<std::tuple<extended_ast::node, typecheck_environment>>(res);

		//		element = std::move(new_node);
		//		env = std::move(new_env);

		//		new_type.product.push_back({ "", std::visit(extended_ast::get_type, element) });

		//	}

		//	tuple.type = std::move(new_type);

		//	return std::make_tuple(
		//		std::move(tuple),
		//		std::move(env)
		//	);
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::block&& block, typecheck_environment&& env)
		//{
		//	types::type final_type;
		//	for (decltype(auto) element : block.children)
		//	{
		//		auto checked_element_or_error = typecheck(std::move(element), std::move(env));
		//		if (std::holds_alternative<typecheck_error>(checked_element_or_error))
		//			return std::get<typecheck_error>(checked_element_or_error);

		//		auto& checked_element = std::get<std::tuple<extended_ast::node, typecheck_environment>>(checked_element_or_error);

		//		element = std::move(std::get<extended_ast::node>(checked_element));
		//		env = std::move(std::get<typecheck_environment>(checked_element));

		//		final_type = std::visit(extended_ast::get_type, element);
		//	}

		//	block.type = std::move(final_type);

		//	return std::make_tuple(
		//		std::move(block),
		//		std::move(env)
		//	);
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::module_declaration&& declaration, typecheck_environment&& env)
		//{
		//	std::string module_name = declaration.name.segments.at(0);
		//	env.name = std::move(module_name);
		//	return std::make_tuple(std::move(declaration), std::move(env));
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::identifier&& id, typecheck_environment&& env)
		//{
		//	id.type = std::get<std::reference_wrapper<const types::type>>(env.typeof(id)).get();

		//	id = env.build_access_pattern(std::move(id));

		//	return std::make_tuple(
		//		std::move(id),
		//		std::move(env)
		//	);
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::assignment&& assignment, typecheck_environment&& env)
		//{
		//	// Put id type in env
		//	auto type = std::visit(extended_ast::get_type, *assignment.value);
		//	env.set_type(assignment.id, type);
		//	assignment.id.type = std::move(type);

		//	assignment.type = types::atom_type("void");

		//	return std::make_tuple(
		//		std::move(assignment),
		//		std::move(env)
		//	);
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::function_call&& call, typecheck_environment&& env)
		//{
		//	auto res = typecheck(
		//		std::move(*call.params),
		//		std::move(env)
		//	);

		//	if (std::holds_alternative<typecheck_error>(res))
		//		return std::get<typecheck_error>(res);
		//	auto& checked_params = std::get<std::tuple<extended_ast::node, typecheck_environment>>(res);

		//	env = std::move(std::get<typecheck_environment>(checked_params));
		//	call.params = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_params)));

		//	auto& argument_type = std::visit(extended_ast::get_type, *call.params);

		//	auto function_or_type_or_error = env.typeof(call.id);
		//	const types::type& function_or_type = std::get<std::reference_wrapper<const types::type>>(function_or_type_or_error).get();
		//	if (std::holds_alternative<types::function_type>(function_or_type))
		//	{
		//		auto function_type = std::get<types::function_type>(function_or_type);

		//		// Check function signature against call signature
		//		if (!(argument_type == *function_type.from))
		//		{
		//			return typecheck_error{
		//				"Function call signature does not match function signature:\n"
		//				+ std::visit(types::to_string, argument_type) + "\n"
		//				+ std::visit(types::to_string, *function_type.from)
		//			};
		//		}

		//		call.type = std::move(*function_type.to);
		//	}
		//	else if (std::holds_alternative<types::product_type>(function_or_type))
		//	{
		//		const types::product_type& product_type = std::get<types::product_type>(function_or_type);

		//		// Check type signature against call signature
		//		if (!(argument_type == types::type(product_type)))
		//		{
		//			return typecheck_error{
		//				"Function call signature does not match function signature:\n"
		//				+ std::visit(types::to_string, argument_type) + "\n"
		//				+ std::visit(types::to_string, types::type(product_type))
		//			};
		//		}

		//		call.type = product_type;
		//	}
		//	else
		//	{
		//		return typecheck_error{
		//			"Function call can only call constructor or function"
		//		};
		//	}

		//	return std::make_tuple(
		//		std::move(call),
		//		std::move(env)
		//	);
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::export_stmt&& export_stmt, typecheck_environment&& env)
		//{
		//	export_stmt.type = types::atom_type("void");
		//	return std::make_tuple(std::move(export_stmt), std::move(env));
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::type_declaration&& type_declaration, typecheck_environment&& env)
		//{
		//	type_declaration.type = types::atom_type("void");

		//	auto type = interpret(type_declaration.types, env);

		//	if (std::holds_alternative<types::product_type>(type))
		//	{
		//		typecheck_environment type_env;
		//		for (decltype(auto) t : std::get<types::product_type>(type).product)
		//		{
		//			type_env.set_type(t.first, t.second);
		//		}
		//		env.add_module(std::move(type_env));
		//	}

		//	type_declaration.types.type = std::move(type);

		//	env.set_type(type_declaration.id.segments.at(0), type_declaration.types.type);


		//	return std::make_tuple(std::move(type_declaration), std::move(env));
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::integer&& integer, typecheck_environment&& env)
		//{
		//	integer.type = types::atom_type("i32");
		//	return std::make_tuple(std::move(integer), std::move(env));
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::string&& string, typecheck_environment&& env)
		//{
		//	string.type = types::atom_type("str");
		//	return std::make_tuple(std::move(string), std::move(env));
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::function&& func, typecheck_environment&& env)
		//{
		//	// Check 'from' type expression
		//	types::type from_type = interpret(func.from, env);

		//	// Check 'to' type expression
		//	types::type to_type = interpret_type(*func.to, env);

		//	func.type = types::function_type(types::make_unique(from_type), types::make_unique(to_type));

		//	// Allow recursion
		//	if (func.name.has_value())
		//	{
		//		env.set_type(func.name.value(), func.type);
		//	}

		//	auto res = typecheck(std::move(*func.body), std::move(env));

		//	if (std::holds_alternative<typecheck_error>(res))
		//		return std::get<typecheck_error>(res);

		//	auto&[new_body, new_env] = std::get<std::tuple<extended_ast::node, typecheck_environment>>(res);
		//	func.body = extended_ast::make_unique(std::move(new_body));
		//	env = std::move(new_env);

		//	if (!(std::visit(extended_ast::get_type, *func.body) == to_type))
		//	{
		//		return typecheck_error{ "Given return type is not the same as the type of the body" };
		//	}

		//	return std::make_tuple(
		//		std::move(func),
		//		std::move(env)
		//	);
		//}
		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::conditional_branch&& branch, typecheck_environment&& env)
		//{
		//	types::type common_type = types::unset_type();

		//	for (uint32_t branch_count = 0; branch_count < branch.branches.size(); branch_count++)
		//	{
		//		auto typechecked_path_or_error = typecheck(std::move(branch.branches.at(branch_count)), std::move(env));
		//		if (std::holds_alternative<typecheck_error>(typechecked_path_or_error))
		//			return std::get<typecheck_error>(typechecked_path_or_error);
		//		auto typechecked_path = std::move(std::get<std::tuple<extended_ast::node, typecheck_environment>>(typechecked_path_or_error));
		//		env = std::move(std::get<typecheck_environment>(typechecked_path));
		//		branch.branches.at(branch_count) = std::move(std::get<extended_ast::conditional_branch_path>(std::get<extended_ast::node>(typechecked_path)));

		//		if (common_type == types::type(types::unset_type()))
		//			common_type = branch.branches.at(branch_count).type;

		//		if (!(common_type == branch.branches.at(branch_count).type))
		//			return typecheck_error{ std::string("Branch is of a different type than those before it") };
		//	}

		//	branch.type = common_type;

		//	return std::make_tuple(
		//		std::move(branch),
		//		std::move(env)
		//	);
		//}

		//std::variant<std::tuple<extended_ast::node, typecheck_environment>, typecheck_error> typecheck(extended_ast::conditional_branch_path&& branch_path, typecheck_environment&& env)
		//{
		//	// Typecheck the test path
		//	auto checked_test_path_or_error = typecheck(std::move(*branch_path.test_path), std::move(env));
		//	if (std::holds_alternative<typecheck_error>(checked_test_path_or_error))
		//		return std::get<typecheck_error>(checked_test_path_or_error);
		//	auto checked_test_path = std::move(std::get<std::tuple<extended_ast::node, typecheck_environment>>(checked_test_path_or_error));
		//	branch_path.test_path = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_test_path)));
		//	env = std::move(std::get<typecheck_environment>(checked_test_path));

		//	// Typecheck the code path
		//	auto checked_code_path_or_error = typecheck(std::move(*branch_path.code_path), std::move(env));
		//	if (std::holds_alternative<typecheck_error>(checked_code_path_or_error))
		//		return std::get<typecheck_error>(checked_code_path_or_error);
		//	auto checked_code_path = std::move(std::get<std::tuple<extended_ast::node, typecheck_environment>>(checked_code_path_or_error));
		//	branch_path.code_path = extended_ast::make_unique(std::move(std::get<extended_ast::node>(checked_code_path)));
		//	env = std::move(std::get<typecheck_environment>(checked_code_path));

		//	// Check the validity of the type of the test path
		//	auto test_type = std::visit(extended_ast::get_type, *branch_path.test_path);
		//	if (!(test_type == types::type(types::atom_type("bool"))))
		//		return typecheck_error{ std::string("Branch number does not have a boolean test") };

		//	branch_path.type = std::visit(extended_ast::get_type, *branch_path.code_path);

		//	return std::make_tuple(
		//		std::move(branch_path),
		//		std::move(env)
		//	);
		//}

		//types::type interpret_type(const extended_ast::node& node, typecheck_environment& env)
		//{
		//	auto x = [&](const auto& n) {
		//		return interpret(n, env);
		//	};
		//	return std::visit(x, node);
		//}

		//types::type interpret(const extended_ast::atom_type& identifier, const typecheck_environment& env)
		//{
		//	auto& type = std::get<std::reference_wrapper<const types::type>>(env.typeof(identifier.name)).get();

		//	if (identifier.name.tags.test(tags::array))
		//	{
		//		return types::array_type(
		//			types::make_unique(type)
		//		);
		//	}
		//	else
		//	{
		//		return type;
		//	}
		//}
		//types::type interpret(const extended_ast::tuple_type& tuple, const typecheck_environment& env)
		//{
		//	types::product_type result;

		//	for (const auto& child : tuple.elements)
		//	{
		//		if (std::holds_alternative<extended_ast::atom_type>(child))
		//		{
		//			result.product.push_back({ "", interpret(std::get<extended_ast::atom_type>(child), env) });
		//		}
		//		else if (std::holds_alternative<extended_ast::function_type>(child))
		//		{
		//			result.product.push_back({ "", interpret(std::get<extended_ast::function_type>(child), env) });
		//		}
		//	}
		//	return result;
		//}
		//types::type interpret(const extended_ast::function_type& function, const typecheck_environment& env)
		//{
		//	types::product_type from = std::get<types::product_type>(interpret(std::move(function.from), env));
		//	types::product_type to = std::get<types::product_type>(interpret(std::move(function.to), env));

		//	return types::function_type(types::make_unique(from), types::make_unique(to));
		//}
		//types::type interpret(const extended_ast::atom_declaration& atom, typecheck_environment& env)
		//{
		//	auto res = interpret(atom.type_name, env);
		//	env.set_type(atom.name, res);
		//	return res;
		//}
		//types::type interpret(const extended_ast::function_declaration& function, typecheck_environment& env)
		//{
		//	auto res = interpret(function.type_name, env);
		//	env.set_type(function.name, res);
		//	return res;
		//}
		//types::type interpret(const extended_ast::tuple_declaration& tuple, typecheck_environment& env)
		//{
		//	types::product_type res;
		//	for (decltype(auto) elem : tuple.elements)
		//	{

		//		if (std::holds_alternative<extended_ast::atom_declaration>(elem))
		//		{
		//			auto& atom = std::get<extended_ast::atom_declaration>(elem);
		//			auto atom_type = interpret(atom, env);
		//			res.product.push_back({ atom.name.segments.at(0), atom_type });
		//		}
		//		else if (std::holds_alternative<extended_ast::function_declaration>(elem))
		//		{
		//			auto& func = std::get<extended_ast::function_declaration>(elem);
		//			auto func_type = interpret(func, env);
		//			res.product.push_back({ func.name.segments.at(0), func_type });
		//		}
		//	}
		//	return res;
		//}
		//types::type interpret(const extended_ast::node& node, const typecheck_environment& env)
		//{
		//	throw std::exception("Cannot interpret the type of this node");
		//}
	};
}