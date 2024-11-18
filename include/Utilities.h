#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#ifndef _WIN32
#define __forceinline inline
#endif

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

/**
* A simple class representing an allocated RAM memory block.
* You may ask, why not use the standard library, and things such as vector ?
* You can definetely do that, it is just personal preference.
* there may be multiple properties to a Heap Memeory:
* - T : its type
* - scoped : should the object destroy the data on destruction ?
* - single_data : is it a single object, or an array of objects ?
* 
* The main objective of this class is that every single allocated memory of the application should be contiguous memeory (for performance reasons).
* This is problematic for memory optimization, but for a simple application such as this one, I think it is fine.
*/
template<typename T, bool scoped, bool single_data>
class HeapMemory
{
protected:
	T* _raw_data{nullptr};

public:

	/*===== Constructor =====*/

	HeapMemory() = default;

	HeapMemory(T* raw_data) :
		_raw_data{ raw_data }
	{

	}

	HeapMemory(uint32_t nb) :
		_raw_data{new T[nb]}
	{
	}

	~HeapMemory()
	{
		if (_raw_data && scoped)
		{
			if (single_data)
				delete _raw_data;
			else
				delete[] _raw_data;
		}
	}

	/*===== Accessor =====*/

	__forceinline T& operator[](int32_t i)const
	{
		return _raw_data[i];
	}

	__forceinline T* operator*()const noexcept
	{
		return _raw_data;
	}

	/*===== Assignement =====*/

	HeapMemory& operator=(T* raw_data)
	{
		_raw_data = raw_data;
		return *this;
	}

	HeapMemory& operator=(const HeapMemory& memory)
	{
		_raw_data = memory._raw_data;
		return *this;
	}

	HeapMemory& operator=(HeapMemory&& memory)
	{
		_raw_data = std::move(memory._raw_data);
		return *this;
	}

	/*===== Comparison =====*/

	__forceinline bool operator== (T* to_compare)const
	{
		return _raw_data == to_compare;
	}

	__forceinline bool operator!= (T* to_compare)const
	{
		return _raw_data != to_compare;
	}

	__forceinline bool operator== (const HeapMemory& other_memory)const
	{
		return _raw_data == other_memory._raw_data;
	}

	__forceinline bool operator!= (const HeapMemory& other_memory)const
	{
		return _raw_data != other_memory._raw_data;
	}

	/*===== Memory Management =====*/

	virtual void Clear()
	{
		if (_raw_data)
		{
			if (single_data)
				delete _raw_data;
			else
				delete[] _raw_data;
		}
		_raw_data = nullptr;
	}

	virtual void Alloc(uint32_t nb)
	{
		if (_raw_data && scoped)
			Clear();

		_raw_data = new T[nb];
	}

};

template<typename T>
using SingleVolatileMemory = HeapMemory<T, false, true>;
template<typename T>
using SingleScopedMemory = HeapMemory<T, true, true>;
template<typename T>
using MultipleVolatileMemory = HeapMemory<T, false, false>;
template<typename T>
using MultipleScopedMemory = HeapMemory<T, true, false>;

/**
* A simple class representing an allocated RAM memory block, in C format.
* This one is for sharing between thread. therefore thread safe.
*/
template<typename T, bool single_data>
class SharedHeapMemory : public HeapMemory<T,true,single_data>
{
protected:
	std::atomic_uint32_t* _shared{nullptr};

public:
    
    using HeapMemory<T,true,single_data>::_raw_data;

	/*===== Constructor =====*/

	SharedHeapMemory() = default;

	SharedHeapMemory(T* raw_data) :
		HeapMemory<T,true,single_data>(raw_data),
		_shared{ raw_data == nullptr ? nullptr : new std::atomic_uint32_t {1} }
	{

	}

	SharedHeapMemory(const SharedHeapMemory<T, single_data>& memory) :
        HeapMemory<T,true, single_data>(memory._raw_data),
		_shared{ memory._shared}
	{
		if (_shared)
			_shared->fetch_add(1);
	}

