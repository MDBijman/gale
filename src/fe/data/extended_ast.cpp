#include "fe/data/extended_ast.h"
#include "fe/data/typecheck_environment.h"
#include "fe/data/core_ast.h"

namespace fe
{
	namespace extended_ast
	{
		// Integer

		void integer::typecheck(typecheck_environment& t_env)
		{
			this->set_type(new types::atom_type("std.i32"));
		}

		core_ast::node* integer::lower()
		{
			return new core_ast::integer(this->value);
		}

		// String

		void string::typecheck(typecheck_environment& t_env)
		{
			this->set_type(new types::atom_type("std.str"));
		}

		core_ast::node* string::lower()
		{
			return new core_ast::string(this->value);
		}

		// Identifier

		void identifier::typecheck(typecheck_environment& env)
		{
			auto& t = env.typeof(*this);
			if (std::holds_alternative<type_env_error>(t))
				throw typecheck_error{ "Type environment error: " + std::get<type_env_error>(t).message };


			set_type(std::get<std::reference_wrapper<types::type>>(t).get().copy());
		
			env.build_access_pattern(*this);
		}

		core_ast::node* identifier::lower()
		{
			auto modules = std::vector<std::string>(segments.begin(), segments.end() - 1 - offsets.size());
			
			return new core_ast::identifier{
				std::move(modules),
				std::move(segments.at(modules.size())),
				std::move(offsets),
				types::unique_type(get_type().copy())
			};
		}

		// Export Statement

		void export_stmt::typecheck(typecheck_environment& env)
		{
			set_type(new types::atom_type("void"));
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
			set_type(new types::reference_type(types::make_unique(child->get_type())));
		}

		core_ast::node* reference_type::lower()
		{
			throw lower_error{"Unimplemented"};
		}

		// Type Atom

		void type_atom::typecheck(typecheck_environment& env)
		{
			type->typecheck(env);
			auto& type = this->type->get_type();

			set_type(type.copy());
		}

		core_ast::node* type_atom::lower()
		{
			return new core_ast::no_op();
		}

		// Tuple Type

		type_tuple::type_tuple(std::vector<unique_node>&& children) :
			node(new types::unset_type())
		{
			for (decltype(auto) child : children)
			{
				if(dynamic_cast<type_atom*>(child.get()) || dynamic_cast<function_type*>(child.get()) || dynamic_cast<type_tuple*>(child.get()))
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
				result.product.push_back({ "", types::unique_type(child->get_type().copy()) });
			}

			set_type(result.copy());
		}

		core_ast::node* type_tuple::lower()
		{
			throw lower_error{ "Cannot lower tupletype" };
		}

		// Function Type

		void function_type::typecheck(typecheck_environment& env)
		{
			from->typecheck(env);
			to->typecheck(env);

			set_type(new types::function_type(from->get_type(), to->get_type()));
		}

		core_ast::node* function_type::lower()
		{
			throw lower_error{ "Cannot lower function type" };
		}

		// Atom Declaration

		atom_declaration::atom_declaration(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			type_expression(std::move(children.at(0))),
			name(*dynamic_cast<identifier*>(children.at(1).get())) {}

		void atom_declaration::typecheck(typecheck_environment& env)
		{
			type_expression->typecheck(env);
			env.set_type(name, types::unique_type(type_expression->get_type().copy()));
			set_type(type_expression->get_type().copy());
		}

		core_ast::node* atom_declaration::lower()
		{
			throw lower_error{ "Cannot lower atom declaration" };
		}

		// Tuple Declaration

		tuple_declaration::tuple_declaration(std::vector<unique_node>&& children) : node(new types::unset_type())
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
					res.product.push_back({ atom->name.segments.at(0), types::unique_type(atom->get_type().copy()) });
				}
			}
			set_type(res.copy());
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

