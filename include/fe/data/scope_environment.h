#pragma once
#include "fe/pipeline/error.h"
#include "fe/data/extended_ast.h"

#include <vector>
#include <unordered_map>

namespace fe
{
	namespace detail
	{
		template <class T>
		static void hash_combine(std::size_t & s, const T & v)
		{
			std::hash<T> h;
			s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
		}

		template<class T> struct id_hasher;

		template<>
		struct id_hasher<extended_ast::identifier>
		{
			std::size_t operator()(const extended_ast::identifier& id) const
			{
				std::size_t res = 0;
				for (auto& r : id.segments) hash_combine<std::string>(res, r);
				for (auto& r : id.offsets) hash_combine<int>(res, r);
				hash_combine<std::size_t>(res, id.scope_distance);
				return res;
			}
		};

		class scope
		{
		public:
			bool resolvable(const extended_ast::identifier& id) const
			{
				const auto loc = identifiers.find(id);

				if (loc == identifiers.end())
					return false;

				if (loc->second == false)
					throw resolution_error{ "Variable referenced in its own definition" };

				return true;
			}

			void declare(const extended_ast::identifier& id)
			{
				identifiers.insert({ id, false });
			}

			void define(const extended_ast::identifier& id)
			{
				identifiers.at(id) = true;
			}

		private:
			std::unordered_map<extended_ast::identifier, bool, id_hasher<extended_ast::identifier>> identifiers;
		};
	}

	class scope_environment
	{
	public:
		scope_environment() { push(); }

		void push()
		{
			scopes.push_back(detail::scope{});
		}

		void pop()
		{
			scopes.pop_back();
		}

		std::size_t resolve(const extended_ast::identifier& id) const
		{
			for (auto it = scopes.rbegin(); it != scopes.rend(); ++it)
			{
				if (it->resolvable(id))
				{
					return std::distance(scopes.rbegin(), it);
				}
			}

			throw resolution_error{ "Cannot resolve identifier" };
		}

		void declare(const extended_ast::identifier_tuple& id)
		{
			for(auto& child : id.content)
				std::visit([this](auto& c) { this->declare(c); }, child);
		}

		void declare(const extended_ast::identifier& id)
		{
			scopes.rbegin()->declare(id);
		}

		void define(const extended_ast::identifier_tuple& id)
		{
			for(auto& child : id.content)
				std::visit([this](auto& c) { this->define(c); }, child);
		}

		void define(const extended_ast::identifier& id)
		{
			scopes.rbegin()->define(id);
		}

	private:
		std::vector<detail::scope> scopes;
	};
}
