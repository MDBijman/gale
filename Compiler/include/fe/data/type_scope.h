#pragma once
#include <unordered_map>
#include <string>
#include <functional>

#include "fe/data/types.h"
#include "fe/data/ast_data.h"
#include "fe/pipeline/error.h"

namespace fe::ext_ast
{
	struct conversion_constraint
	{
		types::type& to;

		conversion_constraint(types::type& t) : to(t) {}

		bool satisfied_by(types::type& t)
		{
			if (to == &t) return true;

			// Number literals
			{
				if (auto lhs = dynamic_cast<types::i64*>(&to); lhs)
				{
					if (dynamic_cast<types::i32*>(&t) != nullptr) { return true; }
					if (dynamic_cast<types::ui32*>(&t) != nullptr) { return true; }
				}
				if (auto lhs = dynamic_cast<types::ui64*>(&to); lhs)
				{
					if (dynamic_cast<types::ui32*>(&t) != nullptr) { return true; }
				}
			}

			// Sum type
			{
				if (auto lhs = dynamic_cast<types::sum_type*>(&t))
				{
					for (auto& lhs_elem : lhs->sum)
					{
						if (conversion_constraint(*lhs_elem).satisfied_by(t)) return true;
					}
				}
			}

			return false;
		}

		std::optional<conversion_constraint> tuple_sub_constraint(size_t i)
		{
			if (auto product = dynamic_cast<types::product_type*>(&to))
			{
				return conversion_constraint(*product->product.at(i));
			}

			return std::nullopt;
		}

		std::optional<conversion_constraint> array_sub_constraint()
		{
			if (auto arr = dynamic_cast<types::array_type*>(&to))
			{
				return conversion_constraint(*arr->element_type);
			}

			return std::nullopt;
		}

		operator std::string() const
		{
			return "conversion_constraint (" + to.operator std::string() + ")";
		}
	};

	struct equality_constraint
	{
		types::type& to;
		equality_constraint(types::type& t) : to(t) {}

		bool satisfied_by(types::type& t)
		{
			// #todo add more cases? constraints must be redone anyway
			if (dynamic_cast<types::atom<types::atom_type::ANY>*>(&to))
				return true;

			return (to == &t);
		}

		std::optional<equality_constraint> tuple_sub_constraint(size_t i)
		{
			if (auto product = dynamic_cast<types::product_type*>(&to))
			{
				return equality_constraint(*product->product.at(i));
			}

			return std::nullopt;
		}

		std::optional<equality_constraint> array_sub_constraint()
		{
			if (auto arr = dynamic_cast<types::array_type*>(&to))
			{
				return equality_constraint(*arr->element_type);
			}

			return std::nullopt;
		}

		operator std::string() const
		{
			return "equality_constraint (" + to.operator std::string() + ")";
		}
	};

	using type_constraint = std::variant<conversion_constraint, equality_constraint>;

	const auto to_string = [](const auto& x) -> std::string { return x.operator std::string(); };

	class type_constraints
	{
	public:
		std::vector<type_constraint> constraints;

		type_constraints() {}
		type_constraints(std::vector<type_constraint> tc) : constraints(std::move(tc)) {}

		bool satisfied_by(types::type& t)
		{
			for (auto& constraint : constraints)
			{
				auto satisfied = std::visit(([&t](auto& x) -> bool { return x.satisfied_by(t); }), constraint);
				if (!satisfied) return false;
			}

			return true;
		}

		std::optional<type_constraints> tuple_sub_constraints(size_t i)
		{
			std::vector<type_constraint> sub_constraints;

			for (auto& constraint : constraints)
			{
				auto sub = std::visit(([i](auto& x) -> std::optional<type_constraint> { 
					auto sub = x.tuple_sub_constraint(i);
					if (!sub) return std::nullopt;
					else return type_constraint(*sub); 
				}), constraint);
				if(sub) sub_constraints.push_back(std::move(*sub));
				else return std::nullopt;
			}

			return type_constraints(std::move(sub_constraints));
		}

		std::optional<type_constraints> array_sub_constraints()
		{
			std::vector<type_constraint> sub_constraints;

			for (auto& constraint : constraints)
			{
				auto sub = std::visit(([](auto& x) -> std::optional<type_constraint> { 
					auto sub = x.array_sub_constraint();
					if (!sub) return std::nullopt;
					else return type_constraint(*sub); 
				}), constraint);
				if(sub) sub_constraints.push_back(std::move(*sub));
				else return std::nullopt;
			}

			return type_constraints(std::move(sub_constraints));
		}

		operator std::string() const
		{
			std::string o = "type_constraints (";
			for (auto& constraint : constraints)
			{
				o += std::visit(to_string, constraint);
			}
			return o + ")";
		}
	};
}

namespace fe::ext_ast
{
	class type_scope
	{
		struct var_lookup
		{
			long long scope_distance;
			types::type& type;
		};

		struct type_lookup
		{
			long long scope_distance;
			types::type& type;
		};

		// The types of variables
		std::unordered_map<name, types::unique_type> variables;

		// The defined types
		std::unordered_map<name, types::unique_type> types;

		std::unordered_map<module_name, scope_index> modules;

		// Parent scope
		std::optional<scope_index> parent;

		types::type& resolve_offsets(const std::vector<size_t>& offsets, types::type* t, size_t cur = 0);

	public:
		using get_scope_cb = std::function<type_scope*(scope_index)>;

		type_scope() {}
		type_scope(scope_index p) : parent(p) {}
		type_scope(const type_scope& o)
		{
			for (const auto& variable : o.variables)
			{
				variables.insert({ variable.first, types::unique_type(variable.second->copy()) });
			}

			for (const auto& type : o.types)
			{
				types.insert({ type.first, types::unique_type(type.second->copy()) });
			}

			for (const auto& mod : o.modules)
			{
				modules.insert({ mod.first, mod.second });
			}

			parent = o.parent;
		}
		type_scope(type_scope&& o) : variables(std::move(o.variables)), types(std::move(o.types)) {}

		void clear();

		void add_module(module_name module_name, scope_index scope);

		void set_parent(scope_index other);

		void merge(type_scope other);

		// Types of variables

		void set_type(name n, types::unique_type t);
		std::optional<var_lookup> resolve_variable(const identifier& n, get_scope_cb);

		// Defined types

		void define_type(name n, types::unique_type t);
		std::optional<type_lookup> resolve_type(const identifier& n, get_scope_cb);
	};
}