#include "extended_ast.h"
#include "typecheck_environment.h"
#include "core_ast.h"

namespace fe
{
	namespace extended_ast
	{
		void integer::typecheck(typecheck_environment& t_env)
		{
			this->set_type(types::atom_type("i32"));
		}

		core_ast::node integer::lower()
		{
			return core_ast::integer(this->value);
		}

		void string::typecheck(typecheck_environment& t_env)
		{
			this->set_type(types::atom_type("str"));
		}

		core_ast::node string::lower()
		{
			return core_ast::string(this->value);
		}

		void identifier::typecheck(typecheck_environment& env)
		{
			set_type(std::get<std::reference_wrapper<const types::type>>(env.typeof(*this)).get());
		
			env.build_access_pattern(*this);
		}

		core_ast::node identifier::lower()
		{
			auto modules = std::vector<std::string>(segments.begin(), segments.end() - 1 - offsets.size());

			return core_ast::identifier{
				std::move(modules),
				std::move(segments.at(modules.size())),
				std::move(offsets)
			};
		}

		void export_stmt::typecheck(typecheck_environment& env)
		{
			set_type(types::atom_type("void"));
		}

		core_ast::node export_stmt::lower()
		{
			return core_ast::no_op();
		}

		void module_declaration::typecheck(typecheck_environment& env)
		{
			env.name = this->name.segments.at(0);
		}

		core_ast::node module_declaration::lower()
		{
			return core_ast::no_op();
		}

		void atom_type::typecheck(typecheck_environment& env)
		{
			auto& type = std::get<std::reference_wrapper<const types::type>>(env.typeof(name)).get();

			if (name.tags.test(tags::array))
			{
				set_type(types::array_type(
					types::make_unique(type)
				));
			}
			else
			{
				set_type(type);
			}
		}

		core_ast::node atom_type::lower()
		{
			throw lower_error{ "Cannot lower atom type" };
		}

		void tuple_type::typecheck(typecheck_environment& env)
		{
			types::product_type result;

			for (auto& child : elements)
			{
				if (std::holds_alternative<atom_type>(child))
				{
					std::get<atom_type>(child).typecheck(env);
					result.product.push_back({ "", std::get<atom_type>(child).get_type() });
				}
				else if (std::holds_alternative<function_type>(child))
				{
					std::get<function_type>(child).typecheck(env);
					result.product.push_back({ "", std::get<function_type>(child).get_type() });
				}
			}

			set_type(result);
		}

		core_ast::node tuple_type::lower()
		{
			throw lower_error{ "Cannot lower tupletype" };
		}

		void function_type::typecheck(typecheck_environment& env)
		{
			from.typecheck(env);
			to.typecheck(env);

			set_type(types::function_type(types::make_unique(from.get_type()), types::make_unique(to.get_type())));
		}

		core_ast::node function_type::lower()
		{
			throw lower_error{ "Cannot lower function type" };
		}

		void atom_declaration::typecheck(typecheck_environment& env)
		{
			type_name.typecheck(env);
			env.set_type(name, type_name.get_type());
			set_type(type_name.get_type());
		}

		core_ast::node atom_declaration::lower()
		{
			throw lower_error{ "Cannot lower atom declaration" };
		}

		void function_declaration::typecheck(typecheck_environment& env)
		{
			type_name.typecheck(env);
			env.set_type(name, type_name.get_type());
			set_type(type_name.get_type());
		}

		core_ast::node function_declaration::lower()
		{
			throw lower_error{ "Cannot lower function declaration" };
		}

		void tuple_declaration::typecheck(typecheck_environment& env)
		{
			types::product_type res;
			for (decltype(auto) elem : elements)
			{
				if (std::holds_alternative<extended_ast::atom_declaration>(elem))
				{
					auto& atom = std::get<extended_ast::atom_declaration>(elem);
					atom.typecheck(env);
					res.product.push_back({ atom.name.segments.at(0), atom.get_type() });
				}
				else if (std::holds_alternative<extended_ast::function_declaration>(elem))
				{
					auto& func = std::get<extended_ast::function_declaration>(elem);
					func.typecheck(env);
					res.product.push_back({ func.name.segments.at(0), func.get_type() });
				}
			}
			set_type(res);
		}

		core_ast::node tuple_declaration::lower()
		{
			throw lower_error{ "Cannot lower tuple declaration" };
		}

