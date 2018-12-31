#pragma once
#include <array>
#include <set>

namespace memory
{
	template<class T, size_t SIZE>
	class static_store
	{
		std::array<T, SIZE> data;
		std::array<bool, SIZE> occupieds;
	public:
		using index = size_t;

		static_store() : occupieds({ false }) {}

		index create()
		{
			auto free_pos = std::find(occupieds.begin(), occupieds.end(), false);
			*free_pos = true;
			return std::distance(occupieds.begin(), free_pos);
		}

		T& get_at(index i)
		{
			assert(i < SIZE);
			assert(occupieds[i]);
			return data.at(i);
		}

		bool is_occupied(index i)
		{
			assert(i < SIZE);
			return occupieds[i];
		}

		void free_at(index i)
		{
			assert(i < SIZE);
			occupieds[i] = false;
		}

		const std::array<T, SIZE>& get_data()
		{
			return data;
		}
	};

	template<class T>
	class dynamic_store
	{
		std::vector<T> data;
		std::vector<uint8_t> occupieds;
		bool is_full = true;
	public:
		using index = size_t;

		void reserve(size_t count)
		{
			data.reserve(count);
			occupieds.reserve(count);
		}

		size_t size()
		{
			return data.size();
		}

		template<typename... Ts> index create(Ts... args)
		{
			if (is_full)
			{
				data.push_back(T(std::forward<Ts>(args)...));
				occupieds.push_back(1);
				return data.size() - 1;
			}
			else
			{
				auto it = std::find(occupieds.begin(), occupieds.end(), 0);
				*it = 1;
				return std::distance(occupieds.begin(), it);
			}
		}

		T& get_at(index i)
		{
#ifdef _DEBUG
			assert(occupieds.size() == data.size());
			assert(i < data.size());
			assert(occupieds[i] == 1);
#endif
			return data[i];
		}

		bool is_occupied(index i)
		{
#ifdef _DEBUG
			assert(occupieds.size() == data.size());
			assert(i < data.size());
#endif
			return occupieds[i] == 1 ? true : false;
		}

		void free_at(index i)
		{
#ifdef _DEBUG
			assert(occupieds.size() == data.size());
			assert(i < data.size());
#endif
			occupieds[i] = 0;
			is_full = false;
		}

		const std::vector<T>& get_data()
		{
			return data;
		}
	};
}