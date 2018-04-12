#include "fe/data/core_ast.h"
#include "fe/data/values.h"
#include "fe/pipeline/error.h"
#include "fe/data/runtime_environment.h"
#include <vector>
#include <assert.h>

namespace fe
{
	namespace core_ast
	{
		// No op

		no_op::no_op() {}

		no_op::no_op(const no_op& other) {}
		no_op& no_op::operator=(const no_op& other)
		{
			return *this;
		}
		no_op::no_op(no_op&& other) {}
		no_op& no_op::operator=(no_op&& other)
		{
			return *this;
		}

		values::unique_value no_op::interp(runtime_environment &)
		{
			return values::make_unique(values::void_value());
		}

		// Identifier

		identifier::identifier(std::vector<std::string> modules, std::string name, std::vector<size_t> offsets,
			std::size_t depth) :
			modules(std::move(modules)), variable_name(std::move(name)),
			offsets(std::move(offsets)), scope_depth(depth) {}

		identifier::identifier(const identifier& other) :
			modules(other.modules), variable_name(other.variable_name),
			offsets(other.offsets), scope_depth(other.scope_depth) {}

		identifier& identifier::operator=(const identifier& other)
		{
			this->modules = other.modules;
			this->variable_name = other.variable_name;
			this->offsets = other.offsets;
			this->scope_depth = other.scope_depth;
			return *this;
		}

		identifier::identifier(identifier&& other) :
			modules(std::move(other.modules)), variable_name(std::move(other.variable_name)),
			offsets(std::move(other.offsets)), scope_depth(std::move(other.scope_depth))
		{}

		identifier& identifier::operator=(identifier&& other)
		{
			this->modules = std::move(other.modules);
			this->variable_name = std::move(other.variable_name);
			this->offsets = std::move(other.offsets);
			this->scope_depth = std::move(other.scope_depth);
			return *this;
		}

		values::unique_value identifier::interp(runtime_environment& env)
		{
			auto value = env.valueof(*this);
			assert(value.has_value());
			return values::unique_value(value.value()->copy());
		}

		// Set

		set::set(identifier id, bool is_dec, unique_node value) : lhs(std::move(id)),
			is_declaration(is_dec), value(std::move(value)) {}

		set::set(identifier_tuple lhs, unique_node value) : lhs(lhs), value(std::move(value)),
			 is_declaration(true) {}

		set::set(const set& other) : lhs(other.lhs), value(unique_node(other.value->copy())),
			 is_declaration(other.is_declaration) {}

		set& set::operator=(const set& other)
		{
			this->lhs = other.lhs;
			this->value = unique_node(other.value->copy());
			this->is_declaration = other.is_declaration;
			return *this;
		}

		set::set(set&& other) : lhs(std::move(other.lhs)), value(std::move(other.value)),
			 is_declaration(other.is_declaration) {}

		set& set::operator=(set&& other)
		{
			this->lhs = std::move(other.lhs);
			this->value = std::move(other.value);
			this->is_declaration = other.is_declaration;
			return *this;
		}

		values::unique_value set::interp(runtime_environment& env)
		{
			auto val = this->value->interp(env);

			// Assign value to lhs, which is slightly complex when the lhs is a destructuring
			std::function<void(std::variant<identifier, identifier_tuple>&, values::value&)> interp
				= [&env, &interp, this](std::variant<identifier, identifier_tuple>& ids, values::value& value)
			{
				if (std::holds_alternative<identifier_tuple>(ids))
				{
					auto& id_tuple = std::get<identifier_tuple>(ids);

					assert(dynamic_cast<values::tuple*>(&value) != nullptr);

					auto tuple = static_cast<values::tuple*>(&value);

					assert(id_tuple.ids.size() == tuple->val.size());

					for (int i = 0; i < id_tuple.ids.size(); i++)
					{
						interp(id_tuple.ids.at(i), *tuple->val.at(i));
					}
				}
				else if (std::holds_alternative<identifier>(ids))
				{
					auto& id = std::get<identifier>(ids);
					if (is_declaration)
					{
						env.set_value(id.variable_name, values::unique_value(value.copy()));
					}
					else
					{
						env.set_value(id.variable_name, value, id.scope_depth);
					}
				}
			};
			interp(this->lhs, *val);

			return val;
		}

