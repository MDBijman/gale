#pragma once
#include <array>

namespace memory
{
	template<class T, size_t SIZE>
	class data_store
	{
		std::array<T, SIZE> data;
		std::array<bool, SIZE> occupieds;
	public:
		using index = size_t;

		data_store() : occupieds({ false }) {}

		index create()
		{
			auto free_pos = std::find(occupieds.begin(), occupieds.end(), false);
			*free_pos = true;
			return std::distance(occupieds.begin(), free_pos);
		}

		T& get_at(index i)
		{
			assert(occupieds[i]);
			return data.at(i);
		}

		bool is_occupied(index i)
		{
			return occupieds[i];
		}

		void free_at(index i)
		{
			occupieds[i] = false;
		}

		const std::array<T, SIZE>& get_data()
		{
			return data;
		}
	};
}