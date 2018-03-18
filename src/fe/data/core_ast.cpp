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

		no_op::no_op() : type(new types::unset_type()) {}

		no_op::no_op(const no_op& other) : type(other.type->copy()) {}
		no_op& no_op::operator=(const no_op& other)
		{
			this->type = types::unique_type(other.type->copy());
			return *this;
		}
		no_op::no_op(no_op&& other) : type(std::move(other.type)) {}
		no_op& no_op::operator=(no_op&& other)
		{
			this->type = std::move(other.type);
			return *this;
		}

		values::unique_value no_op::interp(runtime_environment &)
		{
			return values::make_unique(values::void_value());
		}

		// Integer

		integer::integer(values::integer val) : type(new types::atom_type("std.i32")), value(val) {}
		
		integer::integer(const integer& other) : value(other.value), type(other.type->copy()) {}
		integer& integer::operator=(const integer& other)
		{
			this->value = other.value;
			this->type = types::unique_type(other.type->copy());
			return *this;
		}
		integer::integer(integer&& other) : value(std::move(other.value)), type(std::move(other.type)) {}
		integer& integer::operator=(integer&& other)
		{
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}

		values::unique_value integer::interp(runtime_environment &)
		{
			return values::unique_value(this->value.copy());
		}

		// String

		string::string(values::string val) : type(new types::atom_type("str")), value(val) {}
		
		string::string(const string& other) : value(other.value), type(other.type->copy()) {}
		string& string::operator=(const string& other)
		{
			this->value = other.value;
			this->type = types::unique_type(other.type->copy());
			return *this;
		}
		string::string(string&& other) : value(std::move(other.value)), type(std::move(other.type)) {}
		string& string::operator=(string&& other)
		{
			this->value = std::move(other.value);
			this->type = std::move(other.type);
			return *this;
		}

		values::unique_value string::interp(runtime_environment &)
		{
			return values::unique_value(this->value.copy());
		}

		// Identifier

		identifier::identifier(std::vector<std::string> modules, std::string name, std::vector<int> offsets, 
			std::size_t depth, types::unique_type t) : 
			modules(std::move(modules)), variable_name(std::move(name)), 
			offsets(std::move(offsets)), type(std::move(t)), scope_depth(depth) {}

		identifier::identifier(const identifier& other) : 
			modules(other.modules), variable_name(other.variable_name), 
			offsets(other.offsets), type(other.type->copy()), scope_depth(other.scope_depth) {}

		identifier& identifier::operator=(const identifier& other)
		{
			this->modules = other.modules;
			this->variable_name = other.variable_name;
			this->offsets = other.offsets;
			this->type = types::unique_type(other.type->copy());
			this->scope_depth = other.scope_depth;
			return *this;
		}

		identifier::identifier(identifier&& other) : 
			modules(std::move(other.modules)), variable_name(std::move(other.variable_name)), 
			offsets(std::move(other.offsets)), type(std::move(other.type)), scope_depth(std::move(other.scope_depth))
		{}

		identifier& identifier::operator=(identifier&& other)
		{
			this->modules = std::move(other.modules);
			this->variable_name = std::move(other.variable_name);
			this->offsets = std::move(other.offsets);
			this->type = std::move(other.type);
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

		set::set(identifier id, unique_node value, types::unique_type t) : lhs(std::move(id)), value(std::move(value)),
			type(std::move(t)) {}

		set::set(identifier_tuple lhs, unique_node value, types::unique_type t) : lhs(lhs), value(std::move(value)), 
			type(std::move(t)) {}
		
		set::set(const set& other) : lhs(other.lhs), value(unique_node(other.value->copy())), 
			type(other.type->copy()) {}

		set& set::operator=(const set& other)
		{
			this->lhs = other.lhs;
			this->type = types::unique_type(other.type->copy());
			this->value = unique_node(other.value->copy());
			return *this;
		}

		set::set(set&& other) : lhs(std::move(other.lhs)), value(std::move(other.value)), 
			type(std::move(other.type)) {}

		set& set::operator=(set&& other)
		{
			this->lhs = std::move(other.lhs);
			this->type = std::move(other.type);
			this->value = std::move(other.value);
			return *this;
		}

		values::unique_value set::interp(runtime_environment& env)
		{
			auto val = this->value->interp(env);

			// Assign value to lhs, which is slightly complex when the lhs is a destructuring
			std::function<void(std::variant<identifier, identifier_tuple>&, values::value&)> interp
				= [&env, &interp](std::variant<identifier, identifier_tuple>& ids, values::value& value) 
			{
				if(std::holds_alternative<identifier_tuple>(ids))
				{
					auto& id_tuple = std::get<identifier_tuple>(ids);
					
					assert(dynamic_cast<values::tuple*>(&value) != nullptr);

					auto tuple = static_cast<values::tuple*>(&value);

					assert(id_tuple.ids.size() == tuple->content.size());

					for (int i = 0; i < id_tuple.ids.size(); i++)
					{
						interp(id_tuple.ids.at(i), *tuple->content.at(i));
					}
				}
				else if(std::holds_alternative<identifier>(ids))
				{
					auto& id = std::get<identifier>(ids);
					env.set_value(id.variable_name, values::unique_value(value.copy()));
				}
			};
			interp(this->lhs, *val);

			return val;
		}

		// Function

		function::function(identifier&& name, std::variant<std::vector<identifier>, identifier>&& parameters, 
			unique_node&& body, types::unique_type t) : name(std::move(name)), parameters(std::move(parameters)), 
			type(std::move(t)), body(std::move(body)) {}

		function::function(const function& other) : name(other.name), parameters(other.parameters), 
			body(other.body->copy()), type(other.type->copy()) {}

		function& function::operator=(const function& other)
		{
			this->name = other.name;
			this->parameters = other.parameters;
			this->body.reset(other.body->copy());
			this->type = types::unique_type(other.type->copy());
			return *this;
		}

		function::function(function&& other) : name(std::move(other.name)), parameters(std::move(other.parameters)), 
			body(std::move(other.body)), type(std::move(other.type)) {}

		function& function::operator=(function&& other)
		{
			this->name = std::move(other.name);
			this->parameters = std::move(other.parameters);
			this->body = std::move(other.body);
			this->type = std::move(other.type);
			return *this;
		}

		values::unique_value function::interp(runtime_environment& env)
		{
			auto unique = values::unique_value(new values::function(unique_node(this->copy())));
			env.set_value(this->name.variable_name, values::unique_value(unique->copy()));
			return values::unique_value(std::move(unique));
		}

		// Tuple

		tuple::tuple(std::vector<unique_node> children, types::unique_type t) : type(std::move(t)), 
			children(std::move(children)) {}

		tuple::tuple(std::vector<unique_node> children, types::type & t) : type(t.copy()), 
			children(std::move(children)) {}

		tuple::tuple(const tuple& other) : type(other.type->copy())
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
			this->type = types::unique_type(other.type->copy());
			return *this;
		}
		tuple::tuple(tuple&& other) : type(std::move(other.type)), children(std::move(other.children)) {}

		tuple& tuple::operator=(tuple&& other)
		{
			this->type = std::move(other.type);
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

		block::block(std::vector<unique_node> children, types::unique_type t) : type(std::move(t)), 
			children(std::move(children)) {}

		block::block(const block& other) : type(other.type->copy())
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
			this->type = types::unique_type(other.type->copy());
			return *this;
		}
		block::block(block&& other) : type(std::move(other.type)), children(std::move(other.children)) {}
		block& block::operator=(block&& other)
		{
			this->type = std::move(other.type);
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
			env.pop();
			return res;
		}

		// Function Call

		function_call::function_call(identifier id, unique_node parameter, types::unique_type t)
			: type(std::move(t)), id(std::move(id)), parameter(std::move(parameter)) {}

		function_call::function_call(const function_call& other) : id(other.id), parameter(other.parameter->copy()), 
			type(other.type->copy()) {}

		function_call& function_call::operator=(const function_call& other)
		{
			this->id = other.id;
			this->parameter.reset(other.parameter->copy());
			this->type = types::unique_type(other.type->copy());
			return *this;
		}

		function_call::function_call(function_call&& other) : id(std::move(other.id)), 
			parameter(std::move(other.parameter)), type(std::move(other.type)) {}

		function_call& function_call::operator=(function_call&& other)
		{
			this->id = std::move(other.id);
			this->parameter = std::move(other.parameter);
			this->type = std::move(other.type);
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
						function_env.set_value(id.variable_name, std::move(param_tuple->content.at(i)));
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
			if (other.type)
				type = types::unique_type(other.type->copy());
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
			this->type = types::unique_type(other.type->copy());
			return *this;
		}
		branch::branch(branch&& other) : type(std::move(other.type)), paths(std::move(other.paths)) {}
		branch& branch::operator=(branch&& other)
		{
			this->paths = std::move(other.paths);
			this->type = std::move(other.type);
			return *this;
		}

		values::unique_value branch::interp(runtime_environment& env)
		{

			for (auto& path : paths)
			{
				env.push();
				auto test_res = dynamic_cast<values::boolean*>(path.first->interp(env).get());

				if (test_res->val)
				{
					auto res = path.second->interp(env);
					env.pop();
					return res;
				}

				env.pop();
			}

			throw interp_error{"No branch matched"};
		}

		reference::reference(unique_node exp) : exp(std::move(exp)) {}

		reference::reference(const reference& other) : exp(unique_node(other.exp->copy())), type(other.type->copy()) {}
		reference& reference::operator=(const reference& other)
		{
			this->exp = unique_node(other.exp->copy());
			this->type = types::unique_type(other.type->copy());
			return *this;
		}

		reference::reference(reference&& other) : exp(std::move(other.exp)), type(std::move(other.type)) {}
		reference& reference::operator=(reference&& other)
		{
			this->exp = std::move(other.exp);
			this->type = std::move(other.type);
			return *this;
		}

		values::unique_value reference::interp(runtime_environment& env)
		{
			return exp->interp(env);
		}

		// While Loop

		while_loop::while_loop(unique_node test_code, unique_node body) : test(std::move(test_code)),
			body(std::move(body)) {}

		while_loop::while_loop(const while_loop& other) : test(unique_node(other.test->copy())),
			body(unique_node(other.body->copy())), type(types::unique_type(other.type->copy())) {}

		while_loop& while_loop::operator=(const while_loop& other)
		{
			this->body = unique_node(other.body->copy());
			this->test = unique_node(other.test->copy());
			this->type = types::unique_type(other.type->copy());
			return *this;
		}

		while_loop::while_loop(while_loop&& other) : test(std::move(other.test)), body(std::move(other.body)),
			type(std::move(other.type)) {}

		while_loop& while_loop::operator=(while_loop&& other)
		{
			this->body = std::move(other.body);
			this->test = std::move(other.test);
			this->type = std::move(other.type);
			return *this;
		}

		values::unique_value while_loop::interp(runtime_environment& env)
		{
			values::unique_value test_res = test->interp(env);
			bool running = dynamic_cast<values::boolean*>(test_res.get())->val;
			while (running)
			{
				body->interp(env);
				running = dynamic_cast<values::boolean*>(test_res.get())->val;
			}

			return values::unique_value();
		}
	}
}