		void tuple::typecheck(typecheck_environment& env)
		{
			auto new_type = types::product_type();

			for (decltype(auto) element : children)
			{
				element->typecheck(env);

				new_type.product.push_back({ "", element->get_type() });
			}

			set_type(std::move(new_type));
		}

		core_ast::node tuple::lower()
		{
			std::vector<core_ast::node> children;
			for (decltype(auto) child : this->children)
			{
				auto lowered_child = child->lower();
				children.push_back(std::move(lowered_child));
			}

			return core_ast::tuple(move(children), this->get_type());
		}

		void block::typecheck(typecheck_environment& env)
		{
			types::type final_type;
			for (decltype(auto) element : children)
			{
				element->typecheck(env);
				final_type = element->get_type();
			}

			set_type(std::move(final_type));
		}

		core_ast::node block::lower()
		{
			std::vector<core_ast::node> children;
			for (decltype(auto) child : this->children)
			{
				auto lowered_child = child->lower();
				children.push_back(std::move(lowered_child));
			}

			return core_ast::block(move(children), this->get_type());
		}

		function_call::function_call(const function_call& other) : node(other), id(other.id), params(other.params->copy()), tags(other.tags) {}

	
		void function_call::typecheck(typecheck_environment& env)
		{
			params->typecheck(env);

			auto& argument_type = params->get_type();

			auto function_or_type_or_error = env.typeof(id);

			auto function_or_type = std::get<std::reference_wrapper<const types::type>>(function_or_type_or_error).get();
			if (std::holds_alternative<types::function_type>(function_or_type))
			{
				auto function_type = std::get<types::function_type>(function_or_type);

				// Check function signature against call signature
				if (!(argument_type == *function_type.from))
				{
					throw typecheck_error{
						"Function call signature does not match function signature:\n"
						+ std::visit(types::to_string, argument_type) + "\n"
						+ std::visit(types::to_string, *function_type.from)
					};
				}

				set_type(std::move(*function_type.to));
			}
			else if (std::holds_alternative<types::product_type>(function_or_type))
			{
				auto product_type = std::get<types::product_type>(function_or_type);

				// Check type signature against call signature
				if (!(argument_type == types::type(product_type)))
				{
					throw typecheck_error{
						"Function call signature does not match function signature:\n"
						+ std::visit(types::to_string, argument_type) + "\n"
						+ std::visit(types::to_string, types::type(product_type))
					};
				}

				set_type(std::move(product_type));
			}
			else
			{
				throw typecheck_error{
					"Function call can only call constructor or function"
				};
			}
		}

		core_ast::node function_call::lower()
		{
			auto lowered_params = params->lower();

			auto lowered_id = id.lower();
			auto& getter = std::get<core_ast::identifier>(lowered_id);

			return core_ast::function_call(
				std::move(getter),
				move(core_ast::make_unique(lowered_params)),
				std::move(get_type())
			);
		}

		void type_declaration::typecheck(typecheck_environment& env)
		{
			set_type(types::atom_type("void"));

			types.typecheck(env);

			env.set_type(id.segments.at(0), types.get_type());
		}

		core_ast::node type_declaration::lower()
		{
			auto func_param_type = std::get<types::product_type>(this->types.get_type());

			auto return_type = func_param_type;
			auto return_statement = core_ast::make_unique(
				core_ast::identifier({}, { "_arg0" }, {})
			);

			auto parameter_name = core_ast::identifier({}, { "_arg0" }, {});

			auto lowered_id = std::get<core_ast::identifier>(id.lower());

			return core_ast::set(
				std::move(lowered_id),

				core_ast::make_unique(core_ast::function(
					std::optional<core_ast::identifier>(),
					parameter_name,
					std::move(return_statement),
					types::function_type(
						types::make_unique(types::product_type(return_type)),
						types::make_unique(types::product_type(return_type))
					)
				)),

				types::function_type(
					types::make_unique(types::product_type(return_type)),
					types::make_unique(types::product_type(return_type))
				)
			);
		}

		assignment::assignment(const assignment& other) : node(other), id(other.id), value(other.value->copy()), tags(other.tags) {}

		void assignment::typecheck(typecheck_environment& env)
		{
			value->typecheck(env);

			env.set_type(id, value->get_type());
			id.set_type(value->get_type());

			set_type(types::atom_type("void"));
		}

