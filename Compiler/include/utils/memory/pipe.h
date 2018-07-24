#pragma once
#include <mutex>
#include <optional>
#include <queue>
#include <array>
#include <thread>

namespace memory
{
	template<class T>
	class pipe
	{
		T t;
		bool full = false;
		std::mutex lock;
		std::condition_variable cv;

	public:
		void send(T t)
		{
			wait_on_receive();
			{
				std::scoped_lock<std::mutex> sl(lock);
				this->t = t;
				full = true;
			}
			cv.notify_one();
		}

		T receive()
		{
			wait_on_send();
			T temp;
			{
				std::scoped_lock<std::mutex> sl(lock);
				temp = std::move(t);
				full = false;
			}
			cv.notify_one();
			return temp;
		}

		void wait_on_receive()
		{
			std::unique_lock<std::mutex> ul(lock);
			if (!full) return;
			cv.wait(ul, [this] { return !full; });
		}

		void wait_on_send()
		{
			std::unique_lock<std::mutex> ul(lock);
			if (full) return;
			cv.wait(ul, [this] { return full; });
		}
	};
}
