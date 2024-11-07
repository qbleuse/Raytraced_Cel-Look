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
* A simple class representing an allocated RAM memory block, in C format.
* You may ask, why use the standard library, and things such as vector ?
* You can definetely do that, it is just personal preference.
* This uses malloc and free, as such, it should be used for simple C types and not with class.
*/
template<typename T, bool scoped = true>
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
		_raw_data{ (T*)malloc(nb * sizeof(T)) }
	{
		memset(_raw_data, 0, nb * sizeof(T));
	}

	~HeapMemory()
	{
		if (_raw_data && scoped)
			free(_raw_data);
	}

	/*===== Accessor =====*/

	__forceinline T& operator[](int32_t i)
	{
		return _raw_data[i];
	}

	__forceinline const T& operator[](int32_t i)const
	{
		return _raw_data[i];
	}

	//__forceinline T** operator&()
	//{
	//	return &_raw_data;
	//}

	//__forceinline T*const* operator&()const
	//{
	//	return &_raw_data;
	//}

	__forceinline T* operator*()const
	{
		return _raw_data;
	}

	/*===== Assignement =====*/

	T* operator=(T* raw_data)
	{
		_raw_data = raw_data;
		return _raw_data;
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

	/*===== Memory Management =====*/

	virtual void Clear()
	{
		if (_raw_data)
			free(_raw_data);
		_raw_data = nullptr;
	}

	virtual void Alloc(uint32_t nb)
	{
		_raw_data = (T*)malloc(nb * sizeof(T));
		memset(_raw_data, 0, nb * sizeof(T));
	}

};

/**
* A simple class representing an allocated RAM memory block, in C format.
* This one is for sharing between thread. therefore thread safe.
*/
template<typename T>
class SharedHeapMemory : public HeapMemory<T,true>
{
protected:
	std::atomic_uint32_t* shared;

public:

	/*===== Constructor =====*/

	SharedHeapMemory() = default;

	SharedHeapMemory(T* raw_data) :
		HeapMemory(raw_data),
		shared{ new std::atomic_uint32_t {1} }
	{

	}

	SharedHeapMemory(const SharedHeapMemory<T>& memory) :
		HeapMemory(memory._raw_data),
		shared{ memory.shared}
	{
		shared->fetch_add(1);
	}

	SharedHeapMemory(uint32_t nb) :
		HeapMemory(nb),
		shared{ new std::atomic_uint32_t {1} }
	{
		//memset(_raw_data, 0, nb * sizeof(T));
	}

	~SharedHeapMemory()
	{
		shared->load();
		shared->fetch_sub(1);
		if (_raw_data && shared->load() == 0)
		{
			free(_raw_data);
			delete shared;
		}
		_raw_data = nullptr;
	}

	/*===== Copy =====*/

	SharedHeapMemory& operator=(const SharedHeapMemory& copy)
	{
		
		shared = copy.shared;
		shared->fetch_add(1);
		_raw_data = copy._raw_data;

		return *this;
	}

	/*===== Memory Management =====*/

	//virtual void Clear() = delete;

	//virtual void Alloc(uint32_t nb)

};

/**
* A simple class representing a simple array of data you would need to loop through.
*/
template<typename T, bool scoped = true>
class LoopArray : public HeapMemory<T,scoped>
{
private:
	uint32_t _nb{0};

public:
	/*===== Constructor =====*/

	LoopArray() = default;

	LoopArray(T* raw_data) = delete;

	LoopArray(T* raw_data, uint32_t nb):
		HeapMemory<T>(raw_data),
		_nb{nb}

	{

	}

	LoopArray(uint32_t nb) :
		HeapMemory<T>(nb),
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
		HeapMemory<T>::Clear();
		_nb = 0;
	}

	virtual void Alloc(uint32_t nb)override
	{
		HeapMemory<T>::Alloc(nb);
		_nb = nb;
	}

};

/*
* A simple class representing an allocated RAM memory block, in cpp.
* in conbtrast with the parent, this uses new and delete, thus preserving vtables for contained classes.
*/
template<typename T, bool scoped = true>
class SmartHeapMemory : public HeapMemory<T, scoped>
{
public:
	/*===== Constructor =====*/

	SmartHeapMemory() = default;

	SmartHeapMemory(T* raw_data) :
		HeapMemory<T>{ raw_data }
	{
	}