	SharedHeapMemory(uint32_t nb) :
        HeapMemory<T,true,single_data>(nb),
		_shared{ new std::atomic_uint32_t {1} }
	{
	}

	~SharedHeapMemory()
	{
		if (_shared)
		{
			_shared->fetch_sub(1);
			if (_raw_data && _shared->load() == 0)
			{
				if (single_data)
					delete _raw_data;
				else
					delete[] _raw_data;
				delete _shared;
			}
			_raw_data = nullptr;
			_shared = nullptr;
		}
	}

	/*===== Copy =====*/

	SharedHeapMemory& operator=(const SharedHeapMemory& copy)
	{
		if (_shared)
			_shared->fetch_sub(1);
		_shared = copy._shared;
		if (_shared)
			_shared->fetch_add(1);
		_raw_data = copy._raw_data;

		return *this;
	}

	/*===== Memory Management =====*/

	virtual void Clear()override
	{
		if (_shared)
		{
			_shared->fetch_sub(1);
			if (_raw_data && _shared->load() == 0)
			{
				if (single_data)
					delete _raw_data;
				else
					delete[] _raw_data;
				delete _shared;
			}
			_raw_data = nullptr;
			_shared = nullptr;
		}
	}

	virtual void Alloc(uint32_t nb)override
	{
		Clear();

		_raw_data = new T[nb];
		_shared = new std::atomic_uint32_t{ 1 };
	}

};

template<typename T>
using SingleSharedMemory = SharedHeapMemory<T, true>;
template<typename T>
using MultipleSharedMemory = SharedHeapMemory<T, false>;

/**
* A simple class representing a simple array of data you would need to loop through.
*/
template<typename T, bool scoped>
class LoopArray : public HeapMemory<T, scoped, false>
{
private:
	uint32_t _nb{0};

public:
    using HeapMemory<T, scoped, false>::_raw_data;
    
	/*===== Constructor =====*/

	LoopArray() = default;

	LoopArray(T* raw_data) = delete;

	LoopArray(T* raw_data, uint32_t nb):
        HeapMemory<T,scoped, false>(raw_data),
		_nb{nb}

	{

	}

	LoopArray(uint32_t nb) :
		HeapMemory<T,scoped, false>(nb),
		_nb{nb}
	{
	}

	/*===== Accessor =====*/

	__forceinline const uint32_t& Nb()const { return _nb; }

	/*===== Assignement =====*/

	T* operator=(T* raw_data) = delete;


	/*===== Memory Management =====*/

	virtual void Clear()override
	{
		HeapMemory<T, scoped, false>::Clear();
		_nb = 0;
	}

	virtual void Alloc(uint32_t nb)override
	{
		HeapMemory<T, scoped, false>::Alloc(nb);
		_nb = nb;
	}

};

template<typename T>
using VolatileLoopArray = LoopArray<T, false>;
template<typename T>
using ScopedLoopArray = LoopArray<T, true>;

/**
* A simple class representing the familiar list container.
* an uncontiguous memory container, linking node with pointer from one to the next.
*/
template<typename T>
class List
{
public:
	struct ListNode
	{
		T data;

		ListNode* prev{ nullptr };
		ListNode* next{ nullptr };

		ListNode* operator++()
		{
			return next;
		}

		ListNode* operator--()
		{
			return prev;
		}

		bool operator==(const ListNode& rhs)
		{
			return data == rhs;
		}
	};

private:
	ListNode* head{ nullptr };//first node
	ListNode* toe{ nullptr };//last node

	uint32_t	nb{ 0 };
public:

	/*===== Manipulation =====*/

	__forceinline void Add(const T& data)
	{
		ListNode* newNode = new ListNode;
		newNode->data = data;

		Add(newNode);
	}

	__forceinline void Add(ListNode* newNode)
	{
		if (head == nullptr)
		{
			head = newNode;
			toe = newNode;
		}
		else if (toe != nullptr)
		{
			toe->next = newNode;
			newNode->prev = toe;
			toe = newNode;
		}
		nb++;
	}

