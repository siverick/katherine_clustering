
/**
 * @MTQueue.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 */

#pragma once

#ifndef PLUGIN_MAIN_MTQUEUE_H_
#define PLUGIN_MAIN_MTQUEUE_H_
#include <queue>
#include <mutex>
#include <atomic>
#include <vector>
#include <list>

 /* Multi threaded queue with sleep while waiting for unlock */
template<typename T>
class MTQueue
{
private:
	std::queue<T> qu;
	std::mutex mtx;

public:

	void Emplace(T element)
	{
		std::lock_guard<std::mutex> lock(mtx);
		qu.push(std::move(element));
	}

	/* Get element and remove it from queue (Front & Pop) */
	T Pop()
	{
		std::lock_guard<std::mutex> lock(mtx);
		T ret = qu.front();
		qu.pop();

		return ret;
	}

	size_t Size()
	{
		return qu.size();
	}

	bool Empty()
	{
		return qu.empty();
	}
};

template<typename T>
class MTVector
{
private:
	std::vector<T> vec;
	std::mutex mtx;

public:

	void Emplace_Back(T&& element)
	{
		std::lock_guard<std::mutex> lock(mtx);
		vec.emplace_back(element);
	}

	void Push_Back(T& element)
	{
		std::lock_guard<std::mutex> lock(mtx);
		vec.emplace_back(element);
	}

	std::vector<T> Get_All()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return vec;
	}

	std::vector<T> Get_All_And_Erase()
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto temp = std::move(vec);
		//vec.erase(vec.begin(), vec.end());
		vec.clear();
		vec.shrink_to_fit();
		return temp;
	}

	void Erase(typename std::vector<T>::iterator first)
	{
		std::lock_guard<std::mutex> lock(mtx);
		vec.erase(first);
	}

	void Erase(typename std::vector<T>::iterator first, typename std::vector<T>::iterator last)
	{
		std::lock_guard<std::mutex> lock(mtx);
		vec.erase(first, last);
	}

	void ClearAndFit()
	{
		std::lock_guard<std::mutex> lock(mtx);
		vec.clear();
		vec.shrink_to_fit();
	}

	T& operator[](size_t index)
	{
		std::lock_guard<std::mutex> lock(mtx);
		return vec[index];
	}

	void Insert(std::vector<T>& toInsert)
	{
		std::lock_guard<std::mutex> lock(mtx);
		vec.insert(vec.end(), toInsert.begin(), toInsert.end());
	}

	size_t Size()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return vec.size();
	}

	typename std::vector<T>::iterator begin()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return vec.begin();
	}

	typename std::vector<T>::iterator end()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return vec.end();
	}
};

#endif /* PLUGIN_MAIN_MTQUEUE_H_ */