	SmartHeapMemory(uint32_t nb) :
		_raw_data{ new T[nb]; }
	{
	}

	~SmartHeapMemory()
	{
		if (_raw_data && scoped)
			delete[] _raw_data;
	}

	/*===== Memory Management =====*/

	virtual void Clear()override
	{
		if (_raw_data)
			delete[] _raw_data;
		_raw_data = nullptr;
	}

	virtual void Alloc(uint32_t nb)override
	{
		_raw_data = new T[nb];
	}
};

/**
* A simple class representing a simple array of data you would need to loop through.
* as being smart, this preserve classes vtable when allocating.
*/
template<typename T, bool scoped = true>
class SmartLoopArray : public SmartHeapMemory<T,scoped>
{
private:
	uint32_t _nb{ 0 };

public:
	/*===== Constructor =====*/

	SmartLoopArray() = default;

	SmartLoopArray(T* raw_data) = delete;

	SmartLoopArray(T* raw_data, uint32_t nb) :
		SmartHeapMemory<T>(raw_data),
		_nb{ nb }

	{

	}

	SmartLoopArray(uint32_t nb) :
		SmartHeapMemory<T>(nb),
		_nb{ nb }
	{
	}

	/*===== Accessor =====*/

	__forceinline const uint32_t& Nb()const { return _nb; }

	/*===== Assignement =====*/

	T* operator=(T* raw_data) = delete;


	/*===== Memory Management =====*/

	virtual void Clear()override
	{
		SmartHeapMemory<T>::Clear();
		_nb = 0;
	}

	virtual void Alloc(uint32_t nb)override
	{
		SmartHeapMemory<T>::Alloc(nb);
		_nb = nb;
	}

};

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

		ListNode* prev{nullptr};
		ListNode* next{nullptr};

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
	ListNode*	head{nullptr};//first node
	ListNode*	toe{ nullptr };//last node

	uint32_t	nb{0};
public:

	/*===== Manipulation =====*/

	__forceinline void Add(const T& data)
	{
		ListNode* newNode = static_cast<ListNode*>(malloc(sizeof(ListNode)));
		memset(newNode, 0, sizeof(ListNode));
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
			}
			else
			{
				head = nullptr;
			}

			if (clearData)
				free(head);

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
					free(&iNode);

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

	__forceinline uint32_t GetNb()const
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
			free(iNode);
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
		QueueNode() = default;
		QueueNode(const QueueNode&) = default;

		SharedHeapMemory<T> data{ nullptr };

		uint32_t index{ 0 };
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

	//pushing a new data. as this is a copy, the data is considered to not being yet allocated.
	__forceinline void Push(const T& data)
	{
		//creating the node
		QueueNode newNode{};
		newNode.data = (T*)malloc(sizeof(T));
		*newNode.data = data;
		newNode.nb = 1;

		//pushing it into the list
		nodes.Add(newNode);

		nb++;
	}

	//pushing a new batch. the batch is considered to be allocated.
	__forceinline void PushBatch(const SharedHeapMemory<T>& batch, uint32_t batchNb)
	{
		//creating the node
		QueueNode newNode{};
		newNode.data = batch;
		newNode.nb = batchNb;

		//pushing it into the list
		nodes.Add(newNode);

		nb += batchNb;
	}


	//pops a single data. is nullptr if queue is empty.
	__forceinline QueueNode Pop()
	{
		if (nb <= 0)
			return QueueNode{};

		QueueNode& headNode = nodes.GetHead()->data;
		QueueNode data = headNode;
		headNode.index++;
		data.nb = 1;

		if (headNode.index >= headNode.nb)
			nodes.Remove(headNode, false);

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
		uint32_t remaining = (headNode.nb - headNode.index);
		if (remaining < wantedBatchNb)
			wantedBatchNb = remaining;

		QueueNode data = headNode;
		headNode.index += wantedBatchNb;
		data.nb = wantedBatchNb;

		if (headNode.index >= headNode.nb)
			nodes.Remove(nodes.GetHead(), false);

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

		//if (shouldFree)
		//{
		//	//if we're not empty, there is a head
		//	List<QueueNode>::ListNode* indexedNode = nodes.GetHead();
		//
		//	while (indexedNode != nullptr)
		//	{
		//		free(indexedNode->data.data);
		//		indexedNode = indexedNode->next;
		//	}
		//}

		nodes.Clear();
		nb = 0;
	}

};

