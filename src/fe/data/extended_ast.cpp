#include "fe/data/extended_ast.h"
#include "fe/data/type_environment.h"
#include "fe/data/core_ast.h"

namespace fe
{
	namespace extended_ast
	{
		// Integer

		core_ast::node* integer::lower()
		{
			return new core_ast::integer(this->value);
		}

		// String

		string::string(const values::string val) : node(new types::unset_type()), value(val) {}

		core_ast::node* string::lower()
		{
			return new core_ast::string(this->value);
		}

		// Identifier

		core_ast::node* identifier::lower()
		{
			auto modules = std::vector<std::string>(segments.begin(), segments.end() - 1 - offsets.size());

			return new core_ast::identifier{
				std::move(modules),
				std::move(segments.at(modules.size())),
				std::move(offsets),
				this->scope_distance.has_value() ? this->scope_distance.value() : 0,
				types::unique_type(get_type().copy())
			};
		}

		// Export Statement

		core_ast::node* export_stmt::lower()
		{
			return new core_ast::no_op();
		}

		// Module Declaration


		core_ast::node* module_declaration::lower()
		{
			return new core_ast::no_op();
		}

		// Reference Type

		core_ast::node* reference_type::lower()
		{
			throw lower_error{ "Unimplemented" };
		}

		// Type Atom

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
				if (dynamic_cast<type_atom*>(child.get()) || dynamic_cast<function_type*>(child.get()) || dynamic_cast<type_tuple*>(child.get()))
					elements.push_back(std::move(child));
				else
					throw cst_to_ast_error{ "Type tuple can only contain atom and function declarations" };
			}
		}


		core_ast::node* type_tuple::lower()
		{
			throw lower_error{ "Cannot lower tupletype" };
		}

		// Function Type


		core_ast::node* function_type::lower()
		{
			throw lower_error{ "Cannot lower function type" };
		}

		// Atom Declaration

		atom_declaration::atom_declaration(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			type_expression(std::move(children.at(0))),
			name(*dynamic_cast<identifier*>(children.at(1).get())) {}

		core_ast::node* atom_declaration::lower()
		{
			throw lower_error{ "Cannot lower atom declaration" };
		}

		// Tuple Declaration

		tuple_declaration::tuple_declaration(std::vector<unique_node>&& children) : node(new types::unset_type())
		{
			// Children can be reference, atom_type, or function_declaration

			for (decltype(auto) child : children)
			{
				elements.push_back(std::move(child));
			}
		}


		core_ast::node* tuple_declaration::lower()
		{
			return new core_ast::no_op();
		}

		// Tuple

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
		{}

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

		// Declaration

		declaration::declaration(const declaration& other) : node(other), lhs(other.lhs), type_name(other.type_name), value(other.value->copy()) {}

		core_ast::node* declaration::lower()
		{
			const auto value = this->value->lower();

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
					std::move(std::get<core_ast::identifier>(res)), true,
					core_ast::unique_node(value),
					types::unique_type(this->value->get_type().copy())
				);
			}
			else if (std::holds_alternative<core_ast::identifier_tuple>(res))
			{
				return new core_ast::set(
					std::move(std::get<core_ast::identifier_tuple>(res)),
					core_ast::unique_node(value),
					types::unique_type(this->value->get_type().copy())
				);
			}

			assert(!"The lhs of an assignment must become either an identifier or an identifier tuple");
		}

		// Assignment

		core_ast::node* assignment::lower()
		{
			return new core_ast::set(
				*dynamic_cast<core_ast::identifier*>(this->lhs.lower()), false,
				core_ast::unique_node(this->value->lower()),
				types::unique_type(this->value->get_type().copy())
			);
		}

		assignment::assignment(const assignment& other) : node(other), lhs(other.lhs), value(other.value->copy()) {}

		// Function

		function::function(const function& other) : node(other), name(other.name), from(other.from->copy()), 
			to(other.to->copy()), body(other.body->copy()) {}
		function::function(std::vector<unique_node>&& children) :
			node(new types::unset_type()),
			name(std::move(*dynamic_cast<identifier*>(children.at(0).get()))),
			from(std::move(children.at(1))),
			to(std::move(children.at(2))),
			body(std::move(children.at(3))) {}


		core_ast::node* function::lower()
		{
			auto lowered_name = dynamic_cast<core_ast::identifier*>(this->name.lower());

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
				std::move(*lowered_name),
				std::move(parameters),
				core_ast::unique_node(std::move(lowered_body)),
				types::unique_type(get_type().copy())
			};
		}

		// Conditional Branch Path

		match_branch::match_branch(const match_branch& other) : node(other), test_path(other.test_path->copy()), code_path(other.code_path->copy()) {}

		core_ast::node* match_branch::lower()
		{
			throw lower_error{ "Cannot lower conditional branch path" };
		}

		// Condition Branch

		core_ast::node* match::lower()
		{
			std::vector<std::pair<core_ast::unique_node, core_ast::unique_node>> new_branches;

			for (auto it = branches.begin(); it != branches.end(); ++it)
			{
				new_branches.emplace_back(
					core_ast::unique_node(it->test_path->lower()),
					core_ast::unique_node(it->code_path->lower())
				);
			}

			return new core_ast::branch(std::move(new_branches));
		}

		// Type Definition

		type_definition::type_definition(std::vector<unique_node>&& children) :
			node(new types::atom_type{ "void" }),
			id(std::move(*dynamic_cast<identifier*>(children.at(0).get()))),
			types(std::move(children.at(1))) {}


		core_ast::node* type_definition::lower()
		{
			auto return_statement = core_ast::unique_node(new core_ast::identifier({}, { "_arg0" }, {}, 0,
				types::make_unique(types::unset_type())));
			auto parameter_name = core_ast::identifier({}, { "_arg0" }, {}, 0, types::make_unique(types::unset_type()));

			auto new_id = std::unique_ptr<core_ast::identifier>(static_cast<core_ast::identifier*>(this->id.lower()));

			return new core_ast::set(
				*new_id, true,
				core_ast::unique_node(new core_ast::function(
					core_ast::identifier(*new_id),
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

		core_ast::node* array_value::lower()
		{
			std::vector<core_ast::unique_node> lowered_children;
			for (decltype(auto) child : children)
				lowered_children.push_back(core_ast::unique_node(child->lower()));

			return new core_ast::tuple(std::move(lowered_children), get_type());
		}
	}
}