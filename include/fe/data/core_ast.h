#pragma once
#include "fe/data/types.h"
#include "fe/data/values.h"

#include <vector>
#include <optional>
#include <variant>

namespace fe
{
	class runtime_environment;

	namespace core_ast
	{
		struct node
		{
			virtual ~node() {};
			virtual node* copy() = 0;
			virtual values::unique_value interp(runtime_environment&) = 0;
		};

		using unique_node = std::unique_ptr<node>;

		// Value nodes
		struct no_op : public node
		{
			no_op();

			// Copy
			no_op(const no_op& other);
			no_op& operator=(const no_op& other);

			// Move
			no_op(no_op&& other);
			no_op& operator=(no_op&& other);

			node* copy()
			{
				return new no_op(*this);
			}
			values::unique_value interp(runtime_environment&) override;
		};

		struct literal : public node
		{
			literal(values::unique_value val) : val(std::move(val)) {}

			literal(const literal& other) : val(other.val->copy()) {}
			literal& operator=(const literal& o)
			{
				this->val = values::unique_value(o.val->copy());
				return *this;
			}

			literal(literal&& other) noexcept : val(std::move(other.val)) {}
			literal& operator=(literal&& other)
			{
				val = std::move(other.val);
				return *this;
			}

			node* copy() override
			{
				return new literal(*this);
			}

			values::unique_value interp(runtime_environment&) override
			{
				return values::unique_value(val->copy());
			}

			values::unique_value val;
		};

		struct identifier : public node
		{
			identifier(std::vector<std::string> module_names, std::string variables, std::vector<size_t> offset,
				size_t depth = std::numeric_limits<size_t>::max());

			// Copy
			identifier(const identifier& other);
			identifier& operator=(const identifier& other);

			// Move
			identifier(identifier&& other);
			identifier& operator=(identifier&& other);

			node* copy()
			{
				return new identifier(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			identifier without_first_module() const
			{
				return
					identifier(
						std::vector<std::string>{modules.begin() + 1, modules.end()},
						std::string(variable_name),
						std::vector<size_t>(offsets),
						scope_depth
					);
			}

			std::vector<std::string> modules;
			std::string variable_name;
			std::vector<size_t> offsets;
			size_t scope_depth;
		};

		struct identifier_tuple
		{
			std::vector<std::variant<identifier, identifier_tuple>> ids;
		};

		struct set : public node
		{
			set(identifier id, bool is_dec, unique_node value);
			set(identifier_tuple lhs, unique_node value);

			// Copy
			set(const set& other);
			set& operator=(const set& other);

			// Move
			set(set&& other);
			set& operator=(set&& other);

			node* copy()
			{
				return new set(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			std::variant<identifier, identifier_tuple> lhs;
			bool is_declaration;
			unique_node value;
		};

		struct function : public node
		{
			function(identifier&& name, std::variant<std::vector<identifier>, identifier>&& parameters, unique_node&& body);

			// Copy
			function(const function& other);
			function& operator=(const function& other);

			// Move
			function(function&& other);
			function& operator=(function&& other);

			node* copy()
			{
				return new function(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			identifier name;
			// Either a named tuple or a single argument
			std::variant<std::vector<identifier>, identifier> parameters;
			unique_node body;
		};

		// Derivatives


		struct tuple : public node
		{
			tuple(std::vector<unique_node> children);

			// Copy
			tuple(const tuple& other);
			tuple& operator=(const tuple& other);

			// Move
			tuple(tuple&& other);
			tuple& operator=(tuple&& other);

			node* copy()
			{
				return new tuple(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			std::vector<unique_node> children;
		};


		struct block : public node
		{
			block(std::vector<unique_node> children);

			// Copy
			block(const block& other);
			block& operator=(const block& other);

			// Move
			block(block&& other);
			block& operator=(block&& other);

			node* copy()
			{
				return new block(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			std::vector<unique_node> children;
		};

		struct function_call : public node
		{
			function_call(identifier id, unique_node parameter);

			// Copy
			function_call(const function_call& other);
			function_call& operator=(const function_call& other);

			// Move
			function_call(function_call&& other);
			function_call& operator=(function_call&& other);

			node* copy()
			{
				return new function_call(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			identifier id;
			unique_node parameter;
		};

		struct branch : public node
		{
			branch(std::vector<std::pair<unique_node, unique_node>> paths);

			// Copy
			branch(const branch& other);
			branch& operator=(const branch& other);

			// Move
			branch(branch&& other);
			branch& operator=(branch&& other);

			node* copy() override
			{
				return new branch(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			std::vector<std::pair<unique_node, unique_node>> paths;
		};

		struct reference : public node
		{
			reference(unique_node exp);

			reference(const reference& other);
			reference& operator=(const reference& other);

			reference(reference&& other);
			reference& operator=(reference&& other);

			node* copy() override
			{
				return new reference(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			unique_node exp;
		};

		struct while_loop : public node
		{
			while_loop(unique_node test_code, unique_node body);

			// Copy
			while_loop(const while_loop& other);
			while_loop& operator=(const while_loop& other);

			// Move
			while_loop(while_loop&& other);
			while_loop& operator=(while_loop&& other);

			node* copy() override
			{
				return new while_loop(*this);
			}
			values::unique_value interp(runtime_environment&) override;

			unique_node test;
			unique_node body;
		};
	}
}