	__forceinline void Remove(ListNode* node, bool clearData = true)
	{
		if (node == head)
		{
			if (node->next != nullptr)
			{
				head = node->next;
				head->prev = nullptr;
			}
			else
			{
				head = nullptr;
			}

			if (clearData)
				delete node;

			nb--;
			return;
		}

		ListNode* iNode = head;
		for (uint32_t i = 0; i < nb; i++)
		{
			if (iNode == node)
			{
				if (iNode->prev != nullptr)
				{
					iNode->prev->next = iNode->next;
				}
				if (iNode->next != nullptr)
				{
					iNode->next->prev = iNode->prev;
				}

				if (clearData)
					delete iNode;

				break;
			}

			iNode++;
		}

		nb--;
	}

	/*===== Accessor =====*/

	__forceinline ListNode* GetHead()const
	{
		return head;
	}

	__forceinline ListNode* GetToe()const
	{
		return toe;
	}

	__forceinline uint32_t Nb()const
	{
		return nb;
	}

	/*===== Memory Management =====*/

	__forceinline void Clear()
	{
		if (toe == nullptr)
			return;

		//going from back
		ListNode* iNode = toe;
		do
		{
			//getting back next node to clear
			ListNode* prevNode = iNode->prev;
			//freeing current node
			delete iNode;
			//make next node to clear current node
			iNode = prevNode;
		} while (iNode != nullptr);

		head = nullptr;
		toe = nullptr;
		nb = 0;
	}
};

#define QUEUE_NODE_CAPACITY 100

/**
* A simple class representing the familiar queue container.
* This queue will work in FIFO (First-in First-Out).
* As for memory it will not be contiguous, to avoid huge copy of memory.
* This queue also expects that value will be pushed as batches (though one at a time will work too).
*/
template<typename T>
class Queue
{
public:
	struct QueueNode
	{
		T data{ nullptr };

		uint32_t offset{ 0 };
		uint32_t nb{ 0 };

		__forceinline T* GetCurrent()const
		{
			return &data[index];
		}
	};

private:

	List<QueueNode> nodes;
	uint32_t nb{ 0 };


public:
	/*===== Manipulation =====*/

	//pushing a new data. as this is a copy, the data is considered to be allocated.
	//this one exists only for polymorphism
	template<typename PolyT>
	__forceinline void Push(const PolyT& data)
	{
		//creating the node
		QueueNode newNode{};
		newNode.data = (T)(data);
		newNode.nb = 1;

		//pushing it into the list
		nodes.Add(newNode);

		nb++;
	}

	//pushing a new batch. the batch is considered to be allocated.
	template<typename PolyT>
	__forceinline void PushBatch(const PolyT& batch, uint32_t batchNb)
	{
		//creating the node
		QueueNode newNode{};
		newNode.data = (T)batch;
		newNode.nb = batchNb;

		//pushing it into the list
		nodes.Add(newNode);

		nb += batchNb;
	}

	//pushing a new batch. the batch is considered to be allocated.
	__forceinline void PushNode(const QueueNode& node)
	{
		//pushing the node into the list
		nodes.Add(node);

		nb += node.nb;
	}

	//pops a single data. is nullptr if queue is empty.
	__forceinline QueueNode Pop()
	{
		if (nb <= 0)
			return QueueNode{};

		QueueNode& headNode = nodes.GetHead()->data;
		QueueNode data = headNode;
		headNode.offset++;
		data.nb = 1;

		if (headNode.offset >= headNode.nb)
			nodes.Remove(nodes.GetHead(), true);

		nb--;

		return data;
	}

	//pops a batch. we cannot guarantee that you'll get all the requested data in the returned array, 
	//therefore the nb you get is given in return ; as such you may need to call the method multiple times
	__forceinline QueueNode PopBatch(uint32_t& wantedBatchNb)
	{
		if (nb <= 0)
		{
			wantedBatchNb = 0;
			return QueueNode{};
		}

		QueueNode& headNode = nodes.GetHead()->data;
		uint32_t remaining = headNode.nb;
		if (remaining < wantedBatchNb)
			wantedBatchNb = remaining;

		QueueNode data = headNode;
		headNode.offset += wantedBatchNb;
		headNode.nb -= wantedBatchNb;
		data.nb = wantedBatchNb;

		if (headNode.nb <= 0)
			nodes.Remove(nodes.GetHead(), true);

		nb -= wantedBatchNb;

		return data;
	}