		core_ast::node assignment::lower()
		{
			auto lowered_value = value->lower();

			auto lowered_id = id.lower();
			auto& getter = std::get<core_ast::identifier>(lowered_id);

			return core_ast::set(
				std::move(getter),
				std::make_unique<core_ast::node>(move(lowered_value)),
				id.get_type()
			);
		}

		function::function(const function& other) : node(other), name(other.name), from(other.from), to(other.to->copy()), body(other.body->copy()), tags(other.tags) {}

		void function::typecheck(typecheck_environment& env)
		{
			// Check 'from' type expression
			from.typecheck(env);

			// Check 'to' type expression
			to->typecheck(env);

			set_type(types::function_type(types::make_unique(from.get_type()), types::make_unique(to->get_type())));

			// Allow recursion
			if (name.has_value())
			{
				env.set_type(name.value(), get_type());
			}

			body->typecheck(env);

			if (!(body->get_type() == to->get_type()))
			{
				throw typecheck_error{ "Given return type is not the same as the type of the body" };
			}
		}

		core_ast::node function::lower()
		{
			auto name = std::optional<core_ast::identifier>();

			if (this->name.has_value())
			{
				auto lowered_name = this->name.value().lower();
				auto getter = std::get<core_ast::identifier>(lowered_name);
				name = std::make_optional(getter);
			}

			auto lowered_body = body->lower();

			std::vector<core_ast::identifier> parameter_names;
			for (decltype(auto) identifier : from.elements)
			{
				if (std::holds_alternative<extended_ast::atom_declaration>(identifier))
				{
					auto& atom = std::get<extended_ast::atom_declaration>(identifier);
					parameter_names.push_back(std::get<core_ast::identifier>(atom.name.lower()));
				}
				if (std::holds_alternative<extended_ast::function_declaration>(identifier))
				{
					auto& function = std::get<extended_ast::function_declaration>(identifier);
					parameter_names.push_back(std::get<core_ast::identifier>(function.name.lower()));
				}
			}

			return core_ast::function{
				std::move(name),
				std::move(parameter_names),
				core_ast::make_unique(lowered_body),
				std::move(get_type())
			};
		}

		conditional_branch_path::conditional_branch_path(const conditional_branch_path& other) : node(other), test_path(other.test_path->copy()), code_path(other.code_path->copy()), tags(other.tags) {}

		void conditional_branch_path::typecheck(typecheck_environment& env)
		{
			test_path->typecheck(env);
			code_path->typecheck(env);

			// Check the validity of the type of the test path
			auto test_type = test_path->get_type();
			if (!(test_type == types::type(types::atom_type("bool"))))
				throw typecheck_error{ std::string("Branch number does not have a boolean test") };

			set_type(code_path->get_type());
		}

		core_ast::node conditional_branch_path::lower()
		{
			throw lower_error{ "Cannot lower conditional branch path" };
		}

		void conditional_branch::typecheck(typecheck_environment& env)
		{
			types::type common_type = types::unset_type();

			for (uint32_t branch_count = 0; branch_count < branches.size(); branch_count++)
			{
				branches.at(branch_count).typecheck(env);

				// In first iteration
				if (common_type == types::type(types::unset_type()))
					common_type = branches.at(branch_count).get_type();

				// Check other elements are the same
				if (!(common_type == branches.at(branch_count).get_type()))
					throw typecheck_error{ std::string("Branch is of a different type than those before it") };
			}

			set_type(common_type);
		}

		core_ast::node conditional_branch::lower()
		{
			core_ast::branch child_branch(nullptr, nullptr, nullptr);
			core_ast::branch* current_child = &child_branch;

			for (auto it = branches.begin(); it != branches.end(); it++)
			{
				if (it != branches.begin())
				{
					current_child->false_path = std::make_unique<core_ast::node>(core_ast::branch(nullptr, nullptr, nullptr));
					current_child = &std::get<core_ast::branch>(*current_child->false_path.get());
				}

				auto lowered_test = it->test_path->lower();
				auto lowered_code = it->code_path->lower();

				current_child->test_path = core_ast::make_unique(std::move(lowered_test));
				current_child->true_path = core_ast::make_unique(std::move(lowered_code));
				current_child->false_path = core_ast::make_unique(core_ast::no_op());
			}

			return child_branch;
		}
	}
}