#pragma once
#include <array>

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
		std::vector<bool> occupieds;
	public:
		using index = size_t;

		index create()
		{
			auto free_pos = std::find(occupieds.begin(), occupieds.end(), false);
			if (free_pos == occupieds.end())
			{
				occupieds.push_back(true);
				data.push_back(T());
				return occupieds.size() - 1;
			}
			else
			{
				*free_pos = true;
				return std::distance(occupieds.begin(), free_pos);
			}
		}

		T& get_at(index i)
		{
			assert(i < occupieds.size());
			assert(occupieds[i]);
			assert(data.size() == occupieds.size());
			return data.at(i);
		}

		bool is_occupied(index i)
		{
			assert(i < occupieds.size());
			assert(data.size() == occupieds.size());
			return occupieds[i];
		}

		void free_at(index i)
		{
			assert(i < occupieds.size());
			assert(data.size() == occupieds.size());
			occupieds[i] = false;
		}

		const std::vector<T>& get_data()
		{
			return data;
		}
	};
}