		// Function

		function::function(identifier&& name, std::variant<std::vector<identifier>, identifier>&& parameters,
			unique_node&& body) : name(std::move(name)), parameters(std::move(parameters)),
			body(std::move(body)) {}

		function::function(const function& other) : name(other.name), parameters(other.parameters),
			body(other.body->copy()){}

		function& function::operator=(const function& other)
		{
			this->name = other.name;
			this->parameters = other.parameters;
			this->body.reset(other.body->copy());
			return *this;
		}

		function::function(function&& other) : name(std::move(other.name)), parameters(std::move(other.parameters)),
			body(std::move(other.body)) {}

		function& function::operator=(function&& other)
		{
			this->name = std::move(other.name);
			this->parameters = std::move(other.parameters);
			this->body = std::move(other.body);
			return *this;
		}

		values::unique_value function::interp(runtime_environment& env)
		{
			auto unique = values::unique_value(new values::function(unique_node(this->copy())));
			env.set_value(this->name.variable_name, values::unique_value(unique->copy()));
			return values::unique_value(std::move(unique));
		}

		// Tuple

		tuple::tuple(std::vector<unique_node> children) : children(std::move(children)) {}

		tuple::tuple(const tuple& other) 
		{
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
		}
		tuple& tuple::operator=(const tuple& other)
		{
			this->children.clear();
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
			return *this;
		}
		tuple::tuple(tuple&& other) : children(std::move(other.children)) {}

		tuple& tuple::operator=(tuple&& other)
		{
			this->children = std::move(other.children);
			return *this;
		}

		values::unique_value tuple::interp(runtime_environment& env)
		{
			std::vector<values::unique_value> res;
			for (decltype(auto) tuple_node : this->children)
			{
				res.push_back(tuple_node->interp(env));
			}

			return values::unique_value(new values::tuple(std::move(res)));
		}

		// Block

		block::block(std::vector<unique_node> children) : children(std::move(children)) {}

		block::block(const block& other)
		{
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
		}
		block& block::operator=(const block& other)
		{
			this->children.clear();
			for (decltype(auto) other_child : other.children)
			{
				this->children.push_back(unique_node(other_child->copy()));
			}
			return *this;
		}
		block::block(block&& other) : children(std::move(other.children)) {}
		block& block::operator=(block&& other)
		{
			this->children = std::move(other.children);
			return *this;
		}

		values::unique_value block::interp(runtime_environment& env)
		{
			env.push();
			values::unique_value res;
			for (decltype(auto) block_node : this->children)
			{
				res = block_node->interp(env);
			}
			// Only pop if this is not the root block
			if(env.depth() > 1) env.pop();
			return res;
		}

		// Function Call

		function_call::function_call(identifier id, unique_node parameter)
			: id(std::move(id)), parameter(std::move(parameter)) {}

		function_call::function_call(const function_call& other) : id(other.id), parameter(other.parameter->copy()) {}

		function_call& function_call::operator=(const function_call& other)
		{
			this->id = other.id;
			this->parameter.reset(other.parameter->copy());
			return *this;
		}

		function_call::function_call(function_call&& other) : id(std::move(other.id)),
			parameter(std::move(other.parameter)) {}

		function_call& function_call::operator=(function_call&& other)
		{
			this->id = std::move(other.id);
			this->parameter = std::move(other.parameter);
			return *this;
		}