				new_type.product.push_back({ "", types::unique_type(element->get_type().copy()) });
			}

			set_type(new_type.copy());
		}

		core_ast::node* tuple::lower()
		{
			std::vector<core_ast::unique_node> children;
			for (decltype(auto) child : this->children)
			{
				children.push_back(core_ast::unique_node(child->lower()));
			}

			return new core_ast::tuple(move(children), types::unique_type(this->get_type().copy()));
		}

		// Block 

		void block::typecheck(typecheck_environment& env)
		{
			types::unique_type final_type = nullptr;
			for (decltype(auto) element : children)
			{
				element->typecheck(env);
				final_type.reset(element->get_type().copy());
			}

			if (final_type != nullptr)
				set_type(final_type->copy());
			else
				set_type(new types::unset_type());
		}

		core_ast::node* block::lower()
		{
			std::vector<core_ast::unique_node> children;
			for (decltype(auto) child : this->children)
			{
				children.push_back(core_ast::unique_node(child->lower()));
			}

			return new core_ast::block(move(children), types::unique_type(this->get_type().copy()));
		}

		// Function Call

		function_call::function_call(const function_call& other) : node(other), id(other.id), params(other.params->copy()) {}
		
		function_call::function_call(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			id(*dynamic_cast<identifier*>(children.at(0).get())),
			params(std::move(children.at(1)))
		{};

		void function_call::typecheck(typecheck_environment& env)
		{
			params->typecheck(env);

			auto& argument_type = params->get_type();

			auto type_or_error = env.typeof(id);
			if (std::holds_alternative<type_env_error>(type_or_error))
			{
				throw typecheck_error{ std::get<type_env_error>(type_or_error).message };
			}

			auto& function_or_type = std::get<std::reference_wrapper<types::type>>(type_or_error).get();
			if (auto function_type = dynamic_cast<types::function_type*>(&function_or_type))
			{
				// Check function signature against call signature
				if (!(argument_type == function_type->from.get()))
				{
					throw typecheck_error{
						"Function call from signature does not match function signature:\n"
						+ argument_type.to_string() + "\n"
						+ function_type->from->to_string()
					};
				}

				set_type(types::unique_type(function_type->to->copy()));
			}
			else if (auto product_type = dynamic_cast<types::product_type*>(&function_or_type))
			{
				// Check type signature against call signature
				if (!(argument_type == product_type))
				{
					throw typecheck_error{
						"Function call to signature does not match function signature:\n"
						+ argument_type.to_string() + "\n"
						+ product_type->to_string()
					};
				}

				set_type(types::unique_type(product_type->copy()));
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
				types::unique_type(this->get_type().copy())
			);
		}

		// Assignment

		assignment::assignment(const assignment& other) : node(other), lhs(other.lhs), value(other.value->copy()) {}

		void assignment::typecheck(typecheck_environment& env)
		{
			value->typecheck(env);

			std::function<void(std::variant<identifier, identifier_tuple>&, types::type&)> typecheck_tuple = [&](std::variant<identifier, identifier_tuple>& lhs, types::type& type) {
				if (std::holds_alternative<identifier_tuple>(lhs))
				{
					auto product_type = dynamic_cast<types::product_type*>(&type);
					assert(product_type != nullptr);

					auto& ids = std::get<identifier_tuple>(lhs);
					assert(ids.content.size() > 1);
					assert(ids.content.size() == product_type->product.size());

					for (int i = 0; i < ids.content.size(); i++)
					{
						typecheck_tuple(ids.content.at(i), *product_type->product.at(i).second);
					}
				}
				else if (std::holds_alternative<identifier>(lhs))
				{
					auto& id = std::get<identifier>(lhs);

					env.set_type(id, types::unique_type(type.copy()));
				}
				else assert(!"Invalid variant contents");
			};
			typecheck_tuple(this->lhs, value->get_type());

			set_type(new types::atom_type("void"));
		}

		core_ast::node* assignment::lower()
		{
			auto value = this->value->lower();

			std::function<std::variant<core_ast::identifier, core_ast::identifier_tuple>(std::variant<identifier, identifier_tuple>)> lower_ids = [&](std::variant<identifier, identifier_tuple>& lhs) {
				if (std::holds_alternative<identifier>(lhs))
				{
					auto name = *dynamic_cast<core_ast::identifier*>(std::get<identifier>(lhs).lower());
					return std::variant<core_ast::identifier, core_ast::identifier_tuple>(name);
				}
				else if (std::holds_alternative<identifier_tuple>(lhs))
				{
					auto ids = std::get<identifier_tuple>(lhs);

					core_ast::identifier_tuple core_id_tuple;

					for (auto& id : ids.content)
					{
						core_id_tuple.ids.push_back(lower_ids(id));
					}

					return std::variant<core_ast::identifier, core_ast::identifier_tuple>(core_id_tuple);
				}
			};
			auto res = lower_ids(this->lhs);

			if (std::holds_alternative<core_ast::identifier>(res))
			{
				return new core_ast::set(
					std::move(std::get<core_ast::identifier>(res)),
					core_ast::unique_node(value),
					types::unique_type(this->value->get_type().copy())
				);
			}
			else if(std::holds_alternative<core_ast::identifier_tuple>(res))
			{
				return new core_ast::set(
					std::move(std::get<core_ast::identifier_tuple>(res)),
					core_ast::unique_node(value),
					types::unique_type(this->value->get_type().copy())
				);
			}
			else assert(!"The lhs of an assignment must become either an identifier or an identifier tuple");
		}

		// Function

		function::function(const function& other) : node(other), name(other.name), from(other.from->copy()), to(other.to->copy()), body(other.body->copy()) {}
		function::function(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			name(std::optional<identifier>()),
			from(std::move(children.at(0))),
			to(std::move(children.at(1))),
			body(std::move(children.at(2))) {}

		void function::typecheck(typecheck_environment& env)
		{
			auto function_env = env;

			// Check 'from' type expression
			from->typecheck(function_env);

			// Check 'to' type expression
			to->typecheck(function_env);

			set_type(types::make_unique(types::function_type(types::unique_type(from->get_type().copy()), types::unique_type(to->get_type().copy()))));

			// Allow recursion
			if (name.has_value())
			{
				function_env.set_type(name.value(), types::unique_type(get_type().copy()));
			}

			body->typecheck(function_env);

			if (!(body->get_type() == &to->get_type()))
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

			auto parameters = ([&]() -> std::variant<std::vector<core_ast::identifier>, core_ast::identifier> {
				if (auto tuple = dynamic_cast<tuple_declaration*>(from.get()))
				{
					std::vector<core_ast::identifier> parameter_names;
					for (decltype(auto) identifier : tuple->elements)
					{
						if (auto atom = dynamic_cast<atom_declaration*>(identifier.get()))
						{
							parameter_names.push_back(*dynamic_cast<core_ast::identifier*>(atom->name.lower()));
						}
					}
					return parameter_names;
				}
				else if (auto atom = dynamic_cast<atom_declaration*>(from.get()))
				{
					return *dynamic_cast<core_ast::identifier*>(atom->name.lower());
				}
			})();

			return new core_ast::function{
				std::move(name),
				std::move(parameters),
				core_ast::unique_node(std::move(lowered_body)),
				types::unique_type(get_type().copy())
			};
		}

		// Conditional Branch Path

		match_branch::match_branch(const match_branch& other) : node(other), test_path(other.test_path->copy()), code_path(other.code_path->copy()) {}

		void match_branch::typecheck(typecheck_environment& env)
		{
			test_path->typecheck(env);
			code_path->typecheck(env);

			// Check the validity of the type of the test path
			auto& test_type = test_path->get_type();
			if (!(types::atom_type("boolean") == &test_type))
				throw typecheck_error{ std::string("Branch number does not have a boolean test") };

			set_type(types::unique_type(code_path->get_type().copy()));
		}

		core_ast::node* match_branch::lower()
		{
			throw lower_error{ "Cannot lower conditional branch path" };
		}

		// Condition Branch

		void match::typecheck(typecheck_environment& env)
		{
			types::type* common_type = new types::unset_type();

			for (uint32_t branch_count = 0; branch_count < branches.size(); branch_count++)
			{
				branches.at(branch_count).typecheck(env);

				// In first iteration
				if (types::unset_type() == common_type)
				{
					common_type = branches.at(branch_count).get_type().copy();
					continue;
				}

				// Check other elements are the same
				if (!(branches.at(branch_count).get_type() == common_type))
					throw typecheck_error{ std::string("Branch is of a different type than those before it") };
			}

			set_type(common_type);
		}

		core_ast::node* match::lower()
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
			node(new types::atom_type{ "void" }),
			id(std::move(*dynamic_cast<identifier*>(children.at(0).get()))),
			types(std::move(children.at(1))) {}

		void type_definition::typecheck(typecheck_environment& env)
		{
			types->typecheck(env);
			env.set_type(id, types::unique_type(types->get_type().copy()));
			id.set_type(types::unique_type(types->get_type().copy()));
			set_type(types::unique_type(types->get_type().copy()));
		}

		core_ast::node* type_definition::lower()
		{
			auto return_statement = core_ast::unique_node(new core_ast::identifier({}, { "_arg0" }, {}, types::make_unique(types::unset_type())));
			auto parameter_name = core_ast::identifier({}, { "_arg0" }, {}, types::make_unique(types::unset_type()));

			return new core_ast::set(
				std::move(*dynamic_cast<core_ast::identifier*>(id.lower())),

				core_ast::unique_node(new core_ast::function(
					std::optional<core_ast::identifier>(),
					parameter_name,
					std::move(return_statement),
					types::make_unique(types::function_type(
						types->get_type(),
						types->get_type()
					))
				)),

				types::make_unique(types::function_type(
					types->get_type(),
					types->get_type()
				))
			);
		}

		// Array Type

		array_type::array_type(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			child(std::move(children.at(0))) {}

		array_type::array_type(const array_type& other) : node(other), child(other.child->copy()) {}

		array_type::array_type(array_type&& other) : node(std::move(other)), child(std::move(other.child)) {}

		array_type& array_type::operator=(array_type&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
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

			set_type(types::make_unique(types::array_type(child->get_type())));
		}

		core_ast::node* array_type::lower()
		{
			throw lower_error{ "Cannot lower array type" };
		}

		// Reference

		reference::reference(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			child(std::move(children.at(0))) {}

		reference::reference(const reference& other) :
			node(other),
			child(other.child->copy()) {}

		reference::reference(reference && other) :
			node(std::move(other)),
			child(std::move(other.child)) {}

		reference& reference::operator=(reference && other)
		{
			set_type(types::unique_type(other.get_type().copy()));
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

			set_type(types::make_unique(types::reference_type(child->get_type())));
		}

		core_ast::node* reference::lower()
		{
			return new core_ast::reference(core_ast::unique_node(this->child->lower()));
		}

		// Array Value

		array_value::array_value(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			children(std::move(children)) {}

		array_value::array_value(const array_value& other) :
			node(other)
		{
			for (decltype(auto) child : other.children)
			{
				children.push_back(unique_node(child->copy()));
			}
		}

		array_value::array_value(array_value&& other) :
			node(std::move(other)),
			children(std::move(other.children)) {}

		array_value& array_value::operator=(array_value&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
			this->children = std::move(other.children);
			return *this;
		}

		node* array_value::copy() { return new array_value(*this); }

		void array_value::typecheck(typecheck_environment& env)
		{
			for (decltype(auto) child : children)
				child->typecheck(env);

			if (children.size() > 0)
			{
				auto element_type = children.at(0)->get_type().copy();

				for (decltype(auto) child : children)
				{
					if (!(child->get_type() == element_type))
						throw typecheck_error{ "All types in an array must be equal" };
				}

				set_type(types::make_unique(types::array_type(types::unique_type(element_type))));
			}
			else
				set_type(types::make_unique(types::array_type(types::atom_type{ "void" })));
		}

		core_ast::node* array_value::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			for (decltype(auto) child : children)
				lowered_children.push_back(core_ast::unique_node(child->lower()));

			return new core_ast::tuple(std::move(lowered_children), get_type());
		}

		// Equality

		equality::equality(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			left(std::move(children.at(0))),
			right(std::move(children.at(1))) {}

		equality::equality(const equality& other) :
			node(other),
			left(other.left->copy()),
			right(other.right->copy()) {}

		equality::equality(equality&& other) :
			node(std::move(other)),
			left(std::move(other.left)),
			right(std::move(other.right)) {}

		equality& equality::operator=(equality&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
			this->left = std::move(other.left);
			this->right = std::move(other.right);
			return *this;
		}

		node* equality::copy()
		{
			return new equality(*this);
		}

		void equality::typecheck(typecheck_environment& env)
		{
			left->typecheck(env);
			right->typecheck(env);

			if (!(types::atom_type{ "std.i32" } == &left->get_type()))
			{
				throw typecheck_error{ "Left side of equality must be a number" };
			}

			if (!(types::atom_type{ "std.i32" } == &right->get_type()))
			{
				throw typecheck_error{ "Right side of equality must be a number" };
			}

			set_type(types::make_unique(types::atom_type{ "boolean" }));
		}

		core_ast::node* equality::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			lowered_children.push_back(core_ast::unique_node(left->lower()));
			lowered_children.push_back(core_ast::unique_node(right->lower()));

			types::product_type p;
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });

			return new core_ast::function_call{
				core_ast::identifier{{}, "eq", {}, types::make_unique(types::unset_type())},
				std::make_unique<core_ast::tuple>(core_ast::tuple{
					std::move(lowered_children),
					types::make_unique(std::move(p))
				}),
				types::make_unique(types::atom_type{"boolean"})
			};
		}

		// Addition

		addition::addition(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			left(std::move(children.at(0))),
			right(std::move(children.at(1))) {}

		addition::addition(const addition& other) :
			node(other),
			left(other.left->copy()),
			right(other.right->copy()) {}

		addition::addition(addition&& other) :
			node(std::move(other)),
			left(std::move(other.left)),
			right(std::move(other.right)) {}

		addition& addition::operator=(addition&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
			this->left = std::move(other.left);
			this->right = std::move(other.right);
			return *this;
		}

		node* addition::copy()
		{
			return new addition(*this);
		}

		void addition::typecheck(typecheck_environment& env)
		{
			left->typecheck(env);
			right->typecheck(env);

			if (!(types::atom_type{ "std.i32" } == &left->get_type()))
			{
				throw typecheck_error{ "Left side of addition must be a number" };
			}

			if (!(types::atom_type{ "std.i32" } == &right->get_type()))
			{
				throw typecheck_error{ "Right side of addition must be a number" };
			}

			set_type(types::make_unique(types::atom_type{ "std.i32" }));
		}

		core_ast::node* addition::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			lowered_children.push_back(core_ast::unique_node(left->lower()));
			lowered_children.push_back(core_ast::unique_node(right->lower()));

			types::product_type p;
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });

			return new core_ast::function_call{
				core_ast::identifier{{}, "add", {}, types::make_unique(types::unset_type())},
				std::make_unique<core_ast::tuple>(core_ast::tuple{
					std::move(lowered_children),
					types::make_unique(std::move(p))
				}),
				types::make_unique(types::atom_type{"std.i32"})
			};
		}

		// Subtraction

		subtraction::subtraction(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			left(std::move(children.at(0))),
			right(std::move(children.at(1))) {}

		subtraction::subtraction(const subtraction& other) :
			node(other),
			left(other.left->copy()),
			right(other.right->copy()) {}

		subtraction::subtraction(subtraction&& other) :
			node(std::move(other)),
			left(std::move(other.left)),
			right(std::move(other.right)) {}

		subtraction& subtraction::operator=(subtraction&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
			this->left = std::move(other.left);
			this->right = std::move(other.right);
			return *this;
		}

		node* subtraction::copy()
		{
			return new subtraction(*this);
		}

		void subtraction::typecheck(typecheck_environment& env)
		{
			left->typecheck(env);
			right->typecheck(env);

			if (!(left->get_type() == &types::atom_type{ "std.i32" }))
			{
				throw typecheck_error{ "Left side of subtraction must be a number" };
			}

			if (!(right->get_type() == &types::atom_type{ "std.i32" }))
			{
				throw typecheck_error{ "Right side of subtraction must be a number" };
			}

			set_type(types::make_unique(types::atom_type{ "std.i32" }));
		}

		core_ast::node* subtraction::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			lowered_children.push_back(core_ast::unique_node(left->lower()));
			lowered_children.push_back(core_ast::unique_node(right->lower()));

			types::product_type p;
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });

			return new core_ast::function_call{
				core_ast::identifier{{}, "sub", {}, types::make_unique(types::unset_type())},
				std::make_unique<core_ast::tuple>(core_ast::tuple{
					std::move(lowered_children),
					types::make_unique(std::move(p))
				}),
				types::make_unique(types::atom_type{"std.i32"})
			};
		}

		// Multiplication

		multiplication::multiplication(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			left(std::move(children.at(0))),
			right(std::move(children.at(1))) {}

		multiplication::multiplication(const multiplication& other) :
			node(other),
			left(other.left->copy()),
			right(other.right->copy()) {}

		multiplication::multiplication(multiplication&& other) :
			node(std::move(other)),
			left(std::move(other.left)),
			right(std::move(other.right)) {}

		multiplication& multiplication::operator=(multiplication&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
			this->left = std::move(other.left);
			this->right = std::move(other.right);
			return *this;
		}

		node* multiplication::copy()
		{
			return new multiplication(*this);
		}

		void multiplication::typecheck(typecheck_environment& env)
		{
			left->typecheck(env);
			right->typecheck(env);

			if (!(left->get_type() == &types::atom_type{ "std.i32" }))
			{
				throw typecheck_error{ "Left side of multiplication must be a number" };
			}

			if (!(right->get_type() == &types::atom_type{ "std.i32" }))
			{
				throw typecheck_error{ "Right side of multiplication must be a number" };
			}

			set_type(types::make_unique(types::atom_type{ "std.i32" }));
		}

		core_ast::node* multiplication::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			lowered_children.push_back(core_ast::unique_node(left->lower()));
			lowered_children.push_back(core_ast::unique_node(right->lower()));

			types::product_type p;
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });

			return new core_ast::function_call{
				core_ast::identifier{{}, "mul", {}, types::make_unique(types::unset_type())},
				std::make_unique<core_ast::tuple>(core_ast::tuple{
					std::move(lowered_children),
					types::make_unique(std::move(p))
				}),
				types::make_unique(types::atom_type{"std.i32"})
			};
		}

		// Division

		division::division(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			left(std::move(children.at(0))),
			right(std::move(children.at(1))) {}

		division::division(const division& other) :
			node(other),
			left(other.left->copy()),
			right(other.right->copy()) {}

		division::division(division&& other) :
			node(std::move(other)),
			left(std::move(other.left)),
			right(std::move(other.right)) {}

		division& division::operator=(division&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
			this->left = std::move(other.left);
			this->right = std::move(other.right);
			return *this;
		}

		node* division::copy()
		{
			return new division(*this);
		}

		void division::typecheck(typecheck_environment& env)
		{
			left->typecheck(env);
			right->typecheck(env);

			if (!(left->get_type() == &types::atom_type{ "std.i32" }))
			{
				throw typecheck_error{ "Left side of division must be a number" };
			}

			if (!(right->get_type() == &types::atom_type{ "std.i32" }))
			{
				throw typecheck_error{ "Right side of division must be a number" };
			}

			set_type(types::make_unique(types::atom_type{ "std.i32" }));
		}

		core_ast::node* division::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			lowered_children.push_back(core_ast::unique_node(left->lower()));
			lowered_children.push_back(core_ast::unique_node(right->lower()));

			types::product_type p;
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });

			return new core_ast::function_call{
				core_ast::identifier{{}, "div", {}, types::make_unique(types::unset_type())},
				std::make_unique<core_ast::tuple>(core_ast::tuple{
					std::move(lowered_children),
					types::make_unique(std::move(p))
				}),
				types::make_unique(types::atom_type{"std.i32"})
			};
		}

		// Array Index

		array_index::array_index(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			array_exp(std::move(children.at(0))),
			index_exp(std::move(children.at(1))) {}

		array_index::array_index(const array_index& other) :
			node(other),
			array_exp(other.array_exp->copy()),
			index_exp(other.index_exp->copy()) {}

		array_index::array_index(array_index&& other) :
			node(std::move(other)),
			array_exp(std::move(other.array_exp)),
			index_exp(std::move(other.index_exp)) {}

		array_index& array_index::operator=(array_index&& other)
		{
			set_type(types::unique_type(other.get_type().copy()));
			this->array_exp = std::move(other.array_exp);
			this->index_exp = std::move(other.index_exp);
			return *this;
		}

		node* array_index::copy()
		{
			return new array_index(*this);
		}

		void array_index::typecheck(typecheck_environment& env)
		{
			array_exp->typecheck(env);
			index_exp->typecheck(env);

			if (auto type = dynamic_cast<types::array_type*>(&array_exp->get_type()))
			{
				set_type(type->element_type->copy());
			}
			else
			{
				throw typecheck_error{ "Array expression must be of type array" };
			}

			if (!(index_exp->get_type() == &types::atom_type{ "std.i32" }))
			{
				throw typecheck_error{ "Array index must be an integer" };
			}
		}

		core_ast::node* array_index::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			lowered_children.push_back(core_ast::unique_node(array_exp->lower()));
			lowered_children.push_back(core_ast::unique_node(index_exp->lower()));

			types::product_type p;
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });
			p.product.push_back({ "", types::make_unique(types::atom_type{"std.i32"}) });

			auto type = dynamic_cast<types::array_type*>(&array_exp->get_type());

			return new core_ast::function_call{
				core_ast::identifier{{}, "get", {}, types::make_unique(types::unset_type())},
				std::make_unique<core_ast::tuple>(core_ast::tuple{
					std::move(lowered_children),
					types::make_unique(std::move(p))
				}),
				types::unique_type(type->element_type->copy())
			};
		}
	}
}