/**
 * @MTQueue.h
 * @author Richard Sivera (richsivera@gmail.com)
 * @copyright Richard Sivera (c) 2024
 *
 * Disclaimer: Created based on diploma thesis of Lukas Meduna (meduna@medunalukas.com)
 */



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
};

template<typename T>
class MTVariable
{
private:
	T var;
	std::mutex mtx;

public:

	void Set_Value(T val)
	{
		std::lock_guard<std::mutex> lock(mtx);
		var = val;
	}

	T Get_Value()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return var;
	}

	void Add_To_Value(T val)
	{
		std::lock_guard<std::mutex> lock(mtx);
		var += val;
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

	std::vector<T> Get_All_And_Erase()
	{
		std::lock_guard<std::mutex> lock(mtx);
		auto temp = std::move(vec);
		vec.clear();
		return temp;
	}

	std::vector<T> Get_All()
	{
		std::lock_guard<std::mutex> lock(mtx);
		return vec;
	}

	void EraseAll()
	{
		std::lock_guard<std::mutex> lock(mtx);
		vec.clear();
	}

	void ShrinkToFit()
	{
		std::lock_guard<std::mutex> lock(mtx);
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
};

/* Spin Lock can be used for locking busy waiting, not wasting time
 * puttin thread to sleep before again checking if locked */
class SpinLock {
  std::atomic_flag locked = ATOMIC_FLAG_INIT;

public:
  void lock() {
    while (locked.test_and_set(std::memory_order_acquire)) {
      ;
    }
  }
  void unlock() { locked.clear(std::memory_order_release); }
};

/* MTqueue with spinlock semantics -> busy waiting for unlock */
template <typename T>
class MTqueue_spinlocked {
public:
  MTqueue_spinlocked(){};
  ~MTqueue_spinlocked(){};
  void Emplace(T &&item) {
    lock_.lock();
    qu.push(std::forward<T &&>(item));
    lock_.unlock();
  }
  T Pop() {
    lock_.lock();
    T obj(std::move(qu.front()));
    qu.pop();
    lock_.unlock();
    return obj;
  }
  size_t Size() { return qu.size(); }

private:
  std::queue<T> qu;
  SpinLock lock_;
};

/* Multi threaded queue with in and out buffer - not much waiting for unlock */
/* ONLY one writer THREAD SAFE */
template <typename T, size_t buffer_size> class MTQueueBuffered {
public:
  MTQueueBuffered(){};
  ~MTQueueBuffered(){};
  size_t counter = 0;
    void Emplace_Back(T &&item) {
    	/*
      lock_out_.lock();
	  listOut_.emplace_back(item);
	  lock_out_.unlock();
	  */

      // If lock successful, pushback into listIn_ and push the remaining elements from inputBuf_
      // Else, emplace into temporary buffer
      // Note: This is to avoid waiting for lock() - avoids collision with Flush() from other thread
      //lock_in_.lock();
      listIn_.emplace_back(std::forward<T &&>(item));
	  //lock_in_.unlock();
      counter++;

      // Efectively only emplaces into listOut when both locks are "lucky" to lock - doesnt guarantee immediate emplacement,
      // but avoids waiting for locking
      // NOTE: No guarantee how long i will take to emplace
      if (counter > buffer_size) {
		// Commit to locking now!
        lock_out_.lock();
		//lock_in_.lock();
		listOut_.splice(listOut_.end(), listIn_);
		counter = 0;
		//lock_in_.unlock();
		lock_out_.unlock();
      }
    }
    T Pop() {
      lock_out_.lock();
      auto obj(listOut_.front());
      listOut_.pop_front();
      lock_out_.unlock();
      return obj;
    }
    void Flush() {
    	lock_out_.lock();
		//lock_in_.lock();
		listOut_.splice(listOut_.end(), listIn_);
		counter = 0;
		//lock_in_.unlock();
		lock_out_.unlock();
    }
    void ClearOut() {
      lock_out_.lock();
      //lock_in_.lock();
      //listIn_.clear();
      listOut_.clear();
      lock_out_.unlock();
      //lock_in_.unlock();
    }
    void ClearIn()
    {
    	listIn_.clear();
    }
    bool isEmpty()
    {
    	/*lock_out_.lock();
    	bool empty = listOut_.empty();
    	lock_out_.unlock();
    	return empty;*/
    	return listOut_.empty();
    }
    size_t sizeOut()
    {
    	lock_out_.lock();
		size_t size = listOut_.size();
		lock_out_.unlock();
    	return size;
    }

private:
  std::list<T> listOut_;
  std::list<T> listIn_;
  SpinLock lock_out_;
  SpinLock lock_in_;
};

#endif /* PLUGIN_MAIN_MTQUEUE_H_ */