		values::unique_value function_call::interp(runtime_environment& env)
		{
			auto& function_value = env.valueof(this->id);
			assert(function_value.has_value());

			auto param_value = parameter->interp(env);

			runtime_environment function_env(env, this->id.scope_depth);

			// Function written in cpp
			if (auto function = dynamic_cast<values::native_function*>(function_value.value()))
			{
				// Eval function
				return function->function(move(param_value));
			}
			// Function written in language itself
			else if (auto function = dynamic_cast<values::function*>(function_value.value()))
			{
				function_env.push();
				core_ast::function* core_function = dynamic_cast<core_ast::function*>(function->func.get());

				if (std::holds_alternative<std::vector<core_ast::identifier>>(core_function->parameters))
				{
					auto param_tuple = dynamic_cast<values::tuple*>(param_value.get());

					auto& parameters = std::get<std::vector<core_ast::identifier>>(core_function->parameters);
					for (auto i = 0; i < parameters.size(); i++)
					{
						auto& id = parameters.at(i);
						function_env.set_value(id.variable_name, std::move(param_tuple->val.at(i)));
					}
				}
				else
				{
					// Single argument function
					auto& parameter = std::get<core_ast::identifier>(core_function->parameters);
					function_env.set_value(parameter.variable_name, std::move(param_value));
				}

				return core_function->body->interp(function_env);
			}
		}

		// Branch

		branch::branch(std::vector<std::pair<unique_node, unique_node>> paths) : paths(std::move(paths)) {}

		branch::branch(const branch& other)
		{
			for (auto& path : other.paths)
			{
				this->paths.push_back(std::make_pair(unique_node(path.first->copy()),
					unique_node(path.second->copy())));
			}
		}
		branch& branch::operator=(const branch& other)
		{
			for (auto& path : other.paths)
			{
				this->paths.push_back(std::make_pair(unique_node(path.first->copy()),
					unique_node(path.second->copy())));
			}
			return *this;
		}
		branch::branch(branch&& other) : paths(std::move(other.paths)) {}
		branch& branch::operator=(branch&& other)
		{
			this->paths = std::move(other.paths);
			return *this;
		}

		values::unique_value branch::interp(runtime_environment& env)
		{

			for (auto& path : paths)
			{
				auto test_res = path.first->interp(env);
				auto res_bool = dynamic_cast<values::boolean*>(test_res.get());

				if (res_bool->val)
				{
					auto res = path.second->interp(env);
					return res;
				}

			}

			return values::unique_value(new values::void_value());
		}

		reference::reference(unique_node exp) : exp(std::move(exp)) {}

		reference::reference(const reference& other) : exp(unique_node(other.exp->copy())) {}
		reference& reference::operator=(const reference& other)
		{
			this->exp = unique_node(other.exp->copy());
			return *this;
		}

		reference::reference(reference&& other) : exp(std::move(other.exp)) {}
		reference& reference::operator=(reference&& other)
		{
			this->exp = std::move(other.exp);
			return *this;
		}

		values::unique_value reference::interp(runtime_environment& env)
		{
			return exp->interp(env);
		}

		// While Loop

		while_loop::while_loop(unique_node test_code, unique_node body) : 
			test(std::move(test_code)), body(std::move(body)) {}

		while_loop::while_loop(const while_loop& other) : node(other), test(unique_node(other.test->copy())),
			body(unique_node(other.body->copy())) {}

		while_loop& while_loop::operator=(const while_loop& other)
		{
			this->body = unique_node(other.body->copy());
			this->test = unique_node(other.test->copy());
			return *this;
		}

		while_loop::while_loop(while_loop&& other) : test(std::move(other.test)), body(std::move(other.body)) {}

		while_loop& while_loop::operator=(while_loop&& other)
		{
			this->body = std::move(other.body);
			this->test = std::move(other.test);
			return *this;
		}

		values::unique_value while_loop::interp(runtime_environment& env)
		{
			values::unique_value test_res = test->interp(env);
			bool running = dynamic_cast<values::boolean*>(test_res.get())->val;
			while (running)
			{
				body->interp(env);

				test_res = test->interp(env);
				running = dynamic_cast<values::boolean*>(test_res.get())->val;
			}

			return values::unique_value();
		}
	}
}