	/*===== Accessor =====*/

	__forceinline uint32_t GetNb()const
	{
		return nb;
	}

	/*===== Memory Management =====*/

	__forceinline void Clear(bool shouldFree = true)
	{
		//no need if already empty
		if (nb <= 0)
			return;

		nodes.Clear();
		nb = 0;
	}

};

class ThreadJob
{
public:
    virtual ~ThreadJob(){}
    
	virtual void Execute() {};
};

class ThreadPool
{
private:
	std::mutex						jobs_mutex;
	Queue<ThreadJob*>				jobs;
	ScopedLoopArray<std::thread>	threads;
	std::condition_variable_any		thread_wait;

	bool killThread{ false };
	bool pause{ false };

public:

	__forceinline ~ThreadPool()
	{
		Clear();
	}

	__forceinline void ThreadLoop()
	{
		ThreadJob* job = nullptr;
		while (!killThread)
		{
			{
				jobs_mutex.lock();

				thread_wait.wait(jobs_mutex, [this]() {return (jobs.GetNb() > 0 && !pause) || killThread; });

				if (killThread)
					return;

				//gets the job
				job = jobs.Pop().data;

				//unlocks the mutex
				jobs_mutex.unlock();
			}

			if (job != nullptr)
			{
				job->Execute();
				delete job;
			}

		}
	}

	/*===== Manipulation =====*/

	template<typename Job>
	__forceinline void Add(const Job& job)
	{
		{
			//locks the mutex to try to get a job
			jobs_mutex.lock();

			//add a new job to do
			jobs.Push(new Job(job));

			//unlocks the mutex
			jobs_mutex.unlock();
		}
		thread_wait.notify_one();
	}

	template<typename Job>
	__forceinline void SilentAdd(const Job& job)
	{
		{
			//locks the mutex to try to get a job
			jobs_mutex.lock();

			//add a new job to do
			jobs.Push(new Job(job));

			//unlocks the mutex
			jobs_mutex.unlock();
		}
	}

	__forceinline void ClearJobs()
	{
		//locks the mutex 
		jobs_mutex.lock();

		//clears all pending jobs
		jobs.Clear();

		//unlocks the mutex
		jobs_mutex.unlock();
	}

	__forceinline void Pause()
	{
		//locks the mutex 
		jobs_mutex.lock();

		//sets the flag
		pause = true;

		//unlocks the mutex
		jobs_mutex.unlock();
	}

	__forceinline void Resume()
	{
		//locks the mutex 
		jobs_mutex.lock();

		//sets the flag
		pause = false;

		//unlocks the mutex
		jobs_mutex.unlock();

		//notify to resume
		thread_wait.notify_all();

	}


	/*===== Accessor =====*/

	__forceinline uint32_t GetJobsNb()const
	{
		return jobs.GetNb();
	}

	__forceinline uint32_t GetThreadsNb()const
	{
		return threads.Nb();
	}

	/*===== Memory Management =====*/

	__forceinline void MakeThreads(uint32_t threadsNb)
	{
		killThread = false;
		threads.Alloc(threadsNb);

		for (uint32_t i = 0; i < threads.Nb(); i++)
		{
			new (&threads[i]) std::thread(&ThreadPool::ThreadLoop, this);
		}
	}

	__forceinline void Clear()
	{
		{
			jobs_mutex.lock();

			//say we want them to stop
			killThread = true;

			jobs_mutex.unlock();
		}

		//let them know we want them to stop
		thread_wait.notify_all();

		//wait for the m to stop
		for (uint32_t i = 0; i < threads.Nb(); i++)
		{
			threads[i].join();
		}
		//then clear
		threads.Clear();
		jobs.Clear();
	}
};

#endif //__UTILITIES_H__
