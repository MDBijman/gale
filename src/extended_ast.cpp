#include "extended_ast.h"
#include "typecheck_environment.h"
#include "core_ast.h"

namespace fe
{
	namespace extended_ast
	{
		// Integer

		void integer::typecheck(typecheck_environment& t_env)
		{
			this->set_type(types::atom_type("i32"));
		}

		core_ast::node* integer::lower()
		{
			return new core_ast::integer(this->value);
		}

		// String

		void string::typecheck(typecheck_environment& t_env)
		{
			this->set_type(types::atom_type("str"));
		}

		core_ast::node* string::lower()
		{
			return new core_ast::string(this->value);
		}

		// Identifier

		void identifier::typecheck(typecheck_environment& env)
		{
			set_type(std::get<std::reference_wrapper<const types::type>>(env.typeof(*this)).get());
		
			env.build_access_pattern(*this);
		}

		core_ast::node* identifier::lower()
		{
			auto modules = std::vector<std::string>(segments.begin(), segments.end() - 1 - offsets.size());

			return new core_ast::identifier{
				std::move(modules),
				std::move(segments.at(modules.size())),
				std::move(offsets)
			};
		}

		// Export Statement

		void export_stmt::typecheck(typecheck_environment& env)
		{
			set_type(types::atom_type("void"));
		}

		core_ast::node* export_stmt::lower()
		{
			return new core_ast::no_op();
		}

		// Module Declaration

		void module_declaration::typecheck(typecheck_environment& env)
		{
			env.name = this->name.segments.at(0);
		}

		core_ast::node* module_declaration::lower()
		{
			return new core_ast::no_op();
		}

		// Reference Type

		void reference_type::typecheck(typecheck_environment & env)
		{
			child->typecheck(env);
			set_type(types::reference_type(types::make_unique(child->get_type())));
		}

		core_ast::node* reference_type::lower()
		{
			throw lower_error{"Unimplemented"};
		}

		// Type Atom

		void type_atom::typecheck(typecheck_environment& env)
		{
			type->typecheck(env);
			auto type = this->type->get_type();

			set_type(type);
		}

		core_ast::node* type_atom::lower()
		{
			return new core_ast::no_op();
		}

		// Tuple Type

		type_tuple::type_tuple(std::vector<unique_node>&& children) :
			node(types::unset_type())
		{
			for (decltype(auto) child : children)
			{
				if(dynamic_cast<type_atom*>(child.get()) || dynamic_cast<function_type*>(child.get()))
					elements.push_back(std::move(child));
				else
					throw cst_to_ast_error{ "Type tuple can only contain atom and function declarations" };
			}
		}

		void type_tuple::typecheck(typecheck_environment& env)
		{
			types::product_type result;

			for (auto& child : elements)
			{
				child->typecheck(env);
				result.product.push_back({ "", child->get_type() });
			}

			set_type(result);
		}

		core_ast::node* type_tuple::lower()
		{
			throw lower_error{ "Cannot lower tupletype" };
		}

		// Function Type

		void function_type::typecheck(typecheck_environment& env)
		{
			from.typecheck(env);
			to.typecheck(env);

			set_type(types::function_type(types::make_unique(from.get_type()), types::make_unique(to.get_type())));
		}

		core_ast::node* function_type::lower()
		{
			throw lower_error{ "Cannot lower function type" };
		}

		// Atom Declaration

		atom_declaration::atom_declaration(std::vector<unique_node>&& children) :
			node(types::unset_type()),
			type_expression(std::move(children.at(0))),
			name(*dynamic_cast<identifier*>(children.at(1).get())) {}

		void atom_declaration::typecheck(typecheck_environment& env)
		{
			type_expression->typecheck(env);
			env.set_type(name, type_expression->get_type());
			set_type(type_expression->get_type());
		}

		core_ast::node* atom_declaration::lower()
		{
			throw lower_error{ "Cannot lower atom declaration" };
		}

		// Tuple Declaration

		tuple_declaration::tuple_declaration(std::vector<unique_node>&& children) : node(types::unset_type())
		{
			// Children can be reference, atom_type, or function_declaration

			for(decltype(auto) child : children)
			{
				elements.push_back(std::move(child));
			}
		}

