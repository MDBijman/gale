#pragma once
#include <unordered_map>
#include <string>
#include <memory>

namespace language
{
	namespace detail
	{
		class storage
		{
		public:
			const std::string& get(const std::string& key) const
			{
				return map.at(key);
			}

			void set(const std::string& key, const std::string& value)
			{
				map.insert_or_assign(key, value);
			}

		private:
			std::unordered_map<std::string, std::string> map;
		};
	}

	class registry
	{
	public:
		registry() : prefix(""), storage() {}

		registry(const std::string& domain, std::shared_ptr<detail::storage> storage) : prefix(domain), storage(storage) {}

		const std::string& get(const std::string& key) const
		{
			return storage->get(prefix + key);
		}

		void set(const std::string& key, const std::string& value)
		{
			storage->set(prefix + key, value);
		}

		registry get_subregistry(const std::string& prefix) const
		{
			return registry(std::string(prefix).append(".").append(prefix), storage);
		}

	private:
		std::string prefix;
		std::shared_ptr<detail::storage> storage;
	};
}