/**
* A simple class representing the familiar queue container.
* This queue will work in FIFO (First-in First-Out).
* As for memory it will not be contiguous, to avoid huge copy of memory.
* This queue also expects that value will be pushed as batches (though one at a time will work too).
* It is smart, as such it uses "new" operator rather the malloc.
*/
template<typename T>
class SmartQueue
{
private:
	struct QueueNode
	{
		T* data{ nullptr };

		uint32_t index{ 0 };
		uint32_t nb{ 0 };

		__forceinline T* GetCurrent()const
		{
			return &data[index];
		}
	};

	List<QueueNode> nodes;
	uint32_t nb{ 0 };


public:
	/*===== Manipulation =====*/

	//pushing a new data. as this is a copy, the data is considered to not being yet allocated.
	__forceinline void Push(const T& data)
	{
		//creating the node
		QueueNode newNode{};
		newNode.data = new T(data);
		newNode.nb = 1;

		//pushing it into the list
		nodes.Add(newNode);

		nb++;
	}

	//pushing a new data. as this is a copy, the data is considered to not being yet allocated.
	//this one exists only for polymorphism
	template<typename PolyT>
	__forceinline void Push(const PolyT& data)
	{
		//creating the node
		QueueNode newNode{};
		newNode.data = (T*)(new PolyT(data));
		newNode.nb = 1;

		//pushing it into the list
		nodes.Add(newNode);

		nb++;
	}

	//pushing a new batch. the batch is considered to be allocated.
	__forceinline void PushBatch(T* batch, uint32_t batchNb)
	{
		//creating the node
		QueueNode newNode{};
		newNode.data = batch;
		newNode.nb = batchNb;

		//pushing it into the list
		nodes.Add(newNode);

		nb += batchNb;
	}

	//pops a single data. is nullptr if queue is empty.
	__forceinline T* Pop()
	{
		if (nb <= 0)
			return nullptr;

		QueueNode& headNode = nodes.GetHead()->data;
		T* data = headNode.GetCurrent();
		headNode.index++;

		if (headNode.index >= headNode.nb)
			nodes.Remove(nodes.GetHead(), false);

		nb--;

		return data;
	}

	//pops a batch. we cannot guarantee that you'll get all the requested data in the returned array, 
	//therefore the nb you get is given in return ; as such you may need to call the method multiple times
	__forceinline T* PopBatch(uint32_t& wantedBatchNb)
	{
		if (nb <= 0)
		{
			wantedBatchNb = 0;
			return nullptr;
		}

		QueueNode& headNode = nodes.GetHead()->data;
		uint32_t remaining = (headNode.nb - headNode.index);
		if (remaining < wantedBatchNb)
			wantedBatchNb = remaining;

		T* data = headNode.GetCurrent();
		headNode.index += wantedBatchNb;

		if (headNode.index >= headNode.nb)
			nodes.Remove(nodes.GetHead(), false);

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

		if (shouldFree)
		{
			//if we're not empty, there is a head
			List<QueueNode>::ListNode* indexedNode = nodes.GetHead();

			while (indexedNode != nullptr)
			{
				delete indexedNode->data.data;
				indexedNode = indexedNode->next;
			}
		}

		nodes.Clear();
		nb = 0;
	}

};

class ThreadJob
{
public:
	virtual void Execute() = 0;
};

class ThreadPool
{
private:
	std::mutex					jobs_mutex;
	SmartQueue<ThreadJob>		jobs;
	LoopArray<std::thread>		threads;
	std::condition_variable_any	thread_wait;

	bool killThread{ false };

public:

	__forceinline void ThreadLoop()
	{
		ThreadJob* job = nullptr;
		while (!killThread)
		{
			{
				//locks the mutex to try to get a job
				jobs_mutex.lock();

				//waits for a job to be available
				thread_wait.wait(jobs_mutex, [this]() {return jobs.GetNb() > 0 || killThread; });

				//gets the job
				job = jobs.Pop();

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
			jobs.Push(job);

			//unlocks the mutex
			jobs_mutex.unlock();
		}
		thread_wait.notify_one();
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
			threads[i] = std::thread(&ThreadPool::ThreadLoop, this);
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