		void tuple_declaration::typecheck(typecheck_environment& env)
		{
			types::product_type res;
			for (decltype(auto) elem : elements)
			{
				elem->typecheck(env);

				if (auto atom = dynamic_cast<atom_declaration*>(elem.get()))
				{
					res.product.push_back({ atom->name.segments.at(0), atom->get_type() });
				}
			}
			set_type(res);
		}

		core_ast::node* tuple_declaration::lower()
		{
			return new core_ast::no_op();
		}

		// Tuple

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

		core_ast::node* tuple::lower()
		{
			std::vector<core_ast::unique_node> children;
			for (decltype(auto) child : this->children)
			{
				children.push_back(core_ast::unique_node(child->lower()));
			}

			return new core_ast::tuple(move(children), this->get_type());
		}

		// Block 

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

		core_ast::node* block::lower()
		{
			std::vector<core_ast::unique_node> children;
			for (decltype(auto) child : this->children)
			{
				children.push_back(core_ast::unique_node(child->lower()));
			}

			return new core_ast::block(move(children), this->get_type());
		}

		// Function Call

		function_call::function_call(const function_call& other) : node(other), id(other.id), params(other.params->copy()), tags(other.tags) {}

		void function_call::typecheck(typecheck_environment& env)
		{
			params->typecheck(env);

			auto& argument_type = params->get_type();

			auto type_or_error = env.typeof(id);
			if (std::holds_alternative<type_env_error>(type_or_error))
			{
				throw typecheck_error{ std::get<type_env_error>(type_or_error).message };
			}

			auto function_or_type = std::get<std::reference_wrapper<const types::type>>(type_or_error).get();
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

		core_ast::node* function_call::lower()
		{
			auto name = *dynamic_cast<core_ast::identifier*>(id.lower());
			auto lowered_params = params->lower();

			return new core_ast::function_call(
				std::move(name),
				core_ast::unique_node(lowered_params),
				std::move(get_type())
			);
		}

		// Assignment

		assignment::assignment(const assignment& other) : node(other), id(other.id), value(other.value->copy()), tags(other.tags) {}

		void assignment::typecheck(typecheck_environment& env)
		{
			if (auto fun = dynamic_cast<function*>(value.get()))
			{
				fun->name = this->id;
			}

			value->typecheck(env);

			env.set_type(id, value->get_type());
			id.set_type(value->get_type());

			set_type(types::atom_type("void"));
		}

		core_ast::node* assignment::lower()
		{
			auto name = *dynamic_cast<core_ast::identifier*>(id.lower());
			auto value = this->value->lower();

			return new core_ast::set(
				std::move(name),
				core_ast::unique_node(value),
				id.get_type()
			);
		}

		// Function

		function::function(const function& other) : node(other), name(other.name), from(other.from), to(other.to->copy()), body(other.body->copy()), tags(other.tags) {}
		function::function(std::vector<unique_node>&& children) :
			node(types::unset_type()),
			name(std::optional<identifier>()),
			from(*dynamic_cast<tuple_declaration*>(children.at(0).get())),
			to(std::move(children.at(1))),
			body(std::move(children.at(2))) {}

		void function::typecheck(typecheck_environment& env)
		{
			auto function_env = env;

			// Check 'from' type expression
			from.typecheck(function_env);

			// Check 'to' type expression
			to->typecheck(function_env);

			set_type(types::function_type(types::make_unique(from.get_type()), types::make_unique(to->get_type())));

			// Allow recursion
			if (name.has_value())
			{
				function_env.set_type(name.value(), get_type());
			}

			body->typecheck(function_env);

			if (!(body->get_type() == to->get_type()))
			{
				throw typecheck_error{ "Given return type is not the same as the type of the body" };
			}
		}

		core_ast::node* function::lower()
		{
			auto name = std::optional<core_ast::identifier>();

			if (this->name.has_value())
			{
				auto lowered_name = this->name.value().lower();
				auto getter = dynamic_cast<core_ast::identifier*>(lowered_name);
				name = std::make_optional(*getter);
			}

			auto lowered_body = body->lower();

			std::vector<core_ast::identifier> parameter_names;
			for (decltype(auto) identifier : from.elements)
			{
				if (auto atom = dynamic_cast<atom_declaration*>(identifier.get()))
				{
					parameter_names.push_back(*dynamic_cast<core_ast::identifier*>(atom->name.lower()));
				}
			}

			return new core_ast::function{
				std::move(name),
				std::move(parameter_names),
				core_ast::unique_node(std::move(lowered_body)),
				std::move(get_type())
			};
		}

		// Conditional Branch Path

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

		core_ast::node* conditional_branch_path::lower()
		{
			throw lower_error{ "Cannot lower conditional branch path" };
		}

		// Condition Branch

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

		core_ast::node* conditional_branch::lower()
		{
			core_ast::branch* child_branch = new core_ast::branch(nullptr, nullptr, nullptr);
			core_ast::branch* current_child = child_branch;

			for (auto it = branches.begin(); it != branches.end(); it++)
			{
				if (it != branches.begin())
				{
					current_child->false_path = core_ast::unique_node(new core_ast::branch(nullptr, nullptr, nullptr));
					current_child = static_cast<core_ast::branch*>(current_child->false_path.get());
				}

				auto lowered_test = it->test_path->lower();
				auto lowered_code = it->code_path->lower();

				current_child->test_path = core_ast::unique_node(std::move(lowered_test));
				current_child->true_path = core_ast::unique_node(std::move(lowered_code));
				current_child->false_path = core_ast::unique_node(new core_ast::no_op());
			}

			return child_branch;
		}
	
		// Type Definition
		
		type_definition::type_definition(std::vector<unique_node>&& children) :
			node(types::atom_type{ "void" }),
			id(std::move(*dynamic_cast<identifier*>(children.at(0).get()))),
			types(std::move(*dynamic_cast<tuple_declaration*>(children.at(1).get()))) {}

		void type_definition::typecheck(typecheck_environment& env)
		{
			types.typecheck(env);
			env.set_type(id, types.get_type());
			id.set_type(types.get_type());
			set_type(types.get_type());
		}

		core_ast::node* type_definition::lower()
		{
			auto function_type = std::get<types::product_type>(types.get_type());

			auto return_statement = core_ast::unique_node(new core_ast::identifier({}, { "_arg0" }, {}));
			auto parameter_name = core_ast::identifier({}, { "_arg0" }, {});

			return new core_ast::set(
				std::move(*dynamic_cast<core_ast::identifier*>(id.lower())),

				core_ast::unique_node(new core_ast::function(
					std::optional<core_ast::identifier>(),
					parameter_name,
					std::move(return_statement),
					types::function_type(
						types::make_unique(types::product_type(function_type)),
						types::make_unique(types::product_type(function_type))
					)
				)),

				types::function_type(
					types::make_unique(types::product_type(function_type)),
					types::make_unique(types::product_type(function_type))
				)
			);
		}

		// Array Type

		array_type::array_type(std::vector<unique_node>&& children) :
			node(types::unset_type()),
			child(std::move(children.at(0))) {}

		array_type::array_type(const array_type& other) : node(other), child(other.child->copy()) {}

		array_type::array_type(array_type&& other) : node(std::move(other)), child(std::move(other.child)) {}

		array_type& array_type::operator=(array_type&& other)
		{
			set_type(other.get_type());
			this->child = std::move(other.child);
			return *this;
		}

		node* array_type::copy() 
		{
			return new array_type(*this);
		}

		void array_type::typecheck(typecheck_environment& env)
		{
			child->typecheck(env);

			set_type(types::array_type(types::make_unique(child->get_type())));
		}

		core_ast::node* array_type::lower()
		{
			throw lower_error{ "Cannot lower array type" };
		}

		// Reference

		reference::reference(std::vector<unique_node>&& children) :
			node(types::unset_type()),
			child(std::move(children.at(0))) {}

		reference::reference(const reference& other) :
			node(other),
			child(other.child->copy()) {}

		reference::reference(reference && other) :
			node(std::move(other)),
			child(std::move(other.child)) {}

		reference& reference::operator=(reference && other)
		{
			set_type(other.get_type());
			this->child = std::move(other.child);
			return *this;
		}

		node* reference::copy() 
		{
			return new reference(*this);
		}

		void reference::typecheck(typecheck_environment & env)
		{
			child->typecheck(env);

			set_type(types::reference_type(types::make_unique(child->get_type())));
		}

		core_ast::node* reference::lower()
		{
			return new core_ast::reference(core_ast::unique_node(this->child->lower()));
		}
	}
}	