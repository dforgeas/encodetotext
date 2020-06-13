#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>
#include <system_error>

#include <semaphore.h>

// while waiting for std::semaphore
class Semaphore
{
public:
	Semaphore(unsigned value);
	~Semaphore();
	Semaphore(Semaphore const&) = delete;
	Semaphore(Semaphore &&) = delete;

	void acquire();
	void release();
private:
	sem_t sem;
};

inline Semaphore::Semaphore(unsigned value)
{
	constexpr int thread_only = 0;
	if (sem_init(&sem, thread_only, value) == -1)
		throw std::system_error(std::error_code(errno, std::generic_category()), "sem_init");
}

inline Semaphore::~Semaphore()
{
	sem_destroy(&sem);
}

inline void Semaphore::acquire()
{
	int result;
	while ((result = sem_wait(&sem)) == -1 and errno == EINTR)
		continue; // retry if interupted
	if (result == -1)
		throw std::system_error(std::error_code(errno, std::generic_category()), "sem_wait");
}

inline void Semaphore::release()
{
	if (sem_post(&sem) == -1)
		throw std::system_error(std::error_code(errno, std::generic_category()), "sem_post");
}

template <typename T>
class Queue
{
public:
	Queue(unsigned maximum_size);
	void push(T &&t);
	T pop();

private:
	std::mutex mutex;
	std::condition_variable filled;
	std::deque<T> deck;
	Semaphore available;
};

template <typename T>
inline Queue<T>::Queue(unsigned maximum_size)
	: available(maximum_size)
{
}

template <typename T>
inline void Queue<T>::push(T &&t)
{
	available.acquire(); // block while the queue is full
	{
		std::lock_guard<std::mutex> lock(mutex);
		deck.push_back(std::move(t));
	}
	filled.notify_one();
}

template <typename T>
inline T Queue<T>::pop()
{
	std::unique_lock<std::mutex> lock(mutex);
	filled.wait(lock, [this]() { return not deck.empty(); });
	T result(std::move(deck.front()));
	deck.pop_front();
	lock.unlock(); // avoid waking up push if the lock is taken
	available.release(); // notify a slot in the queue is now available
	return result